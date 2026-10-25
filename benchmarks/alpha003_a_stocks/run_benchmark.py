#!/usr/bin/env python3
"""Run and compare the alpha003 research benchmark.

Both runners read the same raw per-symbol CSVs. The Python runner uses pandas
for alpha003 and fincore for research; the C++ runner uses native fincore_cpp
kernels. This script runs both, compares the public research tables with a
strict tolerance, and writes a Markdown/JSON report with stage timings.
"""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import locale
import os
import platform
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import numpy as np
import pandas as pd

if __package__:
    from . import measurement
else:
    import measurement

BENCHMARK_ROOT = Path(__file__).resolve().parent
REPO_ROOT = BENCHMARK_ROOT.parents[1]
DEFAULT_DATA = BENCHMARK_ROOT / "data" / "a_stocks"
DEFAULT_RESULTS = BENCHMARK_ROOT / "results"
PYTHON_RUNNER = BENCHMARK_ROOT / "python" / "run_research.py"
PINNED_PYTHON_HEAD = "3e9ef205ef2023f92e89fddd1052d46b16429c64"
CPP_CANDIDATES = (
    REPO_ROOT / "build-win" / "fincore_alpha003_research.exe",
    REPO_ROOT / "build-win" / "Release" / "fincore_alpha003_research.exe",
    REPO_ROOT / "build" / "fincore_alpha003_research",
    REPO_ROOT / "build" / "Release" / "fincore_alpha003_research",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-dir", type=Path, default=DEFAULT_DATA)
    parser.add_argument("--results-dir", type=Path, default=DEFAULT_RESULTS)
    parser.add_argument("--max-symbols", type=int, default=500,
                        help="uniform sample size; 0 uses every stock file")
    parser.add_argument("--repeats", type=int, default=5,
                        help="paired process samples; formal batches need at least 10")
    parser.add_argument("--cpu", type=int, default=None,
                        help="logical CPU index, default: first allowed CPU")
    parser.add_argument("--end-offset", type=int, default=22,
                        help="reserve trailing grid rows so all period labels exist")
    parser.add_argument("--preprocess", default="cross_section_rank")
    parser.add_argument("--fincore-root",
                        default=os.environ.get("FINCORE_ROOT", r"D:\source_code\fincore"))
    parser.add_argument("--cpp-exe", type=Path)
    parser.add_argument("--tolerance", type=float, default=1e-6)
    parser.add_argument("--stability-tolerance", type=float, default=1e-3)
    parser.add_argument("--skip-run", action="store_true",
                        help="reuse existing results/<engine>/run1")
    parser.add_argument("--keep-results", action="store_true",
                        help="deprecated: output directories are always preserved")
    return parser.parse_args()


def find_cpp(explicit: Path | None) -> Path:
    if explicit is not None:
        if not explicit.is_file():
            raise FileNotFoundError(f"C++ benchmark executable not found: {explicit}")
        return explicit
    for candidate in CPP_CANDIDATES:
        if candidate.is_file():
            return candidate
    searched = "\n".join(str(candidate) for candidate in CPP_CANDIDATES)
    raise FileNotFoundError(
        "C++ benchmark executable not found. Build it first:\n"
        "  cmake --build build-win --target fincore_alpha003_research\n"
        f"Searched:\n{searched}")


def run_command(command: list[str], log_path: Path,
                cpu_index: int | None = None) -> dict[str, Any]:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    completed = measurement.run_command(command, cpu_index=cpu_index,
                                        stdout_path=log_path)
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed with exit {completed.returncode}: {' '.join(command)}\n"
            f"see {log_path}")
    return dataclasses.asdict(completed)


def file_identity(path: Path) -> dict[str, Any]:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return {"path": str(path.resolve()), "size_bytes": path.stat().st_size,
            "sha256": digest.hexdigest()}


def git_identity(root: Path) -> dict[str, str]:
    def git(*arguments: str) -> str:
        return subprocess.check_output(
            ["git", "-C", str(root), *arguments], encoding="utf-8").strip()
    return {"head": git("rev-parse", "HEAD"),
            "branch": git("branch", "--show-current"),
            "tracked_status": git("status", "--short", "--untracked-files=no")}


def input_identity(args: argparse.Namespace, cpp_exe: Path) -> dict[str, Any]:
    python_root = Path(args.fincore_root).resolve()
    python_git = git_identity(python_root)
    if python_git["head"] != PINNED_PYTHON_HEAD or python_git["tracked_status"]:
        raise RuntimeError("Python reference must be the clean pinned fincore checkout")
    files = sorted(path for path in args.data_dir.glob("*.csv") if path.is_file())
    if args.max_symbols and args.max_symbols < len(files):
        files = [files[index * len(files) // args.max_symbols]
                 for index in range(args.max_symbols)]
    files += [args.data_dir / "_metadata" / "observed_calendar.csv"]
    return {
        "native_binary": file_identity(cpp_exe),
        "python_binary": file_identity(Path(sys.executable)),
        "cpp_repository": git_identity(REPO_ROOT),
        "python_repository": python_git,
        "python_package_sources": [file_identity(path) for path in
                                   sorted((python_root / "fincore").rglob("*.py"))],
        "runner_sources": [file_identity(path) for path in
                           (Path(__file__), PYTHON_RUNNER,
                            Path(measurement.__file__),
                            BENCHMARK_ROOT / "cpp" / "alpha003_research.cpp")],
        "ordered_data_files": [file_identity(path) for path in files],
    }


def host_identity(cpp_exe: Path) -> dict[str, Any]:
    host: dict[str, Any] = {
        "platform": platform.platform(), "windows_version": platform.win32_ver(),
        "processor": platform.processor(), "logical_cpu_count": os.cpu_count(),
        "python": sys.version, "pandas": pd.__version__, "numpy": np.__version__,
    }
    if os.name == "nt":
        import winreg
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
            host["cpu_model"] = winreg.QueryValueEx(key, "ProcessorNameString")[0].strip()
        power = subprocess.check_output(["powercfg", "/getactivescheme"],
                                        encoding=locale.getencoding(), errors="replace")
        match = re.search(r"[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}", power)
        host["power_scheme_guid"] = match.group(0) if match else None
    cache = cpp_exe.parent / "CMakeCache.txt"
    if not cache.exists():
        cache = cpp_exe.parent.parent / "CMakeCache.txt"
    if cache.exists():
        fields = {}
        for line in cache.read_text(encoding="utf-8").splitlines():
            if line.startswith(("CMAKE_BUILD_TYPE:", "CMAKE_CXX_COMPILER:",
                                "CMAKE_CXX_FLAGS:", "CMAKE_CXX_FLAGS_RELEASE:",
                                "CMAKE_GENERATOR:", "CMAKE_INTERPROCEDURAL_OPTIMIZATION:")):
                key, value = line.split("=", 1)
                fields[key.split(":", 1)[0]] = value
        host["build"] = {"cache": file_identity(cache), "configuration": fields}
        compiler = Path(fields.get("CMAKE_CXX_COMPILER", ""))
        if compiler.is_file():
            host["build"]["compiler_binary"] = file_identity(compiler)
    else:
        host["build"] = {"status": "CMAKE_CACHE_NOT_FOUND"}
    return host


def read_table(path: Path) -> pd.DataFrame:
    frame = pd.read_csv(path)
    if "time" in frame.columns:
        frame["date"] = pd.to_datetime(frame.pop("time"), utc=True)
    elif "date" in frame.columns:
        frame["date"] = pd.to_datetime(frame["date"], utc=True)
    return frame


def numeric_check(name: str, left: pd.DataFrame, right: pd.DataFrame,
                  keys: list[str], left_column: str, right_column: str,
                  tolerance: float, *, allow_left_nan: bool = False,
                  secondary: bool = False) -> dict[str, Any]:
    merged = pd.merge(left, right, on=keys, how="outer", indicator=True,
                      suffixes=("_py", "_cpp"))
    left_name = left_column if left_column in merged.columns else f"{left_column}_py"
    right_name = right_column if right_column in merged.columns else f"{right_column}_cpp"
    both = merged[merged["_merge"] == "both"]
    left_only = merged[merged["_merge"] == "left_only"]
    right_only = merged[merged["_merge"] == "right_only"]
    left_only_nan = int(left_only[left_name].isna().sum()) if len(left_only) else 0
    unexpected_left = left_only[~left_only[left_name].isna()] if allow_left_nan else left_only
    diff = (both[left_name] - both[right_name]).abs()
    max_diff = float(diff.max()) if len(diff) else float("nan")
    finite = diff[np.isfinite(diff)]
    passed = (
        len(right_only) == 0
        and len(unexpected_left) == 0
        and len(finite) == len(diff)
        and (len(finite) == 0 or float(finite.max()) <= tolerance)
    )
    return {
        "name": name,
        "kind": "secondary" if secondary else "primary",
        "status": "PASS" if passed else "FAIL",
        "rows": {"python": int(len(left)), "cpp": int(len(right)),
                 "common": int(len(both)), "python_only": int(len(left_only)),
                 "cpp_only": int(len(right_only)),
                 "python_only_nan": left_only_nan},
        "max_abs_diff": max_diff,
        "tolerance": tolerance,
    }


def aggregate_python_quantiles(frame: pd.DataFrame) -> pd.DataFrame:
    grouped = frame.groupby(["period", "quantile"])["mean_return"].agg(
        mean_py="mean", std_py="std", count_py="count").reset_index()
    grouped["se_py"] = grouped["std_py"] / np.sqrt(grouped["count_py"])
    return grouped


def compare_results(py_dir: Path, cpp_dir: Path, args: argparse.Namespace) -> dict[str, Any]:
    py_tables = py_dir
    cpp_tables = cpp_dir / "tables"
    checks: list[dict[str, Any]] = []

    py_factor = read_table(py_dir / "factor_values.csv")
    cpp_factor = read_table(cpp_dir / "factor_values.csv")
    checks.append(numeric_check(
        "factor_panel", py_factor, cpp_factor, ["date", "asset"],
        "alpha003", "alpha003", args.tolerance))

    py_signals = read_table(py_tables / "signals.csv")
    cpp_signals = read_table(cpp_tables / "signals.csv")
    checks.append(numeric_check(
        "signals.raw_value", py_signals, cpp_signals, ["date", "asset"],
        "value", "value", args.tolerance))
    checks.append(numeric_check(
        "signals.processed_value", py_signals, cpp_signals,
        ["date", "asset"], "processed_value", "processed_value",
        args.tolerance))

    py_labels = read_table(py_tables / "labels.csv")
    cpp_labels = read_table(cpp_tables / "labels.csv")
    checks.append(numeric_check(
        "labels.common", py_labels, cpp_labels,
        ["date", "asset", "period"], "value", "value", args.tolerance,
        allow_left_nan=True))

    py_ic = read_table(py_tables / "ic.csv")
    cpp_ic = read_table(cpp_tables / "ic.csv")
    cpp_ic_long = cpp_ic.melt(id_vars="date", var_name="period",
                              value_name="ic_cpp")
    checks.append(numeric_check(
        "ic.rank_ic", py_ic, cpp_ic_long, ["date", "period"],
        "rank_ic", "ic_cpp", args.tolerance))

    py_summary = read_table(py_tables / "ic_summary.csv")
    cpp_summary = read_table(cpp_tables / "ic_summary.csv").rename(
        columns={"label": "period", "mean_ic": "mean_ic_cpp"})
    checks.append(numeric_check(
        "ic_summary.mean_rank_ic", py_summary, cpp_summary, ["period"],
        "mean_rank_ic", "mean_ic_cpp", args.tolerance))

    py_quantiles = read_table(py_tables / "quantile_returns.csv")
    cpp_quantiles = read_table(cpp_tables / "quantile_returns.csv").rename(
        columns={"label": "period", "mean": "mean_cpp",
                 "standard_error": "se_cpp"})
    py_quantile_agg = aggregate_python_quantiles(py_quantiles)
    checks.append(numeric_check(
        "quantile_returns.mean", py_quantile_agg, cpp_quantiles,
        ["period", "quantile"], "mean_py", "mean_cpp", args.tolerance))
    checks.append(numeric_check(
        "quantile_returns.standard_error", py_quantile_agg, cpp_quantiles,
        ["period", "quantile"], "se_py", "se_cpp", args.tolerance))

    py_spreads = read_table(py_tables / "spreads.csv")
    cpp_spreads = read_table(cpp_tables / "spreads.csv").rename(
        columns={"label": "period", "spread": "spread_cpp"})
    py_spread_agg = py_spreads.groupby("period")["spread"].mean().reset_index()
    checks.append(numeric_check(
        "spreads.mean", py_spread_agg, cpp_spreads, ["period"],
        "spread", "spread_cpp", args.tolerance))

    py_turnover = read_table(py_tables / "turnover.csv")
    cpp_turnover = read_table(cpp_tables / "turnover.csv").rename(
        columns={"value": "turnover_cpp"})
    first_period = str(py_turnover["period"].iloc[0])
    py_turnover_first = py_turnover[py_turnover["period"] == first_period][
        ["date", "quantile", "turnover"]]
    turnover_merge = pd.merge(py_turnover_first, cpp_turnover,
                              on=["date", "quantile"], how="outer")
    turnover_both = turnover_merge.dropna(subset=["turnover", "turnover_cpp"])
    turnover_diff = (turnover_both["turnover"] - turnover_both["turnover_cpp"]).abs()
    checks.append({
        "name": f"turnover.{first_period}",
        "kind": "primary",
        "status": "PASS" if (len(turnover_diff) > 0 and
                             float(turnover_diff.max()) <= args.tolerance) else "FAIL",
        "rows": {"python": int(len(py_turnover_first)),
                 "cpp": int(len(cpp_turnover)),
                 "common": int(len(turnover_both))},
        "max_abs_diff": float(turnover_diff.max()) if len(turnover_diff) else float("nan"),
        "tolerance": args.tolerance,
    })

    py_stability = read_table(py_tables / "stability.csv")
    cpp_stability = read_table(cpp_tables / "stability.csv")
    cpp_stability_agg = cpp_stability.groupby("lag")["value"].mean().reset_index(
        name="value_cpp")
    checks.append(numeric_check(
        "stability.published_summary", py_stability, cpp_stability_agg, ["lag"],
        "mean_rank_autocorrelation", "value_cpp", args.stability_tolerance,
        secondary=True))

    # Per-date kernel check on the shared prepared cross-sections. The
    # published Python summary aggregates a slightly different table, so this
    # check proves the rank-autocorrelation kernel itself.
    kernel = stability_kernel_check(py_signals, py_labels, cpp_stability,
                                    first_period, args.tolerance)
    if kernel is not None:
        checks.append(kernel)

    py_coverage = read_table(py_tables / "coverage.csv")
    cpp_coverage = read_table(cpp_tables / "coverage.csv")
    coverage_note = {
        "python": py_coverage.to_dict(orient="records"),
        "cpp": cpp_coverage.to_dict(orient="records"),
        "note": ("Python coverage is factor-level with input/finite/label/"
                 "evaluable counts; C++ coverage counts prepared rows per "
                 "horizon. They are reported, not numerically gated."),
    }

    statuses = [check["status"] for check in checks]
    return {
        "checks": checks,
        "coverage": coverage_note,
        "all_primary_pass": all(
            check["status"] == "PASS" for check in checks
            if check["kind"] == "primary"),
        "all_secondary_pass": all(
            check["status"] == "PASS" for check in checks
            if check["kind"] == "secondary"),
        "all_pass": all(status == "PASS" for status in statuses),
    }


def stability_kernel_check(py_signals: pd.DataFrame, py_labels: pd.DataFrame,
                           cpp_stability: pd.DataFrame, first_period: str,
                           tolerance: float) -> dict[str, Any] | None:
    labels = py_labels[py_labels["period"] == first_period][
        ["date", "asset", "value"]].rename(columns={"value": "label"})
    # Research stability consumes the direction-adjusted, preprocessed factor
    # (the same `processed_value` exposed in signals).
    signals = py_signals[["date", "asset", "processed_value"]].rename(
        columns={"processed_value": "factor"})
    prepared = labels.merge(signals, on=["date", "asset"])
    prepared = prepared.dropna(subset=["label", "factor"])
    kept = []
    for _, group in prepared.groupby("date"):
        try:
            group = group.copy()
            pd.qcut(group["factor"], 5, labels=False)
            kept.append(group)
        except ValueError:
            continue
    if not kept:
        return None
    prepared = pd.concat(kept).set_index(["date", "asset"])
    ranks = prepared.groupby(level="date")["factor"].rank()
    wide = ranks.unstack("asset")
    cpp = cpp_stability.copy()
    cpp["date"] = pd.to_datetime(cpp["date"], utc=True)
    max_diffs = []
    common_rows = 0
    for lag, group in cpp.groupby("lag"):
        reconstructed = wide.corrwith(wide.shift(int(lag)), axis=1)
        expected = group.set_index("date")["value"]
        merged = pd.concat([reconstructed.rename("py"),
                            expected.rename("cpp")], axis=1).dropna()
        common_rows += len(merged)
        if len(merged):
            max_diffs.append(float((merged["py"] - merged["cpp"]).abs().max()))
    max_diff = max(max_diffs) if max_diffs else float("nan")
    return {
        "name": "stability.per_date_kernel",
        "kind": "primary",
        "status": "PASS" if np.isfinite(max_diff) and max_diff <= tolerance else "FAIL",
        "rows": {"common": int(common_rows)},
        "max_abs_diff": max_diff,
        "tolerance": tolerance,
    }


def medians(values: list[float]) -> float:
    return float(statistics.median(values)) if values else float("nan")


def collect_timings(run_dir: Path) -> dict[str, float]:
    payload = json.loads((run_dir / "timings.json").read_text(encoding="utf-8"))
    return {key: float(value) for key, value in payload.items()
            if key.endswith("_seconds") and isinstance(value, (float, int))}


def performance_report(py_runs: list[Path], cpp_runs: list[Path],
                       py_wall: list[float], cpp_wall: list[float]) -> dict[str, Any]:
    py_stage = [collect_timings(run) for run in py_runs]
    cpp_stage = [collect_timings(run) for run in cpp_runs]
    stages = {}
    common_stages = sorted(set.intersection(
        *(set(item) for item in py_stage + cpp_stage)))
    for stage in common_stages:
        py_median = medians([item[stage] for item in py_stage])
        cpp_median = medians([item[stage] for item in cpp_stage])
        stages[stage] = {
            "python_seconds": py_median,
            "cpp_seconds": cpp_median,
            "speedup": py_median / cpp_median if cpp_median else float("nan"),
        }
    py_wall_median = medians(py_wall)
    cpp_wall_median = medians(cpp_wall)
    return {
        "repeats": len(py_runs),
        "stages": stages,
        "wall": {
            "python_seconds": py_wall_median,
            "cpp_seconds": cpp_wall_median,
            "speedup": py_wall_median / cpp_wall_median if cpp_wall_median else float("nan"),
        },
        "sampling": {
            "python": measurement.summarize_samples(py_wall),
            "cpp": measurement.summarize_samples(cpp_wall),
            "paired_speedups": [py / cpp for py, cpp in zip(py_wall, cpp_wall)],
            "pairs_at_least_30x": sum(py / cpp >= 30 for py, cpp in zip(py_wall, cpp_wall)),
            "pairs_at_least_100x": sum(py / cpp >= 100 for py, cpp in zip(py_wall, cpp_wall)),
        },
    }


def format_markdown(report: dict[str, Any]) -> str:
    lines = [
        "# alpha003 A-share research benchmark",
        "",
        f"Generated: {report['generated_at']}",
        "",
        "Measurement protocol: single logical CPU, one-thread environment, "
        "symmetric warmups and alternating paired order. Full-output parity "
        "is still pending P112; this report cannot grant iteration 9 acceptance.",
        "",
        "## Configuration",
        "",
        f"- Data: `{report['config']['data_dir']}`",
        f"- Uniform sample: {report['config']['max_symbols']} files "
        f"(0 means all); assets after factor evaluation: {report['scale']['assets']}",
        f"- Grid dates: {report['scale']['grid_dates']}; research end: "
        f"{report['config']['research_end']} (tail offset {report['config']['end_offset']})",
        f"- Periods: {report['config']['periods']}; quantiles: "
        f"{report['config']['quantiles']}; preprocessing: "
        f"`{report['config']['preprocess']}`",
        f"- Factor cells: {report['scale']['factor_cells']}",
        "",
        "## Numerical comparison",
        "",
        "| Check | Status | Rows (py/cpp/common) | Max abs diff | Tolerance |",
        "|---|---|---:|---:|---:|",
    ]
    for check in report["comparison"]["checks"]:
        rows = check["rows"]
        row_text = (f"{rows.get('python', '-')}/{rows.get('cpp', '-')}/"
                    f"{rows.get('common', rows.get('python', '-'))}")
        lines.append(
            f"| {check['name']} | {check['status']} | {row_text} | "
            f"{check['max_abs_diff']:.3e} | {check['tolerance']:.0e} |")
    lines += [
        "",
        "`ic.rank_ic` compares the C++ `ic.csv` column with Python `rank_ic`; "
        "the C++ research table is a rank IC. Python also reports Pearson "
        "`ic`, which the C++ export does not contain.",
        "",
        "`labels.common` compares only cells present in both tables. Python "
        "keeps NaN forward-return rows; C++ omits them, so the Python-only "
        "count is reported above and is not a numeric failure.",
        "",
        "`stability.published_summary` is secondary: Python's summary table "
        "aggregates a slightly different prepared set. "
        "`stability.per_date_kernel` reconstructs the rank-autocorrelation "
        "series from the shared cells and proves the kernel itself.",
        "",
        "## Performance",
        "",
        "| Stage | Python median (s) | C++ median (s) | Python / C++ |",
        "|---|---:|---:|---:|",
    ]
    for stage, values in report["performance"]["stages"].items():
        lines.append(
            f"| {stage} | {values['python_seconds']:.4f} | "
            f"{values['cpp_seconds']:.4f} | {values['speedup']:.1f}x |")
    lines += [
        f"| wall (process) | {report['performance']['wall']['python_seconds']:.4f} | "
        f"{report['performance']['wall']['cpp_seconds']:.4f} | "
        f"{report['performance']['wall']['speedup']:.1f}x |",
        "",
        "Wall time includes each process invocation; stage times come from the "
        "runners' `timings.json` files and exclude interpreter import/startup.",
        "",
        "| Process wall distribution | Median (s) | IQR / median | Min / max (s) | Noise gate |",
        "|---|---:|---:|---:|---|",
        *[
            f"| {engine} | {sample['median']:.6f} | {sample['iqr_over_median']:.2%} | "
            f"{sample['min']:.6f} / {sample['max']:.6f} | {sample['status']} |"
            for engine in ("python", "cpp")
            for sample in [report["performance"]["sampling"][engine]]
        ],
        "",
        "Per-process CPU time, peak working set, actual affinity, thread environment, "
        "raw timings and every pair's comparison are retained in `measurements.json`.",
        "",
        "## Coverage notes",
        "",
        "```json",
        json.dumps(report["comparison"]["coverage"], indent=2, default=str),
        "```",
        "",
        "## Verdict",
        "",
        ("**PARTIAL COMPARISON PASS** — all measured and warmup pairs passed "
         "the legacy subset checks; full parity remains pending." if
         report["all_pairs_pass"] else
         "**FAIL** — at least one pair failed the legacy subset checks."),
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    args.data_dir = args.data_dir.resolve()
    results = args.results_dir.resolve()
    cpp_exe = find_cpp(args.cpp_exe)
    if args.repeats < 1:
        raise ValueError("--repeats must be at least 1")
    if args.max_symbols < 0:
        raise ValueError("--max-symbols must be non-negative")

    def write_record(path: Path, payload: Any) -> None:
        path.write_text(json.dumps(payload, indent=2, sort_keys=True, default=str)
                        + "\n", encoding="utf-8", newline="\n")

    def command_for(engine: str, out: Path, run_id: str) -> list[str]:
        common = ["--data-dir", str(args.data_dir), "--out", str(out),
                  "--max-symbols", str(args.max_symbols),
                  "--end-offset", str(args.end_offset),
                  "--preprocess", args.preprocess, "--run-id", run_id]
        if engine == "python":
            return [sys.executable, str(PYTHON_RUNNER), *common,
                    "--fincore-root", str(args.fincore_root)]
        return [str(cpp_exe), *common]

    engines = {"A": "python", "B": "cpp"}
    all_comparisons = []

    if not args.skip_run:
        # Historical measurements must never be silently replaced.
        if results.exists():
            raise FileExistsError(f"use a fresh --results-dir: {results}")
        (results / "logs").mkdir(parents=True)
        identities = input_identity(args, cpp_exe)
        write_record(results / "inputs.json", identities)
        host = host_identity(cpp_exe)
        write_record(results / "environment.json", host)
        records: dict[str, Any] = {
            "protocol": "i9-process-paired-v1", "warmups": [], "pairs": [],
            "cpu_index": args.cpu, "cache_policy": "symmetric-warm-raw-csv",
        }

        # Each implementation gets two fresh-process warmups (AB then BA).
        # Warmup outputs are also checked outside the measured intervals.
        for index, order in enumerate(measurement.warmup_order_schedule(), 1):
            row: dict[str, Any] = {"order": list(order)}
            paths = {}
            for letter in order:
                engine = engines[letter]
                out = results / "warmups" / engine / f"run{index}"
                paths[engine] = out
                print(f"[warmup {index}/2] {engine} ...", flush=True)
                row[engine] = run_command(
                    command_for(engine, out, f"warmup{index}"),
                    results / "logs" / f"{engine}-warmup{index}.log", args.cpu)
            row["comparison"] = compare_results(paths["python"], paths["cpp"], args)
            records["warmups"].append(row)
            write_record(results / "measurements.json", records)

        py_runs: list[Path] = []
        cpp_runs: list[Path] = []
        py_wall: list[float] = []
        cpp_wall: list[float] = []
        for repeat, order in enumerate(measurement.pair_order_schedule(args.repeats), 1):
            py_run = results / "python" / f"run{repeat}"
            cpp_run = results / "cpp" / f"run{repeat}"
            paths = {"python": py_run, "cpp": cpp_run}
            row = {"order": list(order)}
            for letter in order:
                engine = engines[letter]
                print(f"[{repeat}/{args.repeats}] {engine} ...", flush=True)
                row[engine] = run_command(
                    command_for(engine, paths[engine], f"run{repeat}"),
                    results / "logs" / f"{engine}-run{repeat}.log", args.cpu)
            py_wall.append(row["python"]["wall_seconds"])
            cpp_wall.append(row["cpp"]["wall_seconds"])
            py_runs.append(py_run)
            cpp_runs.append(cpp_run)
            # Comparison finishes before the next process measurement starts.
            comparison = compare_results(py_run, cpp_run, args)
            all_comparisons.append(comparison)
            row["comparison"] = comparison
            records["pairs"].append(row)
            write_record(results / "measurements.json", records)
        unchanged = input_identity(args, cpp_exe) == identities
        records["inputs_unchanged"] = unchanged
        write_record(results / "measurements.json", records)
        if not unchanged:
            raise RuntimeError("benchmark inputs changed during the campaign")
    else:
        records = json.loads((results / "measurements.json").read_text(encoding="utf-8"))
        host = json.loads((results / "environment.json").read_text(encoding="utf-8"))
        py_runs = [results / "python" / f"run{index + 1}"
                   for index in range(len(records["pairs"]))]
        cpp_runs = [results / "cpp" / f"run{index + 1}"
                    for index in range(len(records["pairs"]))]
        py_wall = [row["python"]["wall_seconds"] for row in records["pairs"]]
        cpp_wall = [row["cpp"]["wall_seconds"] for row in records["pairs"]]
        all_comparisons = [compare_results(py, cpp, args)
                           for py, cpp in zip(py_runs, cpp_runs)]

    py_info = json.loads((py_runs[0] / "run_info.json").read_text(encoding="utf-8"))
    cpp_info = json.loads((cpp_runs[0] / "run_info.json").read_text(encoding="utf-8"))
    comparison = all_comparisons[0]
    all_pass = (all(item["all_pass"] for item in all_comparisons)
                and all(item["comparison"]["all_pass"] for item in records["warmups"]))
    performance = performance_report(py_runs, cpp_runs, py_wall, cpp_wall)
    report = {
        "schema_version": 2,
        "comparison_contract": "legacy-partial-v1-pending-P112",
        "formal_acceptance": "NOT_ELIGIBLE_PENDING_FULL_PARITY",
        "all_pairs_pass": all_pass,
        "all_comparisons": all_comparisons,
        "measurements": records,
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "host": host,
        "config": {
            "data_dir": str(args.data_dir),
            "max_symbols": args.max_symbols,
            "end_offset": args.end_offset,
            "research_end": py_info.get("research_end"),
            "preprocess": args.preprocess,
            "periods": py_info.get("periods"),
            "quantiles": py_info.get("quantiles"),
            "tolerance": args.tolerance,
            "stability_tolerance": args.stability_tolerance,
        },
        "scale": {
            "assets": py_info.get("assets"),
            "grid_dates": py_info.get("grid_dates"),
            "factor_cells": py_info.get("factor_cells"),
        },
        "python": py_info,
        "cpp": cpp_info,
        "comparison": comparison,
        "performance": performance,
    }
    # Rechecking never rewrites the report from the original measurement.
    suffix = f"-recheck-{time.time_ns()}" if args.skip_run else ""
    with (results / f"comparison{suffix}.json").open(
            "x", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(report, indent=2, sort_keys=True, default=str) + "\n")
    with (results / f"report{suffix}.md").open(
            "x", encoding="utf-8", newline="\n") as output:
        output.write(format_markdown(report))
    print(json.dumps({
        "all_pairs_pass": all_pass,
        "formal_acceptance": report["formal_acceptance"],
        "max_primary_diff": max(
            (check["max_abs_diff"] for item in all_comparisons for check in item["checks"]
             if check["kind"] == "primary" and np.isfinite(check["max_abs_diff"])),
            default=float("nan")),
        "python_total_seconds": performance["stages"]["total_seconds"]["python_seconds"],
        "cpp_total_seconds": performance["stages"]["total_seconds"]["cpp_seconds"],
        "total_speedup": performance["stages"]["total_seconds"]["speedup"],
    }, indent=2))
    return 0 if all_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
