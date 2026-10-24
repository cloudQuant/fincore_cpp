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
import json
import os
import sys
import time
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd

BENCHMARK_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DATA = BENCHMARK_ROOT / "data" / "a_stocks"
DEFAULT_OUT = BENCHMARK_ROOT / "results" / "python"
TIMEZONE = "Asia/Shanghai"
FACTOR = "alpha003"
WINDOW = 10


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
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))
    import fincore  # noqa: F401
    from fincore.factor_analysis.research import evaluate_factor_research
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


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True, default=str) + "\n",
                    encoding="utf-8", newline="\n")


def main() -> int:
    args = parse_args()
    periods = tuple(int(value) for value in args.periods.split(",") if value)
    fincore, evaluate_factor_research = import_fincore(args.fincore_root)

    started = time.perf_counter()
    grid, open_panel, close_panel, volume_panel, files = load_panel(
        args.data_dir, args.max_symbols)
    load_seconds = time.perf_counter() - started

    started = time.perf_counter()
    factor_long, _ = compute_alpha003(open_panel, volume_panel)
    factor_seconds = time.perf_counter() - started

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

    started = time.perf_counter()
    result = evaluate_factor_research(
        factor_frame,
        grid=grid,
        prices=prices,
        universe=None,
        groups=None,
        precomputed_returns=None,
        spec=spec,
    )
    research_seconds = time.perf_counter() - started

    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    factor_out = pd.DataFrame({
        "date": factor_long["time"].dt.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "asset": factor_long["asset"],
        FACTOR: factor_long[FACTOR],
    })
    factor_out.to_csv(out / "factor_values.csv", index=False, encoding="utf-8")
    table_names = (
        "signals", "labels", "ic", "ic_summary", "quantile_returns",
        "turnover", "spreads", "stability", "coverage", "diagnostics",
    )
    for name in table_names:
        result.table(name).to_csv(out / f"{name}.csv", index=False,
                                  encoding="utf-8")
    export_seconds = time.perf_counter() - started

    timings = {
        "load_seconds": load_seconds,
        "factor_seconds": factor_seconds,
        "research_seconds": research_seconds,
        "export_seconds": export_seconds,
        "total_seconds": load_seconds + factor_seconds + research_seconds + export_seconds,
    }
    run_info = {
        "run_id": args.run_id,
        "engine": "python-fincore",
        "fincore_version": getattr(fincore, "__version__", "unknown"),
        "python": sys.version.split()[0],
        "pandas": pd.__version__,
        "numpy": np.__version__,
        "data_dir": str(args.data_dir.resolve()),
        "selected_files": len(files),
        "assets": len(targets),
        "grid_dates": len(grid),
        "end_offset": args.end_offset,
        "research_end": research_end.isoformat(),
        "factor_cells": len(factor_long),
        "periods": list(periods),
        "quantiles": args.quantiles,
        "label": args.label,
        "preprocess": args.preprocess,
        "window": WINDOW,
        "status": result.status,
        "timings": timings,
    }
    write_json(out / "timings.json", timings)
    write_json(out / "run_info.json", run_info)
    write_json(out / "manifest.json", dict(result.manifest))
    print(json.dumps(run_info, sort_keys=True, default=str))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
