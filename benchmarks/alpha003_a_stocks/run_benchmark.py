#!/usr/bin/env python3
"""Run and compare the alpha003 research benchmark, retaining HTML reports.

Both runners read the same raw per-symbol CSVs. The Python runner uses pandas
for alpha003 and fincore for research; the C++ runner uses native fincore_cpp
kernels. This script runs both, compares the public research tables with a
strict tolerance, and writes one self-contained HTML comparison report.
"""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
from html import escape as html_escape
import importlib.metadata
import json
import locale
import math
import os
import platform
import re
import statistics
import subprocess
import sys
import tempfile
import time
from html.parser import HTMLParser
from pathlib import Path
from shutil import copy2
from typing import Any

import numpy as np
import pandas as pd

if __package__:
    from . import contract, html_report, measurement
else:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from benchmarks.alpha003_a_stocks import contract, html_report, measurement

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
                        help="number of measured paired process samples")
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
                        help="recheck only the retained pair in an existing comparison.html")
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


def _log_excerpt(path: Path, limit: int = 8192) -> dict[str, Any]:
    if not path.exists():
        return {"text": "", "truncated": False}
    size = path.stat().st_size
    with path.open("rb") as stream:
        if size > limit:
            stream.seek(-limit, os.SEEK_END)
        raw = stream.read(limit)
    return {"text": raw.decode("utf-8", errors="replace"),
            "truncated": size > limit, "bytes_retained": len(raw)}


def run_command(command: list[str], stdout_path: Path, stderr_path: Path,
                cpu_index: int | None = None) -> dict[str, Any]:
    stdout_path.parent.mkdir(parents=True, exist_ok=True)
    stderr_path.parent.mkdir(parents=True, exist_ok=True)
    base: dict[str, Any] = {
        "command": [str(part) for part in command],
        "returncode": None,
    }
    try:
        completed = measurement.run_command(
            command, cpu_index=cpu_index, stdout_path=stdout_path,
            stderr_path=stderr_path)
    except Exception as error:
        base["launch_error"] = {
            "type": type(error).__name__, "message": str(error)[:2000],
        }
    else:
        base.update(dataclasses.asdict(completed))
    base["stdout_excerpt"] = _log_excerpt(stdout_path)
    base["stderr_excerpt"] = _log_excerpt(stderr_path)
    return base


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
                            Path(html_report.__file__),
                            BENCHMARK_ROOT / "report_template.html",
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


compare_results = html_report.compare_reports


def medians(values: list[float]) -> float | None:
    return float(statistics.median(values)) if values else None


def collect_timings(run_dir: Path) -> dict[str, float]:
    payload = html_report.read_report(run_dir / html_report.REPORT_FILENAME)
    return {key: float(value) for key, value in payload["timings"].items()
            if key.endswith("_seconds") and isinstance(value, (float, int))
            and math.isfinite(value)}


def performance_report(records: dict[str, Any]) -> dict[str, Any]:
    measured = records["pairs"]
    paired = [row for row in measured
              if _valid_wall(row.get("python", {}).get("wall_seconds"))
              and _valid_wall(row.get("cpp", {}).get("wall_seconds"))]
    py_wall = [float(row["python"]["wall_seconds"]) for row in paired]
    cpp_wall = [float(row["cpp"]["wall_seconds"]) for row in paired]
    py_stage = [row["python"].get("timings", {}) for row in paired
                if isinstance(row["python"].get("timings"), dict)]
    cpp_stage = [row["cpp"].get("timings", {}) for row in paired
                 if isinstance(row["cpp"].get("timings"), dict)]
    stages = {}
    all_stages = py_stage + cpp_stage
    common_stages = sorted(set.intersection(*(set(item) for item in all_stages))) if all_stages else []
    for stage in common_stages:
        py_median = medians([item[stage] for item in py_stage])
        cpp_median = medians([item[stage] for item in cpp_stage])
        stages[stage] = {
            "python_seconds": py_median,
            "cpp_seconds": cpp_median,
            "speedup": (py_median / cpp_median
                        if py_median is not None and cpp_median else None),
        }
    py_wall_median = medians(py_wall)
    cpp_wall_median = medians(cpp_wall)
    return {
        "repeats": len(paired),
        "requested_pairs": records["requested_pairs"],
        "stages": stages,
        "wall": {
            "python_seconds": py_wall_median,
            "cpp_seconds": cpp_wall_median,
            "speedup": (py_wall_median / cpp_wall_median
                        if py_wall_median is not None and cpp_wall_median else None),
        },
        "sampling": {
            "python": _sample_summary(py_wall),
            "cpp": _sample_summary(cpp_wall),
            "paired_speedups": [py / cpp for py, cpp in zip(py_wall, cpp_wall) if cpp > 0],
        },
    }


def _sample_summary(values: list[float]) -> dict[str, Any]:
    if not values:
        return {"samples": [], "count": 0, "status": "UNAVAILABLE",
                "median": None, "iqr_over_median": None}
    return measurement.summarize_samples(values)


def _valid_wall(value: Any) -> bool:
    return (isinstance(value, (int, float)) and not isinstance(value, bool)
            and math.isfinite(value) and value >= 0)


class _ComparisonPayloadParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=False)
        self.matches = 0
        self.scripts = 0
        self.script_type: str | None = None
        self.closed = False
        self.capturing = False
        self.chunks: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if values.get("id") != "fincore-benchmark-data":
            return
        self.matches += 1
        if tag.lower() != "script":
            return
        self.scripts += 1
        self.script_type = values.get("type")
        self.capturing = True

    def handle_data(self, data: str) -> None:
        if self.capturing:
            self.chunks.append(data)

    def handle_entityref(self, name: str) -> None:
        if self.capturing:
            self.chunks.append(f"&{name};")

    def handle_charref(self, name: str) -> None:
        if self.capturing:
            self.chunks.append(f"&#{name};")

    def handle_endtag(self, tag: str) -> None:
        if self.capturing and tag.lower() == "script":
            self.capturing = False
            self.closed = True


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"non-standard JSON constant {value!r} is forbidden")


def _unique_json_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key!r}")
        result[key] = value
    return result


def read_comparison(path: Path) -> dict[str, Any]:
    """Read the embedded benchmark record from comparison.html."""
    parser = _ComparisonPayloadParser()
    parser.feed(Path(path).read_text(encoding="utf-8"))
    parser.close()
    if (parser.matches != 1 or parser.scripts != 1
            or parser.script_type != "application/json" or not parser.closed
            or parser.capturing):
        raise ValueError("comparison.html must contain one closed benchmark JSON record")
    payload = json.loads("".join(parser.chunks), object_pairs_hook=_unique_json_object,
                         parse_constant=_reject_json_constant)
    if not isinstance(payload, dict) or payload.get("schema_version") != "alpha003-benchmark-html-v1":
        raise ValueError("unsupported comparison report schema")
    return payload


def _json_script_text(payload: dict[str, Any]) -> str:
    value = json.dumps(payload, ensure_ascii=False, allow_nan=False,
                       separators=(",", ":"), default=str)
    return (value.replace("&", r"\u0026").replace("<", r"\u003c")
            .replace(">", r"\u003e").replace("\u2028", r"\u2028")
            .replace("\u2029", r"\u2029"))


def _number(value: Any, digits: int = 4) -> str:
    if not isinstance(value, (int, float)) or not math.isfinite(value):
        return "—"
    return f"{value:.{digits}f}"


def _render_comparison_html(report: dict[str, Any]) -> str:
    config = report["configuration"]
    performance = report["performance"]
    sample_summary = performance["sampling"]
    max_symbols = config["max_symbols"]
    scope = "all source files" if max_symbols == 0 else f"uniform sample of {max_symbols} files"
    cpu = config.get("observed_cpu_index")
    cpu_text = f"logical CPU {cpu}" if cpu is not None else "first allowed logical CPU (not observed)"
    status = "PASS" if report["all_pairs_pass"] else "FAIL"

    pair_rows: list[str] = []
    measurements = report["measurements"]
    for section, title in (("warmups", "Warmups"), ("pairs", "Measured pairs")):
        for row in measurements.get(section, []):
            python_run = row.get("python", {})
            cpp_run = row.get("cpp", {})
            python_wall = python_run.get("wall_seconds")
            cpp_wall = cpp_run.get("wall_seconds")
            ratio = (python_wall / cpp_wall if _valid_wall(python_wall)
                     and _valid_wall(cpp_wall) and cpp_wall > 0 else None)
            comparison = row.get("comparison") or {}
            pair_rows.append(
                "<tr><td>" + html_escape(title) + " " + str(row.get("index", "?")) +
                "</td><td>" + html_escape("".join(row.get("order", []))) +
                "</td><td>" + _number(python_wall, 6) +
                "</td><td>" + _number(cpp_wall, 6) +
                "</td><td>" + _number(ratio, 3) +
                "</td><td>" + html_escape(str(comparison.get("all_pass", "not compared"))) +
                "</td></tr>")
    if not pair_rows:
        pair_rows.append('<tr><td colspan="6">No complete pair was compared.</td></tr>')

    sampling_rows = []
    for engine in ("python", "cpp"):
        sample = sample_summary[engine]
        iqr_ratio = sample.get("iqr_over_median")
        ratio_text = (f"{100 * iqr_ratio:.2f}%" if isinstance(iqr_ratio, (int, float))
                      and math.isfinite(iqr_ratio) else "unavailable")
        sampling_rows.append(
            f"<tr><td>{html_escape(engine)}</td>"
            f"<td>{_number(sample.get('median'), 6)}</td>"
            f"<td>{ratio_text}</td><td>{html_escape(str(sample.get('status', 'UNAVAILABLE')))}</td></tr>")

    parity_rows: list[str] = []
    latest = report.get("last_retained_comparison") or {}
    checks = {check.get("name"): check for check in latest.get("checks", [])}
    for table_name in html_report.TABLE_NAMES:
        check = checks.get(table_name, {})
        parity_rows.append(
            f"<tr><td>{html_escape(table_name)}</td>"
            f"<td>{html_escape(str(check.get('status', 'UNAVAILABLE')))}</td>"
            f"<td>{_number(check.get('max_abs_diff'), 9)}</td></tr>")

    retained = report.get("retained_reports", {})
    report_links = []
    for engine in ("python", "cpp"):
        relative = f"{engine}/research.html"
        if engine in retained:
            report_links.append(f'<a href="{relative}">{html_escape(engine)} research.html</a>')
        else:
            report_links.append(f"<span>{html_escape(engine)} report unavailable</span>")

    failure = report.get("failure")
    failure_html = ("<p>No failure recorded.</p>" if failure is None else
                    "<pre>" + html_escape(json.dumps(failure, ensure_ascii=False,
                                                        indent=2, default=str)) + "</pre>")
    median_speedup = performance["wall"].get("speedup")
    if median_speedup is None:
        speedup_text = "unavailable"
    else:
        speedup_text = f"{median_speedup:.3f}×"

    payload_script = _json_script_text(report)
    return f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>alpha003 benchmark comparison</title>
<style>
body{{font:16px/1.5 system-ui,sans-serif;max-width:1100px;margin:2rem auto;padding:0 1rem;color:#17202a}}
h1,h2{{line-height:1.2}}.status{{font-size:1.4rem;font-weight:700}}table{{border-collapse:collapse;width:100%;margin:1rem 0 2rem}}
th,td{{border:1px solid #ccd3d8;padding:.5rem;text-align:left}}th{{background:#eef2f4}}
.notice{{background:#fff7dd;padding:1rem;border-left:4px solid #d69e00}}pre{{white-space:pre-wrap;overflow-wrap:anywhere;background:#f5f6f7;padding:1rem}}
</style></head><body>
<h1>alpha003 A-share benchmark</h1>
<p class="status">{status}: {html_escape(report.get('outcome', 'campaign result'))}</p>
<p>Configuration: {html_escape(scope)}; data directory <code>{html_escape(str(config['data_dir']))}</code>;
{html_escape(cpu_text)}; {config['requested_pairs']} requested measured pairs; one-thread environment.</p>
<p>Measured-pair median Python/C++ wall ratio: <strong>{speedup_text}</strong>.
Process wall includes launch through HTML close and process exit.</p>
<div class="notice">This result uses the new single-file HTML output contract and compares eight embedded research tables plus the shared workload. It does not establish parity for the former 13-file CSV output contract.</div>
<h2>Per-pair process wall</h2><table><thead><tr><th>Pair</th><th>Order</th><th>Python (s)</th><th>C++ (s)</th><th>Python/C++</th><th>Parity</th></tr></thead><tbody>{''.join(pair_rows)}</tbody></table>
<h2>Noise gate</h2><table><thead><tr><th>Engine</th><th>Median wall (s)</th><th>IQR / median</th><th>Status</th></tr></thead><tbody>{''.join(sampling_rows)}</tbody></table>
<h2>Last retained pair: eight-table parity</h2><table><thead><tr><th>Table</th><th>Status</th><th>Max absolute difference</th></tr></thead><tbody>{''.join(parity_rows)}</tbody></table>
<h2>Research reports</h2><p>{' · '.join(report_links)}</p>
<h2>Failure record</h2>{failure_html}
<script id="fincore-benchmark-data" type="application/json">{payload_script}</script>
</body></html>\n'''


def write_comparison(path: Path, report: dict[str, Any]) -> None:
    with Path(path).open("x", encoding="utf-8", newline="\n") as output:
        output.write(_render_comparison_html(report))


def _run_failed(result: dict[str, Any]) -> bool:
    return result.get("returncode") != 0 or "launch_error" in result


def _output_directory_issue(path: Path) -> dict[str, Any] | None:
    if path.is_symlink() or not path.is_dir():
        return {"reason": "output directory missing", "entries": []}
    entries = list(path.iterdir())
    names = sorted(entry.name for entry in entries)
    report = path / html_report.REPORT_FILENAME
    if (names != [html_report.REPORT_FILENAME] or not report.is_file()
            or report.is_symlink()):
        return {
            "reason": "each child output directory must contain only research.html",
            "entries": [name[:160] for name in names[:50]],
            "entries_truncated": len(names) > 50,
        }
    return None


def _failure_record(phase: str, message: str, **facts: Any) -> dict[str, Any]:
    return {"phase": phase, "message": message[:2000], **facts}


def _find_pair_row(report: dict[str, Any], retained: dict[str, Any]) -> dict[str, Any]:
    section = "warmups" if retained.get("kind") == "warmup" else "pairs"
    for row in report["measurements"].get(section, []):
        if row.get("index") == retained.get("index"):
            return row
    raise ValueError("retained pair is absent from embedded measurements")


def verify_skip_run(results: Path) -> dict[str, Any]:
    report = read_comparison(results / "comparison.html")
    retained = report.get("last_retained_pair")
    hashes = report.get("retained_reports")
    if not isinstance(retained, dict) or not isinstance(hashes, dict) or set(hashes) != {"python", "cpp"}:
        raise ValueError("comparison report does not contain one complete retained pair")
    current: dict[str, Path] = {}
    for engine in ("python", "cpp"):
        path = results / engine / html_report.REPORT_FILENAME
        if not path.is_file():
            raise ValueError(f"retained {engine} report is missing")
        observed = file_identity(path)
        expected = hashes.get(engine)
        if (not isinstance(expected, dict)
                or observed.get("sha256") != expected.get("sha256")
                or observed.get("size_bytes") != expected.get("size_bytes")):
            raise ValueError(f"retained {engine} report hash or size changed")
        current[engine] = path
    row = _find_pair_row(report, retained)
    expected_comparison = row.get("comparison")
    if not isinstance(expected_comparison, dict):
        raise ValueError("retained pair has no recorded parity result")
    # Parity is re-judged with the tolerance recorded in the retained report, not
    # with the one on the command line, and that saved value is held to the same
    # (0, 1e-6] ceiling as a fresh run. The recomputed result then has to match
    # the recorded dictionary entry for entry, so a loosened bound cannot turn a
    # stored failure into a pass.
    saved_tolerance = report.get("configuration", {}).get("tolerance")
    if not _valid_wall(saved_tolerance) or saved_tolerance <= 0 or saved_tolerance > 1e-6:
        raise ValueError("comparison report has an invalid saved tolerance")
    observed_comparison = html_report.compare_reports(
        current["python"].parent, current["cpp"].parent, tolerance=saved_tolerance)
    if observed_comparison != expected_comparison:
        raise ValueError("retained pair parity differs from comparison.html")
    retained_pair_pass = bool(observed_comparison.get("all_pass"))
    campaign_pass = report.get("all_pairs_pass") is True
    return {
        "mode": "skip-run",
        "status": "PASS" if campaign_pass and retained_pair_pass else "FAIL",
        "retained_pair": retained,
        "retained_pair_rechecked": True,
        "retained_pair_pass": retained_pair_pass,
        "historical_pairs_rechecked": 0,
        "campaign_all_pairs_pass_recorded": campaign_pass,
        "retained_pair_comparison": observed_comparison,
    }


def _copy_completed_pair(paths: dict[str, Path], results: Path) -> dict[str, Any] | None:
    sources = {engine: paths[engine] / html_report.REPORT_FILENAME
               for engine in ("python", "cpp")}
    if not all(sources[engine].is_file() for engine in ("python", "cpp")):
        return None
    for engine in ("python", "cpp"):
        destination = results / engine / html_report.REPORT_FILENAME
        destination.parent.mkdir(parents=True, exist_ok=True)
        copy2(sources[engine], destination)
    return {engine: file_identity(results / engine / html_report.REPORT_FILENAME)
            for engine in ("python", "cpp")}


def main() -> int:
    args = parse_args()
    results = args.results_dir.resolve()
    if args.skip_run:
        try:
            summary = verify_skip_run(results)
        except Exception as error:
            print(json.dumps({"mode": "skip-run", "status": "FAIL",
                              "failure": {"type": type(error).__name__,
                                          "message": str(error)[:2000]}},
                             ensure_ascii=False, allow_nan=False))
            return 1
        print(json.dumps(summary, ensure_ascii=False, allow_nan=False))
        return 0 if summary["status"] == "PASS" else 1

    if results.exists():
        raise FileExistsError(f"use a fresh --results-dir: {results}")
    if args.repeats < 1:
        raise ValueError("--repeats must be at least 1")
    if args.max_symbols < 0:
        raise ValueError("--max-symbols must be non-negative")
    if (not math.isfinite(args.tolerance) or args.tolerance <= 0
            or args.tolerance > 1e-6):
        raise ValueError("--tolerance must be finite and in (0, 1e-6]")
    args.data_dir = args.data_dir.resolve()
    results.mkdir(parents=True)

    records: dict[str, Any] = {
        "protocol": "alpha003-html-paired-v1",
        "warmups": [], "pairs": [], "requested_pairs": args.repeats,
        "cpu_index_requested": args.cpu,
        "cache_policy": "symmetric-warm-raw-csv",
        "inputs_unchanged": None,
    }
    identities: dict[str, Any] | None = None
    host: dict[str, Any] | None = None
    cpp_exe: Path | None = None
    failure: dict[str, Any] | None = None
    last_retained_pair: dict[str, Any] | None = None
    retained_hashes: dict[str, Any] = {}
    last_retained_comparison: dict[str, Any] | None = None

    try:
        cpp_exe = find_cpp(args.cpp_exe)
        identities = input_identity(args, cpp_exe)
        host = host_identity(cpp_exe)
    except Exception as error:
        failure = _failure_record("preflight", f"{type(error).__name__}: {error}")

    if failure is None and cpp_exe is not None:
        try:
            with tempfile.TemporaryDirectory(prefix=".runs-", dir=results) as temporary:
                scratch = Path(temporary).resolve()
                root = results.resolve()
                if root not in scratch.parents:
                    raise RuntimeError("temporary run directory resolved outside results root")

                def command_for(engine: str, out: Path, run_id: str) -> list[str]:
                    common = ["--data-dir", str(args.data_dir), "--out", str(out),
                              "--max-symbols", str(args.max_symbols),
                              "--end-offset", str(args.end_offset),
                              "--preprocess", args.preprocess, "--run-id", run_id]
                    if engine == "python":
                        return [sys.executable, str(PYTHON_RUNNER), *common,
                                "--fincore-root", str(args.fincore_root)]
                    return [str(cpp_exe), *common, "--report-template",
                            str((BENCHMARK_ROOT / "report_template.html").resolve())]

                def execute_pair(kind: str, index: int, order: tuple[str, str]) -> bool:
                    nonlocal failure, last_retained_pair, retained_hashes
                    nonlocal last_retained_comparison
                    row: dict[str, Any] = {"kind": kind, "index": index,
                                           "order": list(order)}
                    paths: dict[str, Path] = {}
                    engines = {"A": "python", "B": "cpp"}
                    for letter in order:
                        engine = engines[letter]
                        out = scratch / "runs" / kind / str(index) / engine
                        paths[engine] = out
                        stdout_path = scratch / "logs" / f"{kind}-{index}-{engine}.stdout"
                        stderr_path = scratch / "logs" / f"{kind}-{index}-{engine}.stderr"
                        print(f"[{kind} {index}] {engine} ...", flush=True)
                        row[engine] = run_command(
                            command_for(engine, out, f"{kind}{index}"),
                            stdout_path, stderr_path, args.cpu)
                        if _run_failed(row[engine]):
                            collection = records["warmups"] if kind == "warmup" else records["pairs"]
                            collection.append(row)
                            failure = _failure_record(
                                f"{kind}_process", "child process failed; later runs stopped",
                                engine=engine, index=index,
                                process=({key: row[engine].get(key) for key in
                                          ("returncode", "launch_error", "stdout_excerpt",
                                           "stderr_excerpt", "wall_seconds", "cpu_seconds")}),
                            )
                            return False
                        output_issue = _output_directory_issue(out)
                        if output_issue is not None:
                            row[engine]["output_contract_failure"] = output_issue
                            collection = records["warmups"] if kind == "warmup" else records["pairs"]
                            collection.append(row)
                            failure = _failure_record(
                                f"{kind}_output_contract",
                                "child output must contain exactly research.html; later runs stopped",
                                engine=engine, index=index, output=output_issue,
                            )
                            return False

                    try:
                        row["comparison"] = compare_results(
                            paths["python"], paths["cpp"], tolerance=args.tolerance)
                    except Exception as error:
                        row["comparison"] = {"all_pass": False, "checks": [],
                                             "error": f"{type(error).__name__}: {str(error)[:2000]}"}
                    for engine in ("python", "cpp"):
                        try:
                            row[engine]["timings"] = collect_timings(paths[engine])
                        except Exception as error:
                            row[engine]["timings_error"] = (
                                f"{type(error).__name__}: {str(error)[:1000]}")
                    collection = records["warmups"] if kind == "warmup" else records["pairs"]
                    collection.append(row)
                    copied = _copy_completed_pair(paths, results)
                    if copied is not None:
                        retained_hashes = copied
                        last_retained_pair = {"kind": kind, "index": index}
                        last_retained_comparison = row["comparison"]
                    if not row["comparison"].get("all_pass", False):
                        failure = _failure_record(
                            f"{kind}_comparison", "pair parity failed; later pairs stopped",
                            index=index, comparison=row["comparison"])
                        return False
                    return True

                for index, order in enumerate(measurement.warmup_order_schedule(), 1):
                    if not execute_pair("warmup", index, order):
                        break
                else:
                    for index, order in enumerate(measurement.pair_order_schedule(args.repeats), 1):
                        if not execute_pair("pair", index, order):
                            break
        except Exception as error:
            if failure is None:
                failure = _failure_record("campaign", f"{type(error).__name__}: {error}")

    if cpp_exe is not None and identities is not None:
        try:
            records["inputs_unchanged"] = input_identity(args, cpp_exe) == identities
            if not records["inputs_unchanged"] and failure is None:
                failure = _failure_record("input_identity", "benchmark inputs changed during the campaign")
        except Exception as error:
            records["inputs_unchanged"] = False
            if failure is None:
                failure = _failure_record("input_identity", f"{type(error).__name__}: {error}")

    all_comparisons = [row["comparison"] for section in ("warmups", "pairs")
                       for row in records[section] if isinstance(row.get("comparison"), dict)]
    campaign_complete = (failure is None
                         and len(records["warmups"]) == 2
                         and len(records["pairs"]) == args.repeats)
    all_pairs_pass = (campaign_complete and bool(all_comparisons)
                      and all(item.get("all_pass", False) for item in all_comparisons))
    performance = performance_report(records)
    if last_retained_pair is not None:
        for engine in ("python", "cpp"):
            path = results / engine / html_report.REPORT_FILENAME
            if path.is_file():
                retained_hashes[engine] = file_identity(path)
    observed_cpu = args.cpu
    if records["warmups"]:
        for row in records["warmups"] + records["pairs"]:
            for engine in ("python", "cpp"):
                value = row.get(engine, {}).get("cpu_index")
                if value is not None:
                    observed_cpu = value
                    break
            if observed_cpu is not None:
                break
    configuration = {
        "data_dir": str(args.data_dir), "max_symbols": args.max_symbols,
        "symbol_scope": "full" if args.max_symbols == 0 else "uniform_sample",
        "end_offset": args.end_offset, "preprocess": args.preprocess,
        "fincore_root": str(args.fincore_root), "requested_pairs": args.repeats,
        "tolerance": args.tolerance, "requested_cpu_index": args.cpu,
        "observed_cpu_index": observed_cpu,
    }
    report = {
        "schema_version": "alpha003-benchmark-html-v1",
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "outcome": "campaign complete" if campaign_complete else "campaign stopped",
        "all_pairs_pass": all_pairs_pass,
        "campaign_complete": campaign_complete,
        "configuration": configuration,
        "measurements": records,
        "performance": performance,
        "input_identities": identities,
        "host": host,
        "retained_reports": retained_hashes,
        "last_retained_pair": last_retained_pair,
        "last_retained_comparison": last_retained_comparison,
        "failure": failure,
    }
    write_comparison(results / "comparison.html", report)
    print(json.dumps({
        "status": "PASS" if all_pairs_pass else "FAIL",
        "all_pairs_pass": all_pairs_pass,
        "campaign_complete": campaign_complete,
        "completed_measured_pairs": len(records["pairs"]),
        "python_process_wall_seconds": performance["wall"]["python_seconds"],
        "cpp_process_wall_seconds": performance["wall"]["cpp_seconds"],
        "process_wall_speedup": performance["wall"]["speedup"],
        "comparison_html": str(results / "comparison.html"),
        "failure": failure,
    }, ensure_ascii=False, allow_nan=False))
    return 0 if all_pairs_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
