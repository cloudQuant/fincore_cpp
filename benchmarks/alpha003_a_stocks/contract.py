"""Complete, ordered alpha003 research CSV contract (iteration 9, v2).

Comparison is streamed and runs outside every timed interval. No join, dropna,
aggregation or tolerance expansion is used to conceal missing observations.
"""
from __future__ import annotations

from dataclasses import dataclass
from itertools import zip_longest
import json
from pathlib import Path

import numpy as np
import pandas as pd


CONTRACT_VERSION = "alpha003-complete-v2"


@dataclass(frozen=True)
class TableContract:
    columns: tuple[str, ...]
    order: tuple[str, ...]
    floats: tuple[str, ...] = ()
    integers: tuple[str, ...] = ()


TABLES = {
    "factor_values": TableContract(("date", "asset", "alpha003"), ("date", "asset"), ("alpha003",)),
    "signals": TableContract(
        ("time", "asset", "factor", "direction", "value", "processed_value", "preprocess", "group"),
        ("factor", "time", "asset"), ("value", "processed_value"), ("direction",)),
    "labels": TableContract(("time", "asset", "period", "value"), ("period", "time", "asset"), ("value",)),
    "ic": TableContract(("time", "factor", "period", "ic", "rank_ic", "n_assets"),
                        ("factor", "period", "time"), ("ic", "rank_ic"), ("n_assets",)),
    "ic_summary": TableContract(
        ("factor", "period", "mean_ic", "std_ic", "icir", "mean_rank_ic", "std_rank_ic", "rank_icir", "n_dates"),
        ("factor", "period"), ("mean_ic", "std_ic", "icir", "mean_rank_ic", "std_rank_ic", "rank_icir"), ("n_dates",)),
    "quantile_returns": TableContract(
        ("time", "factor", "period", "quantile", "mean_return", "n_assets"),
        ("factor", "period", "quantile", "time"), ("mean_return",), ("quantile", "n_assets")),
    "turnover": TableContract(("time", "factor", "period", "quantile", "turnover"),
                              ("factor", "period", "quantile", "time"), ("turnover",), ("quantile",)),
    "spreads": TableContract(
        ("time", "factor", "period", "spread", "top_quantile", "bottom_quantile"),
        ("factor", "period", "time"), ("spread",), ("top_quantile", "bottom_quantile")),
    "stability": TableContract(
        ("factor", "lag", "mean_rank_autocorrelation", "std_rank_autocorrelation", "n_dates"),
        ("factor", "lag"), ("mean_rank_autocorrelation", "std_rank_autocorrelation"), ("lag", "n_dates")),
    "coverage": TableContract(
        ("factor", "kind", "direction", "input_cells", "finite_cells", "label_cells", "evaluable_cells", "status"),
        ("factor", "kind"), (), ("direction", "input_cells", "finite_cells", "label_cells", "evaluable_cells")),
    # Diagnostics may repeat; the occurrence position is its row key.
    "diagnostics": TableContract(("factor", "stage", "severity", "code", "message"), ()),
}


def _normalized_chunk(frame: pd.DataFrame, contract: TableContract) -> pd.DataFrame:
    if tuple(frame.columns) != contract.columns:
        raise ValueError(f"columns differ: expected {contract.columns}, got {tuple(frame.columns)}")
    for name in contract.columns:
        if name in ("time", "date"):
            if not frame[name].str.fullmatch(
                    r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z").all():
                raise ValueError(f"{name} must use canonical UTC ISO8601 Z text")
            frame[name] = pd.to_datetime(frame[name], utc=True, format="ISO8601", errors="raise")
        elif name in contract.floats:
            frame[name] = pd.to_numeric(frame[name].where(frame[name].ne(""), np.nan), errors="raise").astype(float)
        elif name in contract.integers:
            # Counts and classifications must be integers, not approximately so.
            if not frame[name].str.fullmatch(r"-?\d+").all():
                raise ValueError(f"{name} must contain exact integers")
            frame[name] = pd.to_numeric(frame[name], errors="raise").astype("int64")
            if name == "direction":
                valid = frame[name].isin((-1, 1))
            elif name in ("quantile", "top_quantile", "bottom_quantile", "lag"):
                valid = frame[name] > 0
            else:
                valid = frame[name] >= 0
            if not valid.all():
                raise ValueError(f"{name} contains invalid integer values")
        if name in contract.order and name not in ("time", "date"):
            if frame[name].eq("").any():
                raise ValueError(f"{name} row key must not be empty")
    return frame


def _check_order(frame: pd.DataFrame, contract: TableContract, previous: tuple | None,
                 periods: tuple[str, ...], lags: tuple[int, ...]) -> tuple | None:
    if not len(frame) or not contract.order:
        return previous
    order = frame.loc[:, list(contract.order)].copy()
    for name, labels in (("period", periods), ("lag", lags)):
        if name in order:
            order[name] = order[name].map({value: index for index, value in enumerate(labels)})
            if order[name].isna().any():
                raise ValueError(f"unregistered {name} in row keys")
    index = pd.MultiIndex.from_frame(order)
    if not index.is_monotonic_increasing or not index.is_unique:
        raise ValueError("row keys must be unique and in registered output order")
    if previous is not None and previous >= index[0]:
        raise ValueError("row keys duplicate or reverse across CSV chunk boundary")
    return index[-1]


def compare_csv(name: str, left: Path, right: Path, *, tolerance: float = 1e-6,
                periods: tuple[str, ...] = ("1D", "5D", "20D"),
                lags: tuple[int, ...] = (1, 5, 20), chunk_size: int = 100_000) -> dict:
    if not np.isfinite(tolerance) or not 0 < tolerance <= 1e-6:
        raise ValueError("complete comparison requires 0 < tolerance <= 1e-6")
    contract = TABLES[name]
    counts = [0, 0]
    maximum = 0.0
    failures: list[str] = []
    previous = [None, None]
    missing = {column: [0, 0] for column in contract.floats}
    try:
        with pd.read_csv(left, dtype=str, keep_default_na=False, encoding="utf-8",
                         chunksize=chunk_size) as left_reader, \
                pd.read_csv(right, dtype=str, keep_default_na=False, encoding="utf-8",
                            chunksize=chunk_size) as right_reader:
            for pair in zip_longest(left_reader, right_reader):
                chunks = []
                for side, frame in enumerate(pair):
                    if frame is None:
                        chunks.append(None)
                        continue
                    counts[side] += len(frame)
                    frame = _normalized_chunk(frame, contract)
                    previous[side] = _check_order(frame, contract, previous[side], periods, lags)
                    chunks.append(frame)
                    for column in contract.floats:
                        missing[column][side] += int(frame[column].isna().sum())
                if any(frame is None for frame in chunks) or len(chunks[0]) != len(chunks[1]):
                    failures.append("row count differs")
                    continue
                a, b = chunks
                for column in contract.columns:
                    if column in contract.floats:
                        x, y = a[column].to_numpy(), b[column].to_numpy()
                        if not (np.array_equal(np.isnan(x), np.isnan(y))
                                and np.array_equal(np.isposinf(x), np.isposinf(y))
                                and np.array_equal(np.isneginf(x), np.isneginf(y))):
                            failures.append(f"{column}: missing/infinity masks differ")
                        finite = np.isfinite(x) & np.isfinite(y)
                        difference = float(np.max(np.abs(x[finite] - y[finite]), initial=0.0))
                        maximum = max(maximum, difference)
                        if difference > tolerance:
                            failures.append(f"{column}: absolute error {difference:.9g}")
                    elif not a[column].equals(b[column]):
                        failures.append(f"{column}: exact values or ordered keys differ")
    except (ValueError, OSError, KeyError, TypeError, pd.errors.ParserError) as error:
        failures.append(f"{type(error).__name__}: {error}")
    return {
        "name": name, "kind": "primary", "status": "FAIL" if failures else "PASS",
        "rows": {"python": counts[0], "cpp": counts[1], "common": min(counts)},
        "bytes": {"python": left.stat().st_size if left.exists() else None,
                  "cpp": right.stat().st_size if right.exists() else None},
        "max_abs_diff": maximum, "tolerance": tolerance, "missing_counts": missing,
        "failures": list(dict.fromkeys(failures))[:20],
    }


def compare_complete_results(py_dir: Path, cpp_dir: Path, *, tolerance: float = 1e-6) -> dict:
    """Require every obligated file, field, key, mask and common manifest fact."""
    errors = []

    def read_manifest(path: Path) -> dict:
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
            if not isinstance(value, dict):
                raise ValueError("manifest must be an object")
            return value
        except (ValueError, OSError) as error:
            errors.append(f"{path}: {type(error).__name__}: {error}")
            return {}

    py_manifest = read_manifest(py_dir / "manifest.json")
    cpp_manifest = read_manifest(cpp_dir / "tables" / "manifest.json")
    py_spec = py_manifest.get("spec", {})
    if not isinstance(py_spec, dict):
        errors.append("Python spec must be an object")
        py_spec = {}
    periods = py_manifest.get("labels", [])
    if not isinstance(periods, list) or any(not isinstance(p, str) or not p for p in periods):
        periods = []
    periods = tuple(periods)
    lags = py_spec.get("stability_lags", [])
    if not isinstance(lags, list) or any(type(lag) is not int or lag <= 0 for lag in lags):
        lags = []
    lags = tuple(lags)
    if not periods or len(set(periods)) != len(periods):
        errors.append("registered periods must be nonempty and unique")
    if not lags or len(set(lags)) != len(lags):
        errors.append("registered stability lags must be positive unique integers")
    checks = [compare_csv(
        name, py_dir / f"{name}.csv",
        (cpp_dir if name == "factor_values" else cpp_dir / "tables") / f"{name}.csv",
        tolerance=tolerance, periods=periods, lags=lags) for name in TABLES]
    if (py_manifest.get("status") not in ("OK", "PARTIAL", "UNEVALUABLE")
            or py_manifest.get("status") != cpp_manifest.get("status")):
        errors.append("overall result status differs")
    if (py_manifest.get("result_schema_version") != 1 or py_manifest.get("api_version") != "1.0"
            or cpp_manifest.get("schema_id") != "fincore.factor_research_detailed.v2"
            or cpp_manifest.get("schema_version") != 2 or cpp_manifest.get("api_version") != "2.0"):
        errors.append("research manifest schema versions differ from the registered contract")
    for check in checks:
        name = check["name"]
        if name == "factor_values":
            continue
        for engine, manifest in (("python", py_manifest), ("cpp", cpp_manifest)):
            tables = manifest.get("tables", {})
            table = tables.get(name, {}) if isinstance(tables, dict) else {}
            if not isinstance(table, dict):
                table = {}
            if (table.get("columns") != list(TABLES[name].columns)
                    or type(table.get("rows")) is not int
                    or table.get("rows") != check["rows"][engine]):
                errors.append(f"{engine} {name}: manifest columns/rows differ from CSV")
    py_workload = read_manifest(py_dir / "workload.json")
    cpp_workload = read_manifest(cpp_dir / "workload.json")
    if py_workload != cpp_workload:
        errors.append("shared workload manifests differ")
    if py_workload.get("contract") != CONTRACT_VERSION:
        errors.append("shared workload contract version is not complete v2")
    required_workload = {
        "contract", "factor", "periods", "quantiles", "min_assets", "min_quantile_assets",
        "preprocess", "label", "direction", "stability_lags", "start", "research_end",
        "grid_dates", "assets", "factor_cells", "label_config", "serialization",
    }
    if set(py_workload) != required_workload:
        errors.append("shared workload fields differ from the registered contract")
    if py_workload.get("factor") != "alpha003" or py_workload.get("direction") != 1:
        errors.append("shared workload is not the registered alpha003 factor")
    for key in ("factor_cells", "assets", "grid_dates"):
        if type(py_workload.get(key)) is not int or py_workload[key] <= 0:
            errors.append(f"shared workload {key} must be a positive integer")
    factor_check = next(check for check in checks if check["name"] == "factor_values")
    for engine in ("python", "cpp"):
        if factor_check["rows"][engine] != py_workload.get("factor_cells"):
            errors.append(f"{engine} factor_values rows differ from the workload cell count")
    signal_check = next(check for check in checks if check["name"] == "signals")
    label_check = next(check for check in checks if check["name"] == "labels")
    for engine in ("python", "cpp"):
        if signal_check["rows"][engine] <= 0 or label_check["rows"][engine] != signal_check["rows"][engine] * len(periods):
            errors.append(f"{engine} requires a nonempty panel and a complete panel-by-period label grid")
    if py_workload.get("serialization") != {
            "encoding": "utf-8", "line_ending": "LF", "timestamp": "ISO8601-UTC-Z",
            "float": "roundtrip-binary64", "nan": "empty"}:
        errors.append("shared serialization policy differs from the registered contract")
    if py_workload.get("periods") != list(periods) or py_workload.get("stability_lags") != list(lags):
        errors.append("shared workload periods/lags differ from the research manifest")
    for key in ("quantiles", "min_assets", "min_quantile_assets", "preprocess", "label"):
        if py_workload.get(key) != py_spec.get(key):
            errors.append(f"shared workload {key} differs from the research spec")
    checks.append({"name": "manifest", "kind": "primary", "status": "FAIL" if errors else "PASS",
                   "rows": {"python": 1, "cpp": 1, "common": 1}, "max_abs_diff": 0.0,
                   "tolerance": 0.0, "failures": errors})
    passed = all(check["status"] == "PASS" for check in checks)
    return {"contract": CONTRACT_VERSION, "checks": checks,
            "coverage": {"policy": "all fields compared exactly, including counts and status"},
            "all_primary_pass": passed, "all_secondary_pass": True, "all_pass": passed}
