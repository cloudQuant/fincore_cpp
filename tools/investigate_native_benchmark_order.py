#!/usr/bin/env python3
"""Collect alternating native-only benchmark runs with host/process context.

The script is deliberately inert unless --execute is supplied. Each binary is
run in a fresh process for 3 alternating pairs per selected case, with 9
measured repeats and 2 warmups. It records each raw native JSON result, stderr,
process CPU/wall time, the native-reported per-process RSS peak, and host process
snapshots before/after each invocation. It does not compare against thresholds.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import platform
import resource
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any


CASE_NAMES = (
    "dense",
    "sparse",
    "scaling-small",
    "scaling-large",
    "scalar-sharpe",
    "batched-sharpe",
    "rolling-sharpe",
    "portfolio",
)
SOURCE_SCHEMA = "factor-benchmark-v5"
REPEATS = 9
WARMUPS = 2
PAIR_COUNT = 3
THREAD_ENVIRONMENT_KEYS = (
    "OMP_NUM_THREADS",
    "OMP_THREAD_LIMIT",
    "OMP_DYNAMIC",
    "OMP_PLACES",
    "OMP_PROC_BIND",
    "OPENBLAS_NUM_THREADS",
    "GOTO_NUM_THREADS",
    "BLAS_NUM_THREADS",
    "MKL_NUM_THREADS",
    "MKL_DYNAMIC",
    "VECLIB_MAXIMUM_THREADS",
    "VECLIB_MINIMUM_THREADS",
    "NUMEXPR_NUM_THREADS",
    "BLIS_NUM_THREADS",
)


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def binary_identity(path: Path) -> dict[str, str]:
    resolved = path.expanduser().resolve()
    if not resolved.is_file() or not os.access(resolved, os.X_OK):
        raise ValueError(f"benchmark binary must be an executable file: {resolved}")
    return {"path": str(resolved), "sha256": sha256_file(resolved)}


def process_snapshot() -> dict[str, Any]:
    """Capture a small process-table summary without command-line arguments."""
    command = ["ps", "-Ao", "pid=,ppid=,%cpu=,rss=,etime=,comm="]
    try:
        result = subprocess.run(command, check=True, capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError) as error:
        return {"error": str(error), "top_processes": []}

    rows: list[dict[str, Any]] = []
    for line in result.stdout.splitlines():
        fields = line.split(None, 5)
        if len(fields) != 6:
            continue
        try:
            row = {
                "pid": int(fields[0]),
                "ppid": int(fields[1]),
                "cpu_percent": float(fields[2]),
                "rss_kib": int(fields[3]),
                "elapsed": fields[4],
                "command_name": fields[5],
            }
        except ValueError:
            continue
        rows.append(row)
    rows.sort(key=lambda row: row["cpu_percent"], reverse=True)
    tracked = [row for row in rows if row["pid"] != os.getpid()]
    return {
        "captured_at_utc": utc_now(),
        "load_average": list(os.getloadavg()),
        "process_count": len(rows),
        "non_runner_cpu_percent_sum": sum(row["cpu_percent"] for row in tracked),
        "top_processes": tracked[:12],
    }


def child_cpu_usage() -> dict[str, float]:
    usage = resource.getrusage(resource.RUSAGE_CHILDREN)
    return {"user_seconds": usage.ru_utime, "system_seconds": usage.ru_stime}


def validate_result(
    result: dict[str, Any], case: str, expected: dict[str, Any]
) -> None:
    if result.get("source_schema") != SOURCE_SCHEMA:
        raise ValueError(f"{case}: unexpected source schema {result.get('source_schema')!r}")
    if result.get("case") != case:
        raise ValueError(f"{case}: binary returned case {result.get('case')!r}")
    if result.get("repeats") != REPEATS or result.get("warmups") != WARMUPS:
        raise ValueError(
            f"{case}: expected repeats/warmups {REPEATS}/{WARMUPS}, got "
            f"{result.get('repeats')}/{result.get('warmups')}"
        )
    if not result.get("input_digest") or not result.get("output_digest"):
        raise ValueError(f"{case}: result is missing an input or output digest")
    for field in ("input_digest", "output_digest", "workload"):
        expected_value = expected.get(field)
        actual_value = result.get(field)
        if not expected_value or not actual_value:
            raise ValueError(f"{case}: expected and actual {field} must be present")
        if actual_value != expected_value:
            raise ValueError(f"{case}: {field} differs from its validated reference")
    measurements = result.get("measurements_ms")
    if not isinstance(measurements, dict) or not measurements:
        raise ValueError(f"{case}: result is missing native stage measurements")
    for stage, measurement in measurements.items():
        samples = measurement.get("samples_ms")
        if not isinstance(samples, list) or len(samples) != REPEATS:
            raise ValueError(f"{case}/{stage}: expected {REPEATS} raw timing samples")
        for counter in ("global_new_calls", "requested_bytes"):
            counter_samples = measurement.get(counter, {}).get("samples")
            if not isinstance(counter_samples, list) or len(counter_samples) != REPEATS:
                raise ValueError(f"{case}/{stage}: expected {REPEATS} {counter} samples")
    checks = result.get("checks", {})
    failed = [name for name, value in checks.items() if value is not True]
    if failed:
        raise ValueError(f"{case}: native benchmark checks failed: {failed}")


def build_schedule(cases: list[str]) -> list[dict[str, Any]]:
    schedule: list[dict[str, Any]] = []
    for case_index, case in enumerate(cases):
        first = "baseline" if case_index % 2 == 0 else "candidate"
        for pair_index in range(PAIR_COUNT):
            pair_first = first if pair_index % 2 == 0 else (
                "candidate" if first == "baseline" else "baseline"
            )
            pair_second = "candidate" if pair_first == "baseline" else "baseline"
            schedule.append(
                {
                    "case": case,
                    "pair": pair_index + 1,
                    "order": [pair_first, pair_second],
                }
            )
    return schedule


def write_manifest(path: Path, manifest: dict[str, Any]) -> None:
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def inherited_thread_environment() -> dict[str, str | None]:
    """Record only inherited thread controls that can affect native workloads."""
    return {name: os.environ.get(name) for name in THREAD_ENVIRONMENT_KEYS}


def run_one(
    *,
    side: str,
    case: str,
    pair: int,
    binary: Path,
    expected: dict[str, Any],
    output_dir: Path,
    manifest: dict[str, Any],
) -> dict[str, Any]:
    run_dir = output_dir / "runs" / case / f"pair-{pair:02d}" / side
    run_dir.mkdir(parents=True, exist_ok=False)
    scratch_dir = output_dir / "scratch" / case / f"pair-{pair:02d}" / side
    scratch_dir.mkdir(parents=True, exist_ok=False)
    stdout_path = run_dir / "stdout.json"
    stderr_path = run_dir / "stderr.txt"
    command = [
        str(binary),
        "--case",
        case,
        "--repeats",
        str(REPEATS),
        "--warmups",
        str(WARMUPS),
        "--scratch-dir",
        str(scratch_dir),
    ]

    host_before = process_snapshot()
    usage_before = child_cpu_usage()
    started_at = utc_now()
    started = time.perf_counter()
    with stdout_path.open("w", encoding="utf-8") as stdout, stderr_path.open(
        "w", encoding="utf-8"
    ) as stderr:
        process = subprocess.Popen(command, stdout=stdout, stderr=stderr, text=True)
        process_pid = process.pid
        return_code = process.wait()
    wall_seconds = time.perf_counter() - started
    usage_after = child_cpu_usage()
    finished_at = utc_now()
    host_after = process_snapshot()

    if return_code != 0:
        raise RuntimeError(
            f"{side}/{case}/pair-{pair}: native process exited {return_code}; "
            f"see {stderr_path}"
        )
    try:
        with stdout_path.open(encoding="utf-8") as source:
            native = json.load(source)
        validate_result(native, case, expected)
    finally:
        # Keep the measured HTML/XLSX file-I/O stage isolated, but do not
        # retain rendered artifacts from these repeated diagnostic processes.
        shutil.rmtree(scratch_dir, ignore_errors=True)

    record = {
        "side": side,
        "case": case,
        "pair": pair,
        "command": command,
        "pid": process_pid,
        "started_at_utc": started_at,
        "finished_at_utc": finished_at,
        "wall_seconds": wall_seconds,
        "child_user_cpu_seconds": usage_after["user_seconds"] - usage_before["user_seconds"],
        "child_system_cpu_seconds": usage_after["system_seconds"] - usage_before["system_seconds"],
        "native_rss_peak_mb": native.get("rss_peak_mb"),
        "input_digest": native["input_digest"],
        "output_digest": native["output_digest"],
        "workload": native.get("workload"),
        "validated_reference": {
            "input_digest": expected["input_digest"],
            "output_digest": expected["output_digest"],
            "workload": expected["workload"],
        },
        "native_environment": native.get("environment"),
        "native_checks": native.get("checks"),
        "details_summary": {
            key: value
            for key, value in native.get("details", {}).items()
            if key not in {"semantic_values"}
        },
        "stage_summary": {
            stage: {
                "median_ms": sample.get("median_ms"),
                "spread_ms": sample.get("spread_ms"),
                "global_new_calls": sample["global_new_calls"].get("median"),
                "requested_bytes": sample["requested_bytes"].get("median"),
            }
            for stage, sample in native["measurements_ms"].items()
        },
        "host_before": host_before,
        "host_after": host_after,
        "raw_stdout": str(stdout_path.relative_to(output_dir)),
        "raw_stderr": str(stderr_path.relative_to(output_dir)),
        "stdout_sha256": sha256_file(stdout_path),
        "stderr_sha256": sha256_file(stderr_path),
    }
    manifest["runs"].append(record)
    write_manifest(output_dir / "investigation.json", manifest)
    return record


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-bin", type=Path, required=True)
    parser.add_argument("--candidate-bin", type=Path, required=True)
    parser.add_argument("--baseline-benchmark-json", type=Path, required=True)
    parser.add_argument("--candidate-benchmark-json", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--cases", nargs="+", choices=CASE_NAMES, default=list(CASE_NAMES))
    parser.add_argument(
        "--execute",
        action="store_true",
        help="actually launch the native workloads; without this flag only print the schedule",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if len(set(args.cases)) != len(args.cases):
        raise ValueError("--cases must not contain duplicates")
    baseline = binary_identity(args.baseline_bin)
    candidate = binary_identity(args.candidate_bin)
    baseline_json_path = args.baseline_benchmark_json.expanduser().resolve()
    candidate_json_path = args.candidate_benchmark_json.expanduser().resolve()
    with baseline_json_path.open(encoding="utf-8") as source:
        baseline_run = json.load(source)
    with candidate_json_path.open(encoding="utf-8") as source:
        candidate_run = json.load(source)
    for label, run, binary in (
        ("baseline", baseline_run, baseline),
        ("candidate", candidate_run, candidate),
    ):
        if run.get("native_binary_sha256") != binary["sha256"]:
            raise ValueError(f"{label} binary hash does not match its benchmark JSON")
        if run.get("source_schema") != SOURCE_SCHEMA:
            raise ValueError(f"{label} benchmark JSON has an unexpected source schema")
    if baseline_run.get("build_compatibility") != candidate_run.get("build_compatibility"):
        raise ValueError("baseline and candidate build compatibility identities differ")
    if baseline_run.get("platform") != candidate_run.get("platform"):
        raise ValueError("baseline and candidate platform identities differ")
    if baseline_run.get("digest_scheme") != candidate_run.get("digest_scheme"):
        raise ValueError("baseline and candidate digest schemes differ")
    baseline_sources = baseline_run.get("source_identity", {})
    candidate_sources = candidate_run.get("source_identity", {})
    if baseline_sources.get("python") != candidate_sources.get("python"):
        raise ValueError("baseline and candidate Python source identities differ")
    for field in ("git_head", "tracked_scope_dirty", "sha256"):
        if baseline_sources.get("runner", {}).get(field) != candidate_sources.get(
            "runner", {}
        ).get(field):
            raise ValueError("baseline and candidate runner source identities differ")
    for case in CASE_NAMES:
        left = baseline_run.get("cases", {}).get(case, {})
        right = candidate_run.get("cases", {}).get(case, {})
        if left.get("native", {}).get("status") != "PASS":
            raise ValueError(f"baseline benchmark JSON has no native PASS for {case}")
        if right.get("native", {}).get("status") != "PASS":
            raise ValueError(f"candidate benchmark JSON has no native PASS for {case}")
        if left["native"].get("input_digest") != right["native"].get("input_digest"):
            raise ValueError(f"baseline/candidate input digest differs for {case}")
        for label, record in (("baseline", left), ("candidate", right)):
            if record.get("python", {}).get("status") != "PASS":
                raise ValueError(f"{label} benchmark JSON has no Python PASS for {case}")
            cross = record.get("cross_language", {})
            if cross.get("status") not in (None, "PASS"):
                raise ValueError(f"{label} cross-language status is not PASS for {case}")
            if cross.get("full_pipeline") == "NOT_COMPARABLE" and (
                cross.get("input_digest") != "PASS"
                or cross.get("shared_factor_returns") != "PASS"
            ):
                raise ValueError(f"{label} comparable panel output check failed for {case}")
        for label, native in (("baseline", left["native"]), ("candidate", right["native"])):
            if not native.get("input_digest") or not native.get("output_digest"):
                raise ValueError(f"{label} benchmark JSON is missing a digest for {case}")
            if not native.get("workload"):
                raise ValueError(f"{label} benchmark JSON is missing workload identity for {case}")
    schedule = build_schedule(args.cases)
    print(json.dumps({
        "mode": "EXECUTE" if args.execute else "PREVIEW_ONLY",
        "cases": args.cases,
        "pair_count_per_case": PAIR_COUNT,
        "repeats": REPEATS,
        "warmups": WARMUPS,
        "baseline_binary": baseline,
        "candidate_binary": candidate,
        "baseline_benchmark_json": {
            "path": str(baseline_json_path),
            "sha256": sha256_file(baseline_json_path),
        },
        "candidate_benchmark_json": {
            "path": str(candidate_json_path),
            "sha256": sha256_file(candidate_json_path),
        },
        "schedule": schedule,
    }, indent=2, sort_keys=True))

    if not args.execute:
        return 0
    if platform.system() != "Darwin":
        raise RuntimeError("process snapshots and rusage format are currently macOS-specific")
    output_dir = args.output_dir.expanduser().resolve()
    if output_dir.exists():
        raise FileExistsError(f"refusing to overwrite existing output directory: {output_dir}")
    output_dir.mkdir(parents=True)

    manifest: dict[str, Any] = {
        "schema_version": 1,
        "source_schema": SOURCE_SCHEMA,
        "created_at_utc": utc_now(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "python": sys.version,
        "fixed_parameters": {"pairs_per_case": PAIR_COUNT, "repeats": REPEATS, "warmups": WARMUPS},
        "baseline_binary": baseline,
        "candidate_binary": candidate,
        "baseline_benchmark_json": {
            "path": str(baseline_json_path),
            "sha256": sha256_file(baseline_json_path),
        },
        "candidate_benchmark_json": {
            "path": str(candidate_json_path),
            "sha256": sha256_file(candidate_json_path),
        },
        "inherited_thread_environment": inherited_thread_environment(),
        "schedule": schedule,
        "status": "COLLECTING",
        "performance_acceptance": "NOT_EVALUATED",
        "runs": [],
    }
    write_manifest(output_dir / "investigation.json", manifest)

    by_case_side_pair: dict[tuple[str, str, int], dict[str, Any]] = {}
    try:
        for item in schedule:
            case = item["case"]
            pair = item["pair"]
            for side in item["order"]:
                binary = Path((baseline if side == "baseline" else candidate)["path"])
                reference_run = baseline_run if side == "baseline" else candidate_run
                expected = reference_run["cases"][case]["native"]
                record = run_one(
                    side=side,
                    case=case,
                    pair=pair,
                    binary=binary,
                    expected=expected,
                    output_dir=output_dir,
                    manifest=manifest,
                )
                by_case_side_pair[(case, side, pair)] = record
            left = by_case_side_pair[(case, "baseline", pair)]
            right = by_case_side_pair[(case, "candidate", pair)]
            if left["input_digest"] != right["input_digest"]:
                raise ValueError(
                    f"{case}/pair-{pair}: baseline/candidate input digests differ"
                )
        manifest["status"] = "COLLECTED_VALIDATED"
        manifest["finished_at_utc"] = utc_now()
        write_manifest(output_dir / "investigation.json", manifest)
    except Exception as error:
        manifest["status"] = "COLLECTION_FAILED"
        manifest["error"] = f"{type(error).__name__}: {error}"
        manifest["finished_at_utc"] = utc_now()
        write_manifest(output_dir / "investigation.json", manifest)
        raise
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
