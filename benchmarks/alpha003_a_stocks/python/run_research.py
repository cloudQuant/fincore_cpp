#!/usr/bin/env python3
"""Alpha003 A-share factor research through the Python fincore package.

The factor itself follows the Alpha101 alpha003 contract used by the
backtrader reference: cross-sectional average rank divided by the number of
eligible assets, a complete 10-bar Pearson correlation, then negation.

This runner is intentionally independent from the C++ runner: it parses the
same raw per-symbol CSVs and calls fincore's public research API. The compare
script checks both the factor panel and the research tables.
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import sys
import time
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd

BENCHMARK_ROOT = Path(__file__).resolve().parents[1]
PYTHON_DIR = Path(__file__).resolve().parent
REPO_ROOT = BENCHMARK_ROOT.parents[1]
for _module_path in (str(REPO_ROOT), str(BENCHMARK_ROOT), str(PYTHON_DIR)):
    if _module_path not in sys.path:
        sys.path.insert(0, _module_path)

from benchmarks.alpha003_a_stocks.contract import TABLES as TABLE_CONTRACTS
from benchmarks.alpha003_a_stocks.html_report import write_report

DEFAULT_DATA = BENCHMARK_ROOT / "data" / "a_stocks"
DEFAULT_OUT = BENCHMARK_ROOT / "results" / "python"
TIMEZONE = "Asia/Shanghai"
FACTOR = "alpha003"
WINDOW = 10
THREAD_LIMIT_ENV_NAMES = (
    "OMP_NUM_THREADS",
    "OPENBLAS_NUM_THREADS",
    "MKL_NUM_THREADS",
    "NUMEXPR_NUM_THREADS",
    "VECLIB_MAXIMUM_THREADS",
    "BLIS_NUM_THREADS",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=DEFAULT_DATA)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--max-symbols", type=int, default=500,
                        help="uniformly sample this many files; 0 means all")
    parser.add_argument("--periods", default="1,5,20")
    parser.add_argument("--quantiles", type=int, default=5)
    parser.add_argument("--min-assets", type=int, default=10)
    parser.add_argument("--label", default="next_open_to_open")
    parser.add_argument("--preprocess", default="cross_section_rank")
    parser.add_argument("--fincore-root", type=Path,
                        default=Path(os.environ.get("FINCORE_ROOT", r"D:\source_code\fincore")))
    parser.add_argument("--end-offset", type=int, default=22,
                        help="reserve trailing grid rows so all period labels exist")
    parser.add_argument("--run-id", default="run1")
    return parser.parse_args()


def import_fincore(root: Path) -> Any:
    root = root.resolve()
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))
    import fincore  # noqa: F401
    from fincore.factor_analysis.research import evaluate_factor_research
    expected = root / "fincore" / "factor_analysis" / "research.py"
    if (Path(fincore.__file__).resolve() != root / "fincore" / "__init__.py"
            or Path(evaluate_factor_research.__code__.co_filename).resolve() != expected):
        raise RuntimeError("loaded fincore sources do not match --fincore-root")
    return fincore, evaluate_factor_research


def symbol_files(data_dir: Path, limit: int) -> list[Path]:
    files = sorted(path for path in data_dir.glob("*.csv") if path.is_file())
    if not files:
        raise FileNotFoundError(f"no stock CSVs under {data_dir}")
    if limit < 0:
        raise ValueError("--max-symbols must be non-negative")
    if limit and limit < len(files):
        count = len(files)
        files = [files[index * count // limit] for index in range(limit)]
    return files


def utc_series(values: pd.Series) -> pd.DatetimeIndex:
    return pd.DatetimeIndex(
        pd.to_datetime(values).dt.tz_localize(TIMEZONE).dt.tz_convert("UTC")
    )


def load_panel(data_dir: Path, limit: int) -> tuple[
        pd.DataFrame, pd.DataFrame, pd.DataFrame, pd.DataFrame, list[Path]]:
    calendar = pd.read_csv(data_dir / "_metadata" / "observed_calendar.csv")
    close_times = utc_series(calendar["last_trade_time"])
    open_times = utc_series(calendar["first_trade_time"])
    if not close_times.is_monotonic_increasing or close_times.has_duplicates:
        raise ValueError("observed calendar close times must be sorted and unique")
    grid = pd.DataFrame(
        {"open_time": open_times.to_numpy()},
        index=pd.DatetimeIndex(close_times, name="close_time"),
    )

    files = symbol_files(data_dir, limit)
    opens: dict[str, pd.Series] = {}
    closes: dict[str, pd.Series] = {}
    volumes: dict[str, pd.Series] = {}
    for path in files:
        frame = pd.read_csv(path)
        missing = {"trade_time", "open", "close", "volume"} - set(frame.columns)
        if missing:
            raise ValueError(f"{path.name} is missing columns {sorted(missing)}")
        index = utc_series(frame["trade_time"])
        if index.has_duplicates:
            raise ValueError(f"{path.name} has duplicate trade times")
        frame = frame.set_index(index).reindex(grid.index)
        symbol = path.stem
        opens[symbol] = frame["open"].astype(float)
        closes[symbol] = frame["close"].astype(float)
        volumes[symbol] = frame["volume"].astype(float)

    columns = [path.stem for path in files]
    open_panel = pd.DataFrame(opens).reindex(index=grid.index, columns=columns)
    close_panel = pd.DataFrame(closes).reindex(index=grid.index, columns=columns)
    volume_panel = pd.DataFrame(volumes).reindex(index=grid.index, columns=columns)
    return grid, open_panel, close_panel, volume_panel, files


def _stable_correlation(left: np.ndarray, right: np.ndarray) -> np.ndarray:
    """Vectorized alpha003 reference kernel.

    Anchor-center each series, scale its differences and centered deviations,
    then take the Pearson ratio of the unit-normalized windows. The scaling
    factors cancel in the ratio; this avoids the catastrophic cancellation that
    makes a naive rolling variance return infinities for near-constant ranks.
    """
    def unit_deviations(windows: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        anchor = windows[..., :1]
        differences = windows - anchor
        difference_scale = np.max(np.abs(differences), axis=-1)
        valid = np.isfinite(difference_scale) & (difference_scale > 0.0)
        safe_scale = np.where(valid, difference_scale, 1.0)
        normalized = differences / safe_scale[..., None]
        normalized = normalized - normalized.mean(axis=-1, keepdims=True)
        centered_scale = np.max(np.abs(normalized), axis=-1)
        valid = valid & np.isfinite(centered_scale) & (centered_scale > 0.0)
        safe_centered = np.where(valid, centered_scale, 1.0)
        return normalized / safe_centered[..., None], valid

    with np.errstate(divide="ignore", invalid="ignore", over="ignore"):
        left_unit, left_valid = unit_deviations(left)
        right_unit, right_valid = unit_deviations(right)
        valid = left_valid & right_valid
        numerator = np.sum(left_unit * right_unit, axis=-1)
        denominator = (np.sqrt(np.sum(left_unit * left_unit, axis=-1)) *
                       np.sqrt(np.sum(right_unit * right_unit, axis=-1)))
        valid = valid & np.isfinite(numerator) & np.isfinite(denominator) & (denominator > 0.0)
        result = np.full_like(numerator, np.nan)
        np.divide(numerator, denominator, out=result, where=valid)
        epsilon = 4.0 * np.finfo(np.float64).eps
        result = np.where((result > 1.0) & (result <= 1.0 + epsilon), 1.0, result)
        result = np.where((result < -1.0) & (result >= -1.0 - epsilon), -1.0, result)
        result = np.where((result > 1.0) | (result < -1.0), np.nan, result)
    return result


def compute_alpha003(open_panel: pd.DataFrame,
                     volume_panel: pd.DataFrame) -> tuple[pd.DataFrame, np.ndarray]:
    eligible = open_panel.notna() & volume_panel.notna()
    open_values = open_panel.where(eligible)
    volume_values = volume_panel.where(eligible)
    rank_open = open_values.rank(axis=1, method="average", pct=True,
                                 na_option="keep")
    rank_volume = volume_values.rank(axis=1, method="average", pct=True,
                                     na_option="keep")
    windows_open = np.lib.stride_tricks.sliding_window_view(
        rank_open.to_numpy(dtype=float), WINDOW, axis=0)
    windows_volume = np.lib.stride_tricks.sliding_window_view(
        rank_volume.to_numpy(dtype=float), WINDOW, axis=0)
    correlation = _stable_correlation(windows_open, windows_volume)
    alpha_values = np.full((len(open_panel), len(open_panel.columns)), np.nan)
    alpha_values[WINDOW - 1:] = -correlation

    date_positions, asset_positions = np.nonzero(np.isfinite(alpha_values))
    long = pd.DataFrame({
        "time": pd.DatetimeIndex(open_panel.index)[date_positions],
        "asset": np.asarray(open_panel.columns, dtype=object)[asset_positions],
        FACTOR: alpha_values[date_positions, asset_positions],
    })
    long["time"] = pd.DatetimeIndex(long["time"]).tz_convert("UTC")
    long = long.sort_values(["time", "asset"], kind="mergesort").reset_index(drop=True)
    return long, alpha_values


def write_json(path: Path, payload: Any, *, require_finite: bool = False) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True, default=str,
                               allow_nan=not require_finite) + "\n",
                    encoding="utf-8", newline="\n")


def validate_utc_seconds(frame: pd.DataFrame) -> None:
    """Validate timestamp arrays before pandas formats them without offsets."""
    for column in ("time", "date"):
        if column not in frame.columns:
            continue
        values = frame[column]
        if values.empty:
            continue
        if not pd.api.types.is_datetime64_any_dtype(values.dtype):
            raise ValueError(f"{column} must remain a UTC datetime column")
        if str(values.dt.tz) != "UTC":
            raise ValueError(f"{column} timestamps must be UTC")
        if np.any(values.array.asi8 % 1_000_000_000 != 0):
            raise ValueError(f"{column} timestamps must have whole-second precision")


def write_csv_frame(frame: pd.DataFrame, path: Path) -> None:
    """Write a complete table with canonical UTC text and round-trip floats."""
    validate_utc_seconds(frame)
    with path.open("w", encoding="utf-8", newline="") as stream:
        frame.to_csv(stream, index=False, date_format="%Y-%m-%dT%H:%M:%SZ",
                     lineterminator="\n", na_rep="")


def canonical_utc_text(value: pd.Timestamp) -> str:
    timestamp = pd.Timestamp(value)
    if timestamp.tzinfo is None:
        raise ValueError("workload timestamps must be timezone-aware")
    timestamp = timestamp.tz_convert("UTC")
    if timestamp.value % 1_000_000_000 != 0:
        raise ValueError("workload timestamps must have whole-second precision")
    return timestamp.strftime("%Y-%m-%dT%H:%M:%SZ")


def workload_snapshot(*, periods: tuple[int, ...], args: argparse.Namespace,
                      grid: pd.DataFrame, research_end: pd.Timestamp,
                      assets: int, factor_cells: int) -> dict[str, Any]:
    return {
        "contract": "alpha003-html-v1",
        "factor": FACTOR,
        "periods": [f"{period}D" for period in periods],
        "quantiles": args.quantiles,
        "min_assets": args.min_assets,
        "min_quantile_assets": 1,
        "preprocess": args.preprocess,
        "label": args.label,
        "direction": 1,
        "stability_lags": [1, 5, 20],
        "start": canonical_utc_text(grid.index[0]),
        "research_end": canonical_utc_text(research_end),
        "grid_dates": len(grid),
        "assets": assets,
        "factor_cells": factor_cells,
        "label_config": {
            "snapshot_id": "a_stocks_2025",
            "price_basis": "unadjusted_open",
            "frequency": "1D",
            "calendar_id": "observed_calendar",
            "feed_time_kind": "close_time",
            "source_timezone": TIMEZONE,
        },
        "serialization": {
            "format": "single-file-html",
            "encoding": "utf-8",
            "line_ending": "LF",
            "timestamp": "ISO8601-UTC-Z",
            "float": "roundtrip-binary64",
            "nan": "null",
        },
    }


def _report_cell(value: Any, column: str) -> Any:
    """Convert a scalar to strict JSON while preserving real strings."""
    if value is None or value is pd.NA or value is pd.NaT:
        return None
    if isinstance(value, (pd.Timestamp, np.datetime64)):
        return canonical_utc_text(pd.Timestamp(value))
    if isinstance(value, (float, np.floating)):
        number = float(value)
        if np.isnan(number):
            return None
        if not np.isfinite(number):
            raise ValueError(f"{column} contains an infinite value")
        return number
    if isinstance(value, (bool, np.bool_)):
        return bool(value)
    if isinstance(value, (int, np.integer)):
        return int(value)
    if isinstance(value, str):
        return value

    missing = pd.isna(value)
    if isinstance(missing, (bool, np.bool_)) and missing:
        return None
    if hasattr(value, "item"):
        return _report_cell(value.item(), column)
    raise TypeError(f"{column} contains an unsupported report value: {type(value).__name__}")


def report_table(frame: pd.DataFrame, name: str) -> dict[str, Any]:
    """Materialize one bounded research table using the shared column contract."""
    if name not in TABLE_CONTRACTS:
        raise KeyError(f"unknown research table: {name}")
    columns = list(TABLE_CONTRACTS[name].columns)
    if list(frame.columns) != columns:
        raise RuntimeError(
            f"fincore {name} columns differ from the registered contract: "
            f"expected {columns}, got {list(frame.columns)}")
    rows = [
        [_report_cell(value, column)
         for column, value in zip(columns, row)]
        for row in frame.itertuples(index=False, name=None)
    ]
    return {"columns": columns, "rows": rows}


def peak_memory_observation() -> dict[str, Any]:
    """Return process peak resident memory in bytes when the OS reports it."""
    if os.name == "nt":
        class ProcessMemoryCounters(ctypes.Structure):
            _fields_ = [
                ("cb", ctypes.c_uint32), ("page_faults", ctypes.c_uint32),
                ("peak_working_set", ctypes.c_size_t),
                ("working_set", ctypes.c_size_t),
                ("quota_peak_paged", ctypes.c_size_t),
                ("quota_paged", ctypes.c_size_t),
                ("quota_peak_nonpaged", ctypes.c_size_t),
                ("quota_nonpaged", ctypes.c_size_t),
                ("pagefile", ctypes.c_size_t),
                ("peak_pagefile", ctypes.c_size_t),
            ]
        try:
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            psapi = ctypes.WinDLL("psapi", use_last_error=True)
            kernel32.GetCurrentProcess.restype = ctypes.c_void_p
            query = psapi.GetProcessMemoryInfo
            query.argtypes = [ctypes.c_void_p,
                              ctypes.POINTER(ProcessMemoryCounters),
                              ctypes.c_uint32]
            query.restype = ctypes.c_int
            counters = ProcessMemoryCounters()
            counters.cb = ctypes.sizeof(counters)
            okay = query(kernel32.GetCurrentProcess(), ctypes.byref(counters),
                         counters.cb)
            peak = int(counters.peak_working_set) if okay else None
            status = "available" if okay else "query_failed"
        except (AttributeError, OSError):
            peak, status = None, "api_unavailable"
        return {"peak_memory_bytes": peak,
                "peak_memory_source": "GetProcessMemoryInfo.PeakWorkingSetSize",
                "peak_memory_status": status}

    try:
        import resource
        raw_peak = int(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
    except (ImportError, AttributeError, OSError, ValueError):
        raw_peak = -1
    if raw_peak < 0:
        peak, status = None, "unavailable_or_invalid"
    elif sys.platform == "darwin":
        peak, status = raw_peak, "available"
    elif sys.platform.startswith(("linux", "freebsd")):
        peak, status = raw_peak * 1024, "available"
    else:
        peak, status = None, "unit_unknown_for_platform"
    return {"peak_memory_bytes": peak,
            "peak_memory_source": "resource.getrusage.ru_maxrss",
            "peak_memory_status": status}


def process_affinity_observation() -> dict[str, Any]:
    """Report the process's observed CPU affinity without changing it."""
    if os.name == "nt":
        try:
            mask_type = ctypes.c_size_t
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel32.GetCurrentProcess.restype = ctypes.c_void_p
            query = kernel32.GetProcessAffinityMask
            query.argtypes = [ctypes.c_void_p, ctypes.POINTER(mask_type),
                              ctypes.POINTER(mask_type)]
            query.restype = ctypes.c_int
            process_mask = mask_type()
            system_mask = mask_type()
            if query(kernel32.GetCurrentProcess(), ctypes.byref(process_mask),
                     ctypes.byref(system_mask)):
                mask = int(process_mask.value)
                ids = [index for index in range(ctypes.sizeof(mask_type) * 8)
                       if mask & (1 << index)]
                return {"affinity_cpu_ids": ids,
                        "affinity_mask_hex": hex(mask),
                        "affinity_source": "GetProcessAffinityMask",
                        "affinity_status": "available"}
            status = "query_failed"
        except (AttributeError, OSError):
            status = "api_unavailable"
        return {"affinity_cpu_ids": None, "affinity_mask_hex": None,
                "affinity_source": "GetProcessAffinityMask",
                "affinity_status": status}

    if hasattr(os, "sched_getaffinity"):
        try:
            ids = sorted(int(cpu) for cpu in os.sched_getaffinity(0))
            return {"affinity_cpu_ids": ids, "affinity_mask_hex": None,
                    "affinity_source": "os.sched_getaffinity",
                    "affinity_status": "available"}
        except (OSError, ValueError):
            status = "query_failed"
    else:
        status = "api_unavailable"
    return {"affinity_cpu_ids": None, "affinity_mask_hex": None,
            "affinity_source": "os.sched_getaffinity",
            "affinity_status": status}


def thread_limit_environment() -> dict[str, str | None]:
    """Capture configured thread-limit variables; this does not verify pools."""
    return {name: os.environ.get(name) for name in THREAD_LIMIT_ENV_NAMES}


def main() -> int:
    internal_started = time.perf_counter()
    cpu_started = time.process_time()
    args = parse_args()
    out = args.out.resolve()
    if out.exists():
        if not out.is_dir():
            raise NotADirectoryError(f"--out must be a directory: {out}")
        if any(out.iterdir()):
            raise FileExistsError(
                f"--out must be empty so the run contains only research.html: {out}")
    else:
        out.mkdir(parents=True)
    periods = tuple(int(value) for value in args.periods.split(",") if value)
    fincore, evaluate_factor_research = import_fincore(args.fincore_root)

    load_started = time.perf_counter()
    preparation_seconds = load_started - internal_started
    grid, open_panel, close_panel, volume_panel, files = load_panel(
        args.data_dir, args.max_symbols)
    for timestamps, name in ((grid.index, "calendar close_time"),
                             (grid["open_time"], "calendar first_trade_time")):
        if np.any(pd.DatetimeIndex(timestamps).asi8 % 1_000_000_000 != 0):
            raise ValueError(f"{name} timestamps must have whole-second precision")
    load_seconds = time.perf_counter() - load_started

    factor_started = time.perf_counter()
    factor_long, _ = compute_alpha003(open_panel, volume_panel)
    factor_seconds = time.perf_counter() - factor_started
    materialization_started = time.perf_counter()

    if args.end_offset < 0 or args.end_offset >= len(grid) - 1:
        raise ValueError("--end-offset must leave at least two grid dates")
    research_end = grid.index[-1 - args.end_offset]

    in_window = factor_long[(factor_long["time"] >= grid.index[0]) &
                            (factor_long["time"] <= research_end)]
    factor_assets = set(in_window["asset"].unique())
    targets = [symbol for symbol in open_panel.columns if symbol in factor_assets]
    if not targets:
        raise RuntimeError("no asset produced a finite alpha003 value")

    factor_frame = factor_long.set_index(["time", "asset"])[[FACTOR]]
    factor_frame.attrs["factor_directions"] = {FACTOR: 1}
    prices = {
        "open": open_panel[targets],
        "close": close_panel[targets],
    }
    spec = {
        "targets": tuple(targets),
        "start": grid.index[0],
        "end": research_end,
        "periods": periods,
        "label": args.label,
        "preprocess": args.preprocess,
        "quantiles": args.quantiles,
        "min_assets": args.min_assets,
        "min_quantile_assets": 1,
        "max_loss": 1.0,
        "stability_lags": (1, 5, 20),
        "snapshot_id": "a_stocks_2025",
        "price_basis": "unadjusted_open",
        "calendar_id": "observed_calendar",
        "frequency": "1D",
        "source_timezone": TIMEZONE,
        "feed_time_kind": "close_time",
    }

    research_started = time.perf_counter()
    materialization_seconds = research_started - materialization_started
    result = evaluate_factor_research(
        factor_frame,
        grid=grid,
        prices=prices,
        universe=None,
        groups=None,
        precomputed_returns=None,
        spec=spec,
    )
    research_seconds = time.perf_counter() - research_started

    export_started = time.perf_counter()
    validation_table_names = (
        "signals", "labels", "ic", "ic_summary", "quantile_returns",
        "turnover", "spreads", "stability", "coverage", "diagnostics",
    )
    report_table_names = (
        "ic", "ic_summary", "quantile_returns", "turnover", "spreads",
        "stability", "coverage", "diagnostics",
    )
    manifest = dict(result.manifest)
    manifest_tables = manifest.get("tables")
    if not isinstance(manifest_tables, dict):
        raise RuntimeError("fincore result manifest is missing table metadata")
    tables: dict[str, dict[str, Any]] = {}
    table_row_counts: dict[str, int] = {}
    for name in validation_table_names:
        table = result.table(name)
        descriptor = manifest_tables.get(name)
        expected = {"rows": len(table),
                    "columns": [str(column) for column in table.columns]}
        if (not isinstance(descriptor, dict) or
                descriptor.get("rows") != expected["rows"] or
                descriptor.get("columns") != expected["columns"]):
            raise RuntimeError(
                f"fincore manifest metadata differs from exported {name} table")
        table_row_counts[name] = len(table)
        if name in report_table_names:
            tables[name] = report_table(table, name)

    workload = workload_snapshot(
        periods=periods, args=args, grid=grid, research_end=research_end,
        assets=len(targets), factor_cells=len(factor_long))
    export_seconds = time.perf_counter() - export_started

    internal_wall_seconds = time.perf_counter() - internal_started
    cpu_seconds = time.process_time() - cpu_started
    peak_memory = peak_memory_observation()
    measured_stage_seconds = (preparation_seconds + materialization_seconds + load_seconds +
                              factor_seconds + research_seconds +
                              export_seconds)
    other_seconds = internal_wall_seconds - measured_stage_seconds

    timings = {
        "preparation_seconds": preparation_seconds,
        "materialization_seconds": materialization_seconds,
        "load_seconds": load_seconds,
        "factor_seconds": factor_seconds,
        "research_seconds": research_seconds,
        "export_seconds": export_seconds,
        "other_seconds": other_seconds,
        "total_seconds": internal_wall_seconds,
        "internal_wall_seconds": internal_wall_seconds,
        "accounted_stage_seconds": measured_stage_seconds + other_seconds,
        "cpu_seconds": cpu_seconds,
        "internal_process_cpu_seconds": cpu_seconds,
        "cpu_measurement_source": "time.process_time (user + system)",
        "html_render_included": False,
        **peak_memory,
    }
    run_info = {
        "run_id": args.run_id,
        "engine": "python-fincore",
        "fincore_version": getattr(fincore, "__version__", "unknown"),
        "loaded_fincore_sources": [
            {"path": str(Path(path).resolve()),
             "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest()}
            for path in (fincore.__file__, evaluate_factor_research.__code__.co_filename)
        ],
        "python": sys.version.split()[0],
        "pandas": pd.__version__,
        "numpy": np.__version__,
        "data_dir": str(args.data_dir.resolve()),
        "selected_files": len(files),
        "assets": len(targets),
        "grid_dates": len(grid),
        "end_offset": args.end_offset,
        "research_end": canonical_utc_text(research_end),
        "factor_cells": len(factor_long),
        "table_rows": table_row_counts,
        "periods": list(periods),
        "quantiles": args.quantiles,
        "label": args.label,
        "preprocess": args.preprocess,
        "window": WINDOW,
        "status": result.status,
        "measurement_scope": {
            "internal_wall":
                "main entry through factor/research and in-memory report table/workload construction; measured before run_info/payload assembly and HTML rendering/file close; excludes interpreter startup and module-level NumPy/pandas imports",
            "process_wall":
                "measured externally from process launch through complete HTML file close and process exit; this is the only benchmark comparison metric",
            "cpu": "process CPU delta over the internal runner interval, ending before HTML rendering",
            "peak_memory":
                "process-lifetime peak resident working set observed before HTML rendering",
            "html_render":
                "excluded from internal stage timings because those values are embedded in the HTML before final serialization; included in external process wall",
            "affinity_and_thread_limits":
                "observed only; this runner does not apply affinity or thread limits",
        },
        "environment": {
            "affinity": process_affinity_observation(),
            "thread_limit_environment": thread_limit_environment(),
            "effective_thread_pools_verified": False,
        },
        "timings": timings,
    }
    payload = {
        "schema_version": "alpha003-html-v1",
        "engine": "python-fincore",
        "workload": workload,
        "run_info": run_info,
        "timings": timings,
        "tables": tables,
    }
    output_path = out / "research.html"
    if output_path.exists():
        raise FileExistsError(f"refusing to replace an existing report: {output_path}")
    write_report(output_path, payload)
    print(json.dumps(run_info, sort_keys=True, default=str, allow_nan=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
