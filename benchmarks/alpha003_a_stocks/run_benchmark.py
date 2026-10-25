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
import importlib.metadata
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
    from . import contract, measurement
else:
    import contract
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
                            Path(contract.__file__),
                            BENCHMARK_ROOT / "cpp" / "alpha003_research.cpp")],
        "ordered_data_files": [file_identity(path) for path in files],
    }


def host_identity(cpp_exe: Path) -> dict[str, Any]:
    host: dict[str, Any] = {
        "platform": platform.platform(), "windows_version": platform.win32_ver(),
        "processor": platform.processor(), "logical_cpu_count": os.cpu_count(),
        "python": sys.version, "pandas": pd.__version__, "numpy": np.__version__,
        "scipy": importlib.metadata.version("scipy"),
        "numpy_build": np.__config__.show(mode="dicts"),
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


def compare_results(py_dir: Path, cpp_dir: Path, args: argparse.Namespace) -> dict[str, Any]:
    return contract.compare_complete_results(py_dir, cpp_dir, tolerance=args.tolerance)


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
        "is checked for every pair. This individual development campaign does not "
        "grant iteration 9 acceptance.",
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
        "Every obligated table, ordered key, missing/infinite mask, count and "
        "classification is checked. All finite numeric fields, including "
        "Pearson IC and stability, use the same absolute tolerance of 1e-6.",
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
        ("**COMPLETE COMPARISON PASS** — all measured and warmup pairs passed "
         "the complete output contract." if
         report["all_pairs_pass"] else
         "**FAIL** — at least one pair failed the complete output contract."),
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
            "requested_pairs": args.repeats,
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
            if not row["comparison"]["all_pass"]:
                records["stopped_reason"] = "warmup_output_mismatch"
                records["inputs_unchanged"] = input_identity(args, cpp_exe) == identities
                write_record(results / "measurements.json", records)
                print(json.dumps({"all_pairs_pass": False,
                                  "stopped_reason": records["stopped_reason"]}))
                return 1

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
            if not comparison["all_pass"]:
                records["stopped_reason"] = "measured_output_mismatch"
                break
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
        "comparison_contract": contract.CONTRACT_VERSION,
        "formal_acceptance": "DEVELOPMENT_CAMPAIGN_ONLY",
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
        "python_process_wall_seconds": performance["wall"]["python_seconds"],
        "cpp_process_wall_seconds": performance["wall"]["cpp_seconds"],
        "process_wall_speedup": performance["wall"]["speedup"],
    }, indent=2))
    return 0 if all_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
