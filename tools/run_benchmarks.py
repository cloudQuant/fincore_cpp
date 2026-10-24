#!/usr/bin/env python3
"""Run isolated fincore C++ and Python performance workloads.

Each workload runs in its own native and Python process. The JSON result and
all subprocess logs are written to a new output directory. Baselines are only
created when ``--create-baseline`` is supplied and are never overwritten.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import hashlib
import importlib.metadata
import json
import math
import os
import platform
import resource
import statistics
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
CONDA = Path("/Users/yunjinqi/opt/anaconda3/bin/conda")
SOURCE_SCHEMA = "factor-benchmark-v5"
DIGEST_SCHEME = "fnv1a64-canonical-decimal-9"
RUNNER_SCHEMA = 2
PINNED_PYTHON_SOURCE_HEAD = "4b8f31b1b59707480d5db9603704d83bbf7e6946"
DEFAULT_PYTHON_SOURCE = ROOT.parent / "fincore"
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
PANEL_CASES: dict[str, dict[str, int]] = {
    "dense": {"dates": 250, "assets": 100, "stride": 1},
    "sparse": {"dates": 250, "assets": 1000, "stride": 20},
    "scaling-small": {"dates": 50, "assets": 50, "stride": 1},
    "scaling-large": {"dates": 100, "assets": 100, "stride": 1},
    "portfolio": {"dates": 250, "assets": 100, "stride": 1},
}
RETURNS_CASES: dict[str, dict[str, int]] = {
    "scalar-sharpe": {"rows": 100_000, "columns": 1, "window": 0},
    "batched-sharpe": {"rows": 50_000, "columns": 16, "window": 0},
    "rolling-sharpe": {"rows": 50_000, "columns": 1, "window": 252},
}
PANEL_CONFIG = {
    "long_short": True,
    "group_neutral": False,
    "equal_weight": False,
    "by_group": False,
    "periods": ["1D", "5D"],
    "turnover_periods": [1, 5],
    "time_aggregation": ["M"],
    "include_portfolio_inputs": True,
    "portfolio_benchmark_period": "1D",
}


class FNV64:
    """Small portable checksum used for cross-language workload parity."""

    def __init__(self) -> None:
        self.value = 14695981039346656037

    def add(self, data: bytes) -> None:
        for byte in data:
            self.value ^= byte
            self.value = (self.value * 1099511628211) & 0xFFFFFFFFFFFFFFFF

    def u64(self, value: int) -> None:
        self.add(struct.pack("<Q", value))

    def i64(self, value: int) -> None:
        self.add(struct.pack("<q", value))

    def i32(self, value: int) -> None:
        self.add(struct.pack("<i", value))

    def double(self, value: float) -> None:
        self.add(struct.pack("<d", float(value)))

    def string(self, value: str) -> None:
        self.add(value.encode("utf-8"))

    def hexdigest(self) -> str:
        return f"fnv1a64:{self.value:016x}"


def canonical_number(value: float) -> str:
    if math.isnan(value):
        return "nan"
    if math.isinf(value):
        return "-inf" if value < 0 else "inf"
    if value == 0.0:
        value = 0.0
    return f"{value:.9f}"


def values_digest(values: Any) -> str:
    digest = FNV64()
    for value in values:
        digest.string(canonical_number(float(value)))
        digest.add(b";")
    return digest.hexdigest()


def median_and_spread(values: list[float]) -> dict[str, Any]:
    if not values or any(not math.isfinite(value) for value in values):
        raise ValueError("measurements must be a nonempty finite sequence")
    return {
        "samples_ms": values,
        "median_ms": statistics.median(values),
        "min_ms": min(values),
        "max_ms": max(values),
        "spread_ms": max(values) - min(values),
    }


def _git_value(path: Path, *args: str) -> str | None:
    try:
        result = subprocess.run(
            ["git", "-C", str(path), *args],
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip()


def source_identity(path: Path, include: tuple[str, ...]) -> dict[str, Any]:
    digest = hashlib.sha256()
    for relative in include:
        item = path / relative
        candidates = [item] if item.is_file() else sorted(item.rglob("*"))
        for candidate in candidates:
            if (
                not candidate.is_file()
                or ".git" in candidate.parts
                or candidate.suffix not in {".py", ".cpp", ".hpp", ".h", ".cc", ".cxx"}
            ):
                continue
            rel = candidate.relative_to(path).as_posix()
            digest.update(rel.encode("utf-8"))
            digest.update(b"\0")
            digest.update(candidate.read_bytes())
            digest.update(b"\0")
    head = _git_value(path, "rev-parse", "HEAD")
    status = _git_value(path, "status", "--porcelain", "--", *include)
    return {
        "path": str(path.resolve()),
        "git_head": head,
        "tracked_scope_dirty": bool(status),
        "sha256": digest.hexdigest(),
    }


def pinned_python_source_identity(path: Path) -> dict[str, Any]:
    identity = source_identity(path, ("fincore",))
    if identity.get("git_head") != PINNED_PYTHON_SOURCE_HEAD:
        raise ValueError(
            "Python source must be pinned to " + PINNED_PYTHON_SOURCE_HEAD
            + f"; found {identity.get('git_head')}"
        )
    if identity.get("tracked_scope_dirty") is not False:
        raise ValueError("pinned Python source fincore/ tree must be clean")
    return identity


def imported_fincore_module_paths(source: Path) -> dict[str, str]:
    root = source.resolve()
    locations: dict[str, str] = {}
    for name, module in sorted(sys.modules.items()):
        if name != "fincore" and not name.startswith("fincore."):
            continue
        module_file = getattr(module, "__file__", None)
        if not module_file:
            raise RuntimeError(f"imported module {name} has no verifiable file path")
        resolved = Path(module_file).resolve()
        try:
            resolved.relative_to(root)
        except ValueError as error:
            raise RuntimeError(
                f"imported module {name} is outside --python-source: {resolved}"
            ) from error
        locations[name] = str(resolved)
    required = {
        "fincore.factor_analysis.analysis",
        "fincore.factor_analysis.performance",
        "fincore.metrics.ratios",
        "fincore.metrics.rolling",
    }
    missing = sorted(required - locations.keys())
    if missing:
        raise RuntimeError(f"required fincore modules were not imported: {missing}")
    return locations


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _consistent_summary_value(name: str, actual: Any, expected: float) -> str | None:
    if not _finite_number(actual):
        return f"measurement {name} summary is missing or non-finite"
    if float(actual) < 0.0:
        return f"measurement {name} summary is negative"
    if not math.isclose(float(actual), expected, rel_tol=1e-12, abs_tol=1e-12):
        return f"measurement {name} summary does not match samples"
    return None


def parse_cache(path: Path) -> dict[str, str]:
    cache = path / "CMakeCache.txt"
    values: dict[str, str] = {}
    if not cache.is_file():
        return values
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        name, value = line.split("=", 1)
        key = name.split(":", 1)[0]
        if key in {
            "CMAKE_BUILD_TYPE",
            "CMAKE_CXX_COMPILER",
            "CMAKE_CXX_COMPILER_ID",
            "CMAKE_CXX_COMPILER_VERSION",
            "CMAKE_CXX_FLAGS",
            "CMAKE_CXX_FLAGS_RELEASE",
            "CMAKE_OSX_ARCHITECTURES",
            "CMAKE_OSX_DEPLOYMENT_TARGET",
            "FINCORE_ENABLE_ASAN",
            "FINCORE_ENABLE_UBSAN",
            "FINCORE_DATAFRAME_SOURCE_DIR",
        }:
            values[key] = value
    compiler = values.get("CMAKE_CXX_COMPILER")
    if compiler:
        try:
            version = subprocess.run(
                [compiler, "--version"], check=True, capture_output=True, text=True
            ).stdout.splitlines()
        except (OSError, subprocess.CalledProcessError):
            version = []
        values["compiler_version_text"] = "\n".join(version[:2])
    return values


def platform_identity() -> dict[str, str]:
    return {
        "system": platform.system(),
        "machine": platform.machine(),
        "release": platform.release(),
    }


def validate_measurements(native: dict[str, Any], repeats: int) -> list[str]:
    reasons: list[str] = []
    measurements = native.get("measurements_ms")
    if not isinstance(measurements, dict) or not measurements:
        return ["native measurement map is missing"]
    for name, summary in measurements.items():
        if not isinstance(summary, dict):
            reasons.append(f"measurement {name} is malformed")
            continue
        samples = summary.get("samples_ms")
        if not isinstance(samples, list) or len(samples) != repeats:
            reasons.append(f"measurement {name} does not contain {repeats} samples")
            continue
        numeric = [value for value in samples if _finite_number(value)]
        if len(numeric) != len(samples) or any(not math.isfinite(value) for value in numeric):
            reasons.append(f"measurement {name} contains missing or non-finite samples")
            continue
        if any(float(value) < 0.0 for value in numeric):
            reasons.append(f"measurement {name} contains negative time samples")
        expected = {
            "median_ms": float(statistics.median(numeric)),
            "min_ms": float(min(numeric)),
            "max_ms": float(max(numeric)),
            "spread_ms": float(max(numeric) - min(numeric)),
        }
        for field, computed in expected.items():
            issue = _consistent_summary_value(f"{name}.{field}", summary.get(field), computed)
            if issue:
                reasons.append(issue)
        invocations = summary.get("stage_invocations_per_sample")
        if not isinstance(invocations, int) or isinstance(invocations, bool) or invocations != 1:
            reasons.append(f"measurement {name} stage invocation count must be exactly 1 per sample")
        for metric in ("global_new_calls", "requested_bytes"):
            allocation_summary = summary.get(metric)
            if not isinstance(allocation_summary, dict):
                reasons.append(f"measurement {name} {metric} is missing")
                continue
            allocation_samples = allocation_summary.get("samples")
            if not isinstance(allocation_samples, list) or len(allocation_samples) != repeats:
                reasons.append(f"measurement {name} {metric} does not contain {repeats} samples")
                continue
            allocation_values = [
                value for value in allocation_samples
                if isinstance(value, int) and not isinstance(value, bool) and value >= 0
            ]
            if len(allocation_values) != len(allocation_samples):
                reasons.append(f"measurement {name} {metric} samples must be nonnegative integers")
                continue
            allocation_numeric = [float(value) for value in allocation_values]
            allocation_expected = {
                "median": float(statistics.median(allocation_numeric)),
                "min": float(min(allocation_numeric)),
                "max": float(max(allocation_numeric)),
                "spread": float(max(allocation_numeric) - min(allocation_numeric)),
            }
            for field, computed in allocation_expected.items():
                actual = allocation_summary.get(field)
                if field != "median" and (not isinstance(actual, int) or isinstance(actual, bool)):
                    reasons.append(f"measurement {name} {metric}.{field} must be an integer")
                issue = _consistent_summary_value(
                    f"{name}.{metric}.{field}", actual, computed
                )
                if issue:
                    reasons.append(issue)
    return reasons


def validate_rss_samples(native: dict[str, Any], label: str) -> tuple[list[float], list[str]]:
    reasons: list[str] = []
    samples = native.get("rss_samples_mb")
    if not isinstance(samples, list) or len(samples) < 3:
        return [], [f"{label} RSS has fewer than 3 independent process samples"]
    if any(not _finite_number(value) for value in samples):
        return [], [f"{label} RSS samples contain missing or non-finite values"]
    numeric = [float(value) for value in samples]
    if any(value < 0.0 for value in numeric):
        reasons.append(f"{label} RSS samples contain negative values")
    expected = {
        "rss_median_mb": float(statistics.median(numeric)),
        "rss_spread_mb": float(max(numeric) - min(numeric)),
    }
    for field, computed in expected.items():
        issue = _consistent_summary_value(f"{label}.{field}", native.get(field), computed)
        if issue:
            reasons.append(issue)
    return numeric, reasons


def _finite_number(value: Any) -> bool:
    return not isinstance(value, bool) and isinstance(value, (int, float)) and math.isfinite(value)


def _nonempty_identity_value(value: Any, *, mapping: bool = False) -> bool:
    if mapping:
        return isinstance(value, dict) and bool(value)
    return isinstance(value, str) and bool(value.strip())


def _is_sha256(value: Any) -> bool:
    return (
        isinstance(value, str)
        and len(value) == 64
        and all(character in "0123456789abcdef" for character in value)
    )


def compare_results(current: dict[str, Any], baseline: dict[str, Any]) -> dict[str, Any]:
    """Fail closed on incompatibility, then flag regressions beyond noise."""

    reasons: list[str] = []
    context_fields = ("schema_version", "source_schema", "digest_scheme", "platform", "build_compatibility")
    for field in context_fields:
        if field not in current or field not in baseline:
            reasons.append(f"compatibility field missing: {field}")
        elif current[field] != baseline[field]:
            reasons.append(f"incompatible {field}")
    for label, result in (("current", current), ("baseline", baseline)):
        for field in ("source_schema", "digest_scheme"):
            if not _nonempty_identity_value(result.get(field)):
                reasons.append(f"{label} {field} is missing or empty")
        for field in ("platform", "build_compatibility"):
            if not _nonempty_identity_value(result.get(field), mapping=True):
                reasons.append(f"{label} {field} is missing or empty")
    source_identities: dict[str, dict[str, Any]] = {}
    for label, result in (("current", current), ("baseline", baseline)):
        identities = result.get("source_identity")
        if not isinstance(identities, dict):
            reasons.append(f"{label} source identity is missing")
            continue
        python_identity = identities.get("python")
        runner_identity = identities.get("runner")
        if not isinstance(python_identity, dict):
            reasons.append(f"{label} pinned Python source identity is missing")
        else:
            if python_identity.get("git_head") != PINNED_PYTHON_SOURCE_HEAD:
                reasons.append(f"{label} Python source is not pinned to {PINNED_PYTHON_SOURCE_HEAD}")
            if python_identity.get("tracked_scope_dirty") is not False:
                reasons.append(f"{label} pinned Python source is dirty or cleanliness is unknown")
            if not _is_sha256(python_identity.get("sha256")):
                reasons.append(f"{label} pinned Python source hash is missing")
        if not isinstance(runner_identity, dict) or not _is_sha256(runner_identity.get("sha256")):
            reasons.append(f"{label} runner source hash is missing")
        else:
            source_identities[label] = {
                "python_sha256": python_identity.get("sha256") if isinstance(python_identity, dict) else None,
                "runner_sha256": runner_identity.get("sha256"),
            }
        binary_sha = result.get("native_binary_sha256")
        if not _is_sha256(binary_sha):
            reasons.append(f"{label} native binary hash is missing")
        if isinstance(runner_identity, dict) and result.get("runner_sha256") != runner_identity.get("sha256"):
            reasons.append(f"{label} runner hash does not match source identity")
    if "current" in source_identities and "baseline" in source_identities:
        if source_identities["current"]["python_sha256"] != source_identities["baseline"]["python_sha256"]:
            reasons.append("incompatible pinned Python source hash")
        if source_identities["current"]["runner_sha256"] != source_identities["baseline"]["runner_sha256"]:
            reasons.append("incompatible benchmark runner source hash")
    if current.get("suite_status") != "PASS":
        reasons.append("current suite status is not PASS")
    if baseline.get("suite_status") != "PASS":
        reasons.append("baseline suite status is not PASS")
    current_cases = current.get("cases")
    baseline_cases = baseline.get("cases")
    if not isinstance(current_cases, dict) or not isinstance(baseline_cases, dict):
        return {"status": "FAIL", "reasons": reasons + ["case map missing"], "cases": {}}
    if set(current_cases) != set(baseline_cases):
        reasons.append("case set mismatch")
    comparisons: dict[str, Any] = {}
    for case_name in sorted(set(current_cases) | set(baseline_cases)):
        current_case = current_cases.get(case_name)
        baseline_case = baseline_cases.get(case_name)
        case_reasons: list[str] = []
        if not isinstance(current_case, dict) or not isinstance(baseline_case, dict):
            case_reasons.append("case missing from one result")
            comparisons[case_name] = {"status": "FAIL", "reasons": case_reasons}
            reasons.extend(f"{case_name}: {reason}" for reason in case_reasons)
            continue
        current_native = current_case.get("native")
        baseline_native = baseline_case.get("native")
        if not isinstance(current_native, dict) or not isinstance(baseline_native, dict):
            case_reasons.append("native result missing")
        else:
            identity_fields = ("case", "source_schema", "input_digest")
            for label, value in (("current", current_native), ("baseline", baseline_native)):
                for field in identity_fields:
                    if not _nonempty_identity_value(value.get(field)):
                        case_reasons.append(f"{label} native {field} is missing or empty")
                if not _nonempty_identity_value(value.get("workload"), mapping=True):
                    case_reasons.append(f"{label} native workload is missing or empty")
            if current_native.get("case") != case_name or baseline_native.get("case") != case_name:
                case_reasons.append("native case identity does not match case map key")
            for field in ("case", "source_schema", "workload", "input_digest"):
                if current_native.get(field) != baseline_native.get(field):
                    case_reasons.append(f"incompatible native {field}")
            for field in ("warmups", "repeats"):
                if current_native.get(field) != baseline_native.get(field):
                    case_reasons.append(f"incompatible native {field}")
            if current_native.get("status") != "PASS" or baseline_native.get("status") != "PASS":
                case_reasons.append("native run status is not PASS")
            for label, value in (("current", current_native), ("baseline", baseline_native)):
                repeats = value.get("repeats")
                if not isinstance(repeats, int) or repeats < 7:
                    case_reasons.append(f"{label} run has fewer than 7 repeats")
                    continue
                case_reasons.extend(
                    f"{label} {reason}"
                    for reason in validate_measurements(value, repeats)
                )
            current_measurements = current_native.get("measurements_ms", {})
            baseline_measurements = baseline_native.get("measurements_ms", {})
            if isinstance(current_measurements, dict) and isinstance(baseline_measurements, dict):
                if set(current_measurements) != set(baseline_measurements):
                    case_reasons.append("measurement stage set mismatch")
                for stage in sorted(set(current_measurements) & set(baseline_measurements)):
                    now = current_measurements.get(stage, {})
                    before = baseline_measurements.get(stage, {})
                    if not isinstance(now, dict) or not isinstance(before, dict):
                        case_reasons.append(f"malformed measurement stage {stage}")
                        continue
                    now_median = now.get("median_ms")
                    before_median = before.get("median_ms")
                    now_spread = now.get("spread_ms")
                    before_spread = before.get("spread_ms")
                    if not all(map(_finite_number, (now_median, before_median, now_spread, before_spread))):
                        case_reasons.append(f"missing or non-finite comparison values for {stage}")
                        continue
                    delta = float(now_median) - float(before_median)
                    noise = max(float(now_spread), float(before_spread))
                    threshold = max(0.10 * float(before_median), noise)
                    if delta > threshold:
                        case_reasons.append(
                            f"{stage} median regression {delta:.6g} ms exceeds "
                            f"10%/noise threshold {threshold:.6g} ms"
                        )
                    for counter_name in ("global_new_calls", "requested_bytes"):
                        now_counter = now.get(counter_name)
                        before_counter = before.get(counter_name)
                        if not isinstance(now_counter, dict) or not isinstance(before_counter, dict):
                            case_reasons.append(f"{stage} {counter_name} comparison data is missing")
                            continue
                        now_counter_median = now_counter.get("median")
                        before_counter_median = before_counter.get("median")
                        now_counter_spread = now_counter.get("spread")
                        before_counter_spread = before_counter.get("spread")
                        if not all(map(_finite_number, (
                            now_counter_median, before_counter_median,
                            now_counter_spread, before_counter_spread,
                        ))):
                            case_reasons.append(f"{stage} {counter_name} comparison data is non-finite")
                            continue
                        counter_delta = float(now_counter_median) - float(before_counter_median)
                        counter_noise = max(float(now_counter_spread), float(before_counter_spread))
                        counter_threshold = max(0.10 * float(before_counter_median), counter_noise)
                        if counter_delta > counter_threshold:
                            case_reasons.append(
                                f"{stage} {counter_name} median regression {counter_delta:.6g} "
                                f"exceeds 10%/noise threshold {counter_threshold:.6g}"
                            )
            current_rss, current_rss_reasons = validate_rss_samples(current_native, "current")
            baseline_rss, baseline_rss_reasons = validate_rss_samples(baseline_native, "baseline")
            case_reasons.extend(current_rss_reasons)
            case_reasons.extend(baseline_rss_reasons)
            if current_rss and baseline_rss and not current_rss_reasons and not baseline_rss_reasons:
                now_median = statistics.median(current_rss)
                before_median = statistics.median(baseline_rss)
                noise = max(max(current_rss) - min(current_rss), max(baseline_rss) - min(baseline_rss))
                threshold = max(0.10 * before_median, noise)
                if now_median - before_median > threshold:
                    case_reasons.append(
                        f"RSS median regression {now_median - before_median:.6g} MiB "
                        f"exceeds 10%/noise threshold {threshold:.6g} MiB"
                    )
            current_details = current_native.get("details")
            baseline_details = baseline_native.get("details")
            if not isinstance(current_details, dict) or not isinstance(baseline_details, dict):
                case_reasons.append("native semantic output details are missing")
            elif case_name in RETURNS_CASES:
                semantic = compare_semantic_values(
                    current_details.get("semantic_values"),
                    baseline_details.get("semantic_values"),
                )
                if semantic["status"] != "PASS":
                    case_reasons.append(
                        "native semantic output differs from baseline: "
                        + str(semantic.get("reason"))
                    )
            elif case_name == "portfolio":
                for field in ("weights_digest", "returns_digest"):
                    if not _nonempty_identity_value(current_details.get(field)) or not _nonempty_identity_value(baseline_details.get(field)):
                        case_reasons.append(f"native portfolio {field} is missing or empty")
                    elif current_details.get(field) != baseline_details.get(field):
                        case_reasons.append(f"native portfolio {field} differs from baseline")
            elif case_name in PANEL_CASES:
                current_returns = current_details.get("factor_returns_digest")
                baseline_returns = baseline_details.get("factor_returns_digest")
                if not _nonempty_identity_value(current_returns) or not _nonempty_identity_value(baseline_returns):
                    case_reasons.append("native factor-return digest is missing or empty")
                elif current_returns != baseline_returns:
                    case_reasons.append("native factor-return digest differs from baseline")
        comparisons[case_name] = {
            "status": "FAIL" if case_reasons else "PASS",
            "reasons": case_reasons,
        }
        reasons.extend(f"{case_name}: {reason}" for reason in case_reasons)
    return {
        "status": "FAIL" if reasons else "PASS",
        "reasons": reasons,
        "cases": comparisons,
    }


def _json_load(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"), parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
    except (OSError, json.JSONDecodeError, ValueError) as error:
        raise ValueError(f"could not read valid JSON from {path}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"JSON root must be an object: {path}")
    return value


def _new_output_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=False)
    (path / "logs").mkdir()
    (path / "scratch").mkdir()


def _write_log(path: Path, result: subprocess.CompletedProcess[str]) -> None:
    path.write_text(
        "$ " + " ".join(result.args) + "\n"
        + "[exit_code] " + str(result.returncode) + "\n"
        + "[stdout]\n" + result.stdout
        + "\n[stderr]\n" + result.stderr,
        encoding="utf-8",
    )


def _run_logged(command: list[str], log_path: Path, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, capture_output=True, text=True, env=env)
    _write_log(log_path, result)
    return result


def _build_native(args: argparse.Namespace, output_dir: Path) -> tuple[Path, Path, dict[str, str]]:
    if args.native_bin:
        binary = Path(args.native_bin).resolve()
        build_dir = Path(os.path.abspath(args.build_dir)) if args.build_dir else binary.parent
        return binary, build_dir, parse_cache(build_dir)
    if args.build_dir:
        build_dir = Path(os.path.abspath(args.build_dir))
        if not str(build_dir).startswith("/tmp/fincore-phase87-"):
            raise ValueError("--build-dir must be a private /tmp/fincore-phase87-* path")
        build_dir.mkdir(parents=True, exist_ok=True)
    else:
        build_dir = Path(tempfile.mkdtemp(prefix="fincore-phase87-", dir="/tmp"))
    configure = [
        "cmake", "-S", str(ROOT), "-B", str(build_dir),
        "-DCMAKE_BUILD_TYPE=Release", "-DFINCORE_BUILD_TESTING=OFF",
        "-DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame",
    ]
    configured = _run_logged(configure, output_dir / "logs" / "cmake-configure.log")
    if configured.returncode != 0:
        raise RuntimeError(f"CMake configure failed; see {output_dir / 'logs' / 'cmake-configure.log'}")
    build = ["cmake", "--build", str(build_dir), "--target", "fincore_factor_benchmark", "-j2"]
    built = _run_logged(build, output_dir / "logs" / "cmake-build.log")
    if built.returncode != 0:
        raise RuntimeError(f"native benchmark build failed; see {output_dir / 'logs' / 'cmake-build.log'}")
    binary = build_dir / "fincore_factor_benchmark"
    if not binary.is_file():
        raise RuntimeError(f"native benchmark binary missing: {binary}")
    return binary, build_dir, parse_cache(build_dir)


def _native_worker(
    binary: Path,
    case_name: str,
    output_dir: Path,
    repeats: int,
    warmups: int,
    suffix: str,
) -> dict[str, Any]:
    scratch = output_dir / "scratch" / case_name
    scratch.mkdir(parents=True, exist_ok=True)
    command = [
        str(binary), "--case", case_name, "--repeats", str(repeats),
        "--warmups", str(warmups), "--scratch-dir", str(scratch),
    ]
    log_path = output_dir / "logs" / f"native-{case_name}-{suffix}.log"
    result = _run_logged(command, log_path)
    if result.returncode != 0:
        raise RuntimeError(f"native {case_name} failed; see {log_path}")
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError(f"native {case_name} did not emit one JSON object; see {log_path}")
    parsed = json.loads(lines[0])
    if not isinstance(parsed, dict):
        raise RuntimeError(f"native {case_name} result was not a JSON object")
    if parsed.get("case") != case_name:
        raise RuntimeError(f"native {case_name} reported a different case identity")
    for field in ("source_schema", "input_digest", "output_digest"):
        if not _nonempty_identity_value(parsed.get(field)):
            raise RuntimeError(f"native {case_name} omitted {field}")
    if not _nonempty_identity_value(parsed.get("workload"), mapping=True):
        raise RuntimeError(f"native {case_name} omitted its workload description")
    measurement_errors = validate_measurements(parsed, repeats)
    if measurement_errors:
        raise RuntimeError(f"native {case_name} measurements are invalid: {measurement_errors}")
    rss_peak = parsed.get("rss_peak_mb")
    if not _finite_number(rss_peak) or float(rss_peak) < 0.0:
        raise RuntimeError(f"native {case_name} RSS peak is missing, non-finite, or negative")
    _validate_native_instrumentation(parsed, case_name, repeats)
    return parsed


def _validate_native_instrumentation(
    native: dict[str, Any], case_name: str, repeats: int
) -> None:
    allocation = native.get("allocation_instrumentation")
    if not isinstance(allocation, dict):
        raise RuntimeError(f"native {case_name} allocation instrumentation description is missing")
    if allocation.get("counted_calls") != "successful global operator new/new[] and aligned forms":
        raise RuntimeError(f"native {case_name} allocation call scope is unknown")
    if allocation.get("bytes") != "cumulative requested bytes, not live bytes or RSS":
        raise RuntimeError(f"native {case_name} allocation byte scope is unknown")

    details = native.get("details")
    if not isinstance(details, dict):
        raise RuntimeError(f"native {case_name} details are missing")
    if case_name in PANEL_CASES and case_name != "portfolio":
        copy_count = details.get("model_copy_invocations_per_measured_sample")
        if not isinstance(copy_count, int) or isinstance(copy_count, bool) or copy_count != 1:
            raise RuntimeError(f"native {case_name} must perform exactly one model copy per measured sample")
        diagnostics = details.get("factor_compute_diagnostics")
        if not isinstance(diagnostics, dict) or not isinstance(diagnostics.get("available"), bool):
            raise RuntimeError(f"native {case_name} kernel diagnostic availability is missing")
        if diagnostics["available"]:
            for field in (
                "weight_kernel_calls_per_analyze",
                "information_coefficient_kernel_calls_per_analyze",
            ):
                values = diagnostics.get(field)
                if not isinstance(values, list) or len(values) != repeats:
                    raise RuntimeError(f"native {case_name} {field} must have one entry per measured sample")
                if any(not isinstance(value, int) or isinstance(value, bool) or value <= 0 for value in values):
                    raise RuntimeError(f"native {case_name} {field} must contain positive integer counts")


def _python_worker(
    case_name: str,
    python_source: Path,
    output_dir: Path,
    repeats: int,
    warmups: int,
) -> dict[str, Any]:
    if not CONDA.is_file():
        raise RuntimeError(f"required Anaconda conda executable is missing: {CONDA}")
    worker_output = output_dir / "scratch" / f"python-{case_name}.json"
    scratch = output_dir / "scratch" / f"python-{case_name}"
    scratch.mkdir(parents=True, exist_ok=True)
    command = [
        str(CONDA), "run", "-n", "base", "--no-capture-output", "python", str(Path(__file__).resolve()),
        "--_python-worker", "--worker-case", case_name,
        "--python-source", str(python_source), "--worker-output", str(worker_output),
        "--worker-scratch", str(scratch), "--repeats", str(repeats), "--warmups", str(warmups),
    ]
    env = os.environ.copy()
    current_pythonpath = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = str(python_source) + (os.pathsep + current_pythonpath if current_pythonpath else "")
    log_path = output_dir / "logs" / f"python-{case_name}.log"
    result = _run_logged(command, log_path, env)
    if result.returncode != 0:
        raise RuntimeError(f"Python {case_name} failed; see {log_path}")
    return _json_load(worker_output)


def run_suite(args: argparse.Namespace) -> dict[str, Any]:
    output_dir = Path(args.output_dir).resolve()
    _new_output_dir(output_dir)
    python_source = Path(args.python_source).resolve()
    if not python_source.is_dir() or not (python_source / "fincore").is_dir():
        raise ValueError(f"--python-source must be the root of the pinned Python source tree: {python_source}")
    py_source = pinned_python_source_identity(python_source)
    binary, build_dir, build_cache = _build_native(args, output_dir)
    cpp_source = source_identity(ROOT, ("include", "src", "benchmarks/factor_pipeline_benchmark.cpp"))
    runner_source = source_identity(ROOT, ("tools/run_benchmarks.py",))
    dataframe_path = Path(build_cache.get(
        "FINCORE_DATAFRAME_SOURCE_DIR", "/Users/yunjinqi/Documents/new_projects/DataFrame"
    )).resolve()
    dataframe_source = (
        source_identity(dataframe_path, ("include", "src"))
        if dataframe_path.is_dir()
        else None
    )
    compiler = {
        "path": build_cache.get("CMAKE_CXX_COMPILER"),
        "id": build_cache.get("CMAKE_CXX_COMPILER_ID"),
        "version": build_cache.get("CMAKE_CXX_COMPILER_VERSION"),
        "version_text": build_cache.get("compiler_version_text"),
    }
    build_compatibility = {
        "build_type": build_cache.get("CMAKE_BUILD_TYPE", "unknown"),
        "cxx_flags": build_cache.get("CMAKE_CXX_FLAGS", ""),
        "cxx_flags_release": build_cache.get("CMAKE_CXX_FLAGS_RELEASE", ""),
        "cxx_standard": "23",
        "osx_architectures": build_cache.get("CMAKE_OSX_ARCHITECTURES", ""),
        "osx_deployment_target": build_cache.get("CMAKE_OSX_DEPLOYMENT_TARGET", ""),
        "sanitizers": {
            "asan": build_cache.get("FINCORE_ENABLE_ASAN", "OFF"),
            "ubsan": build_cache.get("FINCORE_ENABLE_UBSAN", "OFF"),
        },
        "source_schema": SOURCE_SCHEMA,
        "compiler": compiler,
        "dataframe_dependency": dataframe_source,
    }
    selected_cases = args.case or list(CASE_NAMES)
    if "all" in selected_cases:
        selected_cases = list(CASE_NAMES)
    if len(selected_cases) != len(set(selected_cases)):
        raise ValueError("--case values must be unique")
    unknown = set(selected_cases) - set(CASE_NAMES)
    if unknown:
        raise ValueError(f"unknown benchmark cases: {sorted(unknown)}")

    result: dict[str, Any] = {
        "schema_version": RUNNER_SCHEMA,
        "source_schema": SOURCE_SCHEMA,
        "digest_scheme": DIGEST_SCHEME,
        "created_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "platform": platform_identity(),
        "build_compatibility": build_compatibility,
        "build_dir": str(build_dir),
        "native_binary": str(binary),
        "source_identity": {"cpp": cpp_source, "python": py_source, "runner": runner_source},
        "runner_sha256": runner_source["sha256"],
        "native_binary_sha256": sha256_file(binary),
        "pinned_python_source_head": PINNED_PYTHON_SOURCE_HEAD,
        "dependencies": {
            "python_runtime": "Anaconda base (conda run -n base python)",
            "native": {
                "compiler": compiler,
                "dataframe_source": build_cache.get("FINCORE_DATAFRAME_SOURCE_DIR"),
                "dataframe_identity": dataframe_source,
            },
        },
        "cases": {},
    }
    failures: list[str] = []
    for case_name in selected_cases:
        try:
            native = _native_worker(
                binary, case_name, output_dir, args.repeats, args.warmups, "timed"
            )
            false_checks = [name for name, value in native.get("checks", {}).items() if value is not True and value != "true"]
            if false_checks:
                raise RuntimeError(f"native {case_name} correctness checks failed: {false_checks}")
            native["status"] = "PASS"
            rss_samples = [float(native["rss_peak_mb"])]
            for sample in range(1, args.rss_process_samples):
                sample_result = _native_worker(
                    binary, case_name, output_dir, args.repeats, args.warmups,
                    f"rss-{sample + 1}",
                )
                if sample_result.get("input_digest") != native.get("input_digest"):
                    raise RuntimeError(f"native {case_name} input changed between RSS runs")
                if sample_result.get("source_schema") != native.get("source_schema"):
                    raise RuntimeError(f"native {case_name} source schema changed between RSS runs")
                if sample_result.get("output_digest") != native.get("output_digest"):
                    raise RuntimeError(f"native {case_name} output changed between RSS runs")
                rss_samples.append(float(sample_result["rss_peak_mb"]))
            native["rss_samples_mb"] = rss_samples
            native["rss_median_mb"] = statistics.median(rss_samples)
            native["rss_spread_mb"] = max(rss_samples) - min(rss_samples)
            native["status"] = "PASS"
            python = _python_worker(
                case_name, python_source, output_dir, args.repeats, args.warmups
            )
            python["status"] = "PASS"
            parity = compare_native_python(native, python)
            if (
                parity.get("status") == "MISMATCH"
                or parity.get("input_digest") == "MISMATCH"
                or parity.get("shared_factor_returns") == "MISMATCH"
            ):
                failures.append(f"{case_name}: cross-language semantic digest mismatch")
            result["cases"][case_name] = {
                "native": native,
                "python": python,
                "cross_language": parity,
            }
        except Exception as error:  # Keep durable partial evidence and fail closed.
            failures.append(f"{case_name}: {error}")
            result["cases"][case_name] = {
                "native": {"status": "FAILED", "error": str(error)},
                "python": {"status": "NOT_RUN"},
                "cross_language": {"status": "NOT_COMPARABLE", "reasons": [str(error)]},
            }
    result["suite_status"] = "FAIL" if failures else "PASS"
    result["suite_reasons"] = failures
    result_path = output_dir / "benchmark.json"
    result_path.write_text(json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")

    if args.baseline:
        baseline = _json_load(Path(args.baseline).resolve())
        result["baseline_comparison"] = compare_results(result, baseline)
        result["suite_status"] = "FAIL" if result["baseline_comparison"]["status"] == "FAIL" or failures else "PASS"
    if args.create_baseline:
        baseline_path = Path(args.create_baseline).resolve()
        if baseline_path.exists():
            raise FileExistsError(f"baseline already exists and will not be overwritten: {baseline_path}")
        if failures:
            raise RuntimeError("cannot create a baseline from an incomplete run: " + "; ".join(failures))
        baseline_path.parent.mkdir(parents=True, exist_ok=True)
        baseline_path.write_text(json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    result_path.write_text(json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8")
    return result


def compare_native_python(native: dict[str, Any], python: dict[str, Any]) -> dict[str, Any]:
    reasons: list[str] = []
    if native.get("input_digest") != python.get("input_digest"):
        reasons.append("synthetic input digest differs")
    case_name = native.get("case")
    if case_name == "portfolio":
        native_details = native.get("details", {})
        python_details = python.get("details", {})
        weights_match = native_details.get("weights_digest") == python_details.get("weights_digest")
        returns_match = native_details.get("returns_digest") == python_details.get("returns_digest")
        if not weights_match:
            reasons.append("portfolio weights digest differs")
        if not returns_match:
            reasons.append("portfolio returns digest differs")
        input_match = native.get("input_digest") == python.get("input_digest")
        return {
            "input_digest": "PASS" if input_match else "MISMATCH",
            "weights": "PASS" if weights_match else "MISMATCH",
            "returns": "PASS" if returns_match else "MISMATCH",
            "status": "PASS" if input_match and weights_match and returns_match else "MISMATCH",
            "reasons": reasons,
        }
    if case_name in PANEL_CASES:
        native_details = native.get("details", {})
        python_details = python.get("details", {})
        native_digest = native_details.get("factor_returns_digest")
        python_digest = python_details.get("factor_returns_digest")
        shared_status = "PASS" if native_digest and native_digest == python_digest else "MISMATCH"
        if shared_status != "PASS":
            reasons.append("shared factor-return output digest differs")
        return {
            "input_digest": "PASS" if not reasons or "synthetic input digest differs" not in reasons else "MISMATCH",
            "shared_factor_returns": shared_status,
            "full_pipeline": "NOT_COMPARABLE",
            "reasons": reasons + ["full model/report contracts are not asserted equivalent"],
        }
    numeric_comparison = compare_semantic_values(
        native.get("details", {}).get("semantic_values"),
        python.get("details", {}).get("semantic_values"),
    )
    if numeric_comparison["status"] != "PASS":
        reasons.append(numeric_comparison["reason"])
    return {
        "input_digest": "PASS" if native.get("input_digest") == python.get("input_digest") else "MISMATCH",
        "shared_output": numeric_comparison["status"],
        "output_digest_equal": native.get("output_digest") == python.get("output_digest"),
        "max_absolute_error": numeric_comparison["max_absolute_error"],
        "numeric_tolerance": numeric_comparison["tolerance"],
        "status": "PASS" if not reasons else "MISMATCH",
        "reasons": reasons,
    }


def compare_semantic_values(native_values: Any, python_values: Any) -> dict[str, Any]:
    """Compare ordered numeric outputs with a small floating-point tolerance."""

    tolerance = {"absolute": 1e-12, "relative": 1e-9}
    if not isinstance(native_values, list) or not isinstance(python_values, list):
        return {"status": "MISMATCH", "reason": "semantic output values are missing",
                "max_absolute_error": None, "tolerance": tolerance}
    if len(native_values) != len(python_values):
        return {"status": "MISMATCH", "reason": "semantic output lengths differ",
                "max_absolute_error": None, "tolerance": tolerance}
    max_error = 0.0
    for index, (native, python) in enumerate(zip(native_values, python_values)):
        if native is None or python is None:
            if native is not None or python is not None:
                return {"status": "MISMATCH",
                        "reason": f"non-finite output positions differ at index {index}",
                        "max_absolute_error": None, "tolerance": tolerance}
            continue
        if not _finite_number(native) or not _finite_number(python):
            return {"status": "MISMATCH", "reason": f"non-finite semantic output at index {index}",
                    "max_absolute_error": None, "tolerance": tolerance}
        native_value = float(native)
        python_value = float(python)
        error = abs(native_value - python_value)
        max_error = max(max_error, error)
        allowed = tolerance["absolute"] + tolerance["relative"] * max(
            abs(native_value), abs(python_value)
        )
        if error > allowed:
            return {"status": "MISMATCH",
                    "reason": f"semantic output differs beyond tolerance at index {index}",
                    "max_absolute_error": max_error, "tolerance": tolerance}
    return {"status": "PASS", "reason": None,
            "max_absolute_error": max_error, "tolerance": tolerance}


def _measure(function: Any, warmups: int, repeats: int) -> tuple[dict[str, Any], Any]:
    result: Any = None
    for _ in range(warmups):
        result = function()
    samples: list[float] = []
    for _ in range(repeats):
        started = time.perf_counter()
        result = function()
        samples.append((time.perf_counter() - started) * 1000.0)
    return median_and_spread(samples), result


def _write_and_verify(html_path: Path, xlsx_path: Path, html: str) -> bool:
    html_path.write_text(html, encoding="utf-8")
    html_matches = html_path.read_text(encoding="utf-8") == html
    xlsx_bytes = xlsx_path.read_bytes()
    return html_matches and xlsx_bytes.startswith(b"PK") and bool(xlsx_bytes)


def _panel_raw(spec: dict[str, int]) -> list[tuple[int, int, float, float, float]]:
    rows: list[tuple[int, int, float, float, float]] = []
    for date in range(spec["dates"]):
        for asset in range(spec["assets"]):
            if spec["stride"] > 1 and (date * 7 + asset) % spec["stride"] != 0:
                continue
            factor = (
                math.sin((date + 1) * 0.17 + asset * 0.9)
                + 0.03 * (asset % 11)
                + 0.002 * math.cos((date + 1) * 0.05)
            )
            one_day = math.sin((date + 1) * 0.11 + asset * 0.7) * 0.002 + 0.0004 * (asset % 13)
            five_day = math.sin((date + 1) * 0.07 + asset * 0.3) * 0.01 - 0.0002 * (asset % 7)
            rows.append((date, asset, factor, one_day, five_day))
    return rows


def _panel_frame(raw: list[tuple[int, int, float, float, float]], spec: dict[str, int], pd: Any) -> tuple[Any, list[tuple[int, str, float, int, float, float]]]:
    # Native timestamps are UTC epoch nanoseconds without a timezone label;
    # use the equivalent naive UTC index for Python's XLSX renderer.
    start = pd.Timestamp("2021-01-04")
    dates: list[Any] = []
    assets: list[str] = []
    factors: list[float] = []
    quantiles: list[int] = []
    one_day: list[float] = []
    five_day: list[float] = []
    records: list[tuple[int, str, float, int, float, float]] = []
    first = 0
    while first < len(raw):
        last = first + 1
        while last < len(raw) and raw[last][0] == raw[first][0]:
            last += 1
        ranked = sorted(range(first, last), key=lambda index: (raw[index][2], raw[index][1]))
        row_quantiles = [1] * (last - first)
        count = last - first
        for rank, index in enumerate(ranked):
            row_quantiles[index - first] = 1 + rank * 5 // max(count, 1)
        for index in range(first, last):
            date_num, asset_num, factor, one, five = raw[index]
            day = start + pd.Timedelta(days=date_num)
            asset_name = f"A{asset_num}"
            q = row_quantiles[index - first]
            dates.append(day)
            assets.append(asset_name)
            factors.append(factor)
            quantiles.append(q)
            one_day.append(one)
            five_day.append(five)
            records.append((int(day.value), asset_name, factor, q, one, five))
        first = last
    index = pd.MultiIndex.from_arrays([dates, assets], names=("date", "asset"))
    frame = pd.DataFrame(
        {"factor": factors, "factor_quantile": quantiles, "1D": one_day, "5D": five_day},
        index=index,
    )
    return frame, records


def panel_input_digest(records: list[tuple[int, str, float, int, float, float]]) -> str:
    digest = FNV64()
    digest.string("panel-input-v1")
    for label in ("1D", "5D"):
        digest.u64(len(label))
        digest.string(label)
    for timestamp, asset, factor, quantile, one_day, five_day in records:
        digest.i64(timestamp)
        digest.u64(len(asset.encode("utf-8")))
        digest.string(asset)
        digest.string(canonical_number(factor))
        digest.add(b";")
        digest.i32(quantile)
        digest.string(canonical_number(one_day))
        digest.add(b";")
        digest.string(canonical_number(five_day))
        digest.add(b";")
    return digest.hexdigest()


def returns_values(rows: int, columns: int) -> list[list[float]]:
    result: list[list[float]] = []
    for row in range(rows):
        values: list[float] = []
        for column in range(columns):
            residue = (row + 3 * column) % 19 - 9
            values.append(math.sin((row + 1) * 0.013 + column * 0.21) * 0.01 + 0.0002 * residue)
        result.append(values)
    return result


def returns_input_digest(values: list[list[float]], columns: int) -> str:
    digest = FNV64()
    digest.string("returns-input-v1")
    digest.u64(columns)
    digest.u64(len(values))
    for row in values:
        for value in row:
            digest.string(canonical_number(value))
            digest.add(b";")
    return digest.hexdigest()


def _python_worker_main(args: argparse.Namespace) -> int:
    source = Path(args.python_source).resolve()
    sys.path.insert(0, str(source))
    import numpy as np
    import pandas as pd
    from fincore.factor_analysis.analysis import analyze_factor
    from fincore.factor_analysis import performance
    from fincore.metrics.ratios import sharpe_ratio
    from fincore.metrics.rolling import rolling_sharpe

    case_name = args.worker_case
    scratch = Path(args.worker_scratch).resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    timings: dict[str, Any] = {}
    details: dict[str, Any] = {}

    if case_name in PANEL_CASES:
        spec = PANEL_CASES[case_name]
        raw_timing, raw = _measure(lambda: _panel_raw(spec), args.warmups, args.repeats)
        convert_timing, converted = _measure(lambda: _panel_frame(raw, spec, pd), args.warmups, args.repeats)
        frame, records = converted
        input_digest = panel_input_digest(records)
        if case_name == "portfolio":
            weights_timing, weights = _measure(lambda: performance.factor_weights(frame), args.warmups, args.repeats)
            returns_timing, returns = _measure(lambda: performance.factor_returns(frame), args.warmups, args.repeats)
            weights_digest = values_digest(weights.to_numpy(dtype=float, copy=False).ravel())
            returns_digest = values_digest(returns.to_numpy(dtype=float, copy=False).ravel())
            combined = FNV64()
            combined.string(weights_digest)
            combined.string(returns_digest)
            output_digest = combined.hexdigest()
            timings.update({"generation": raw_timing, "conversion": convert_timing,
                            "portfolio_weights": weights_timing, "portfolio_returns": returns_timing})
            details.update({"rows": len(frame), "weights_digest": weights_digest,
                            "returns_digest": returns_digest,
                            "weights_count": len(weights), "returns_count": int(returns.size)})
        else:
            compute_timing, model = _measure(
                lambda: analyze_factor(frame, **PANEL_CONFIG), args.warmups, args.repeats
            )
            copy_timing, copied = _measure(lambda: dataclasses.replace(model), args.warmups, args.repeats)
            projection_timing, document = _measure(
                lambda: __import__("fincore.report.factor.compute", fromlist=["build_factor_report"])
                .build_factor_report(model), args.warmups, args.repeats
            )
            from fincore.report.renderers.html import render_html
            from fincore.report.renderers.xlsx import write_xlsx

            html_timing, html = _measure(lambda: render_html(document), args.warmups, args.repeats)
            xlsx_path = scratch / "factor-report.xlsx"
            xlsx_timing, _ = _measure(
                lambda: write_xlsx(document, xlsx_path), args.warmups, args.repeats
            )
            html_path = scratch / "factor-report.html"
            io_timing, io_ok = _measure(
                lambda: _write_and_verify(html_path, xlsx_path, html),
                args.warmups, args.repeats,
            )
            timings.update({"generation": raw_timing, "conversion": convert_timing,
                            "compute": compute_timing, "model_copy": copy_timing,
                            "report_projection": projection_timing, "html_render": html_timing,
                            "xlsx_export": xlsx_timing, "file_io": io_timing})
            factor_return_digest = values_digest(model.factor_returns.to_numpy(dtype=float, copy=False).ravel())
            output_digest = model.result_fingerprint
            details.update({"rows": len(frame), "model_fingerprint": model.result_fingerprint,
                            "factor_returns_digest": factor_return_digest,
                            "html_bytes": len(html.encode("utf-8")),
                            "xlsx_bytes": xlsx_path.stat().st_size,
                            "xlsx_measurement_scope": "write_xlsx includes file writing",
                            "file_io_ok": bool(io_ok)})
    elif case_name in RETURNS_CASES:
        spec = RETURNS_CASES[case_name]
        rows = spec["rows"]
        columns = spec["columns"]
        generation_timing, generated = _measure(
            lambda: returns_values(rows, columns), args.warmups, args.repeats
        )
        index = pd.date_range("2000-01-01", periods=rows, freq="D")
        conversion_timing, data = _measure(
            lambda: np.asarray(generated, dtype=np.float64), args.warmups, args.repeats
        )
        input_digest = returns_input_digest(generated, columns)
        if case_name == "scalar-sharpe":
            compute_timing, computed = _measure(
                lambda: [float(sharpe_ratio(data[:, 0], annualization=252.0))], args.warmups, args.repeats
            )
        elif case_name == "batched-sharpe":
            compute_timing, computed = _measure(
                lambda: np.asarray(sharpe_ratio(data, annualization=252.0), dtype=float).ravel().tolist(),
                args.warmups, args.repeats,
            )
        else:
            series = pd.Series(data[:, 0], index=index, name="returns")
            compute_timing, computed_series = _measure(
                lambda: rolling_sharpe(series, spec["window"], annualization=252.0),
                args.warmups, args.repeats,
            )
            computed = computed_series.to_numpy(dtype=float, copy=False).tolist()
        timings.update({"generation": generation_timing, "conversion": conversion_timing,
                        "compute": compute_timing})
        output_digest = values_digest(computed)
        semantic_values = [
            float(value) if value is not None and math.isfinite(float(value)) else None
            for value in computed
        ]
        details.update({"rows": rows, "columns": columns, "output_count": len(computed),
                        "semantic_values": semantic_values})
    else:
        raise ValueError(f"unknown Python worker case: {case_name}")

    imported_modules = imported_fincore_module_paths(source)
    dependencies = {}
    for package in ("numpy", "pandas", "scipy", "numba", "openpyxl", "xlsxwriter"):
        try:
            dependencies[package] = importlib.metadata.version(package)
        except importlib.metadata.PackageNotFoundError:
            dependencies[package] = None
    result = {
        "schema_version": 3,
        "source_schema": SOURCE_SCHEMA,
        "digest_scheme": DIGEST_SCHEME,
        "case": case_name,
        "warmups": args.warmups,
        "repeats": args.repeats,
        "input_digest": input_digest,
        "output_digest": output_digest,
        "measurements_ms": timings,
        "details": details,
        "rss_peak_mb": _rss_megabytes(),
        "environment": {
            "python": sys.version.split()[0],
            "platform": platform_identity(),
            "dependencies": dependencies,
            "imported_fincore_modules": imported_modules,
        },
    }
    Path(args.worker_output).write_text(
        json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n", encoding="utf-8"
    )
    return 0


def _rss_megabytes() -> float:
    value = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if sys.platform == "darwin":
        return float(value) / (1024.0 * 1024.0)
    return float(value) / 1024.0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", action="append", choices=(*CASE_NAMES, "all"), help="benchmark case; repeat to select several")
    parser.add_argument("--repeats", type=int, default=9, help="measured repetitions per stage (minimum 7)")
    parser.add_argument("--warmups", type=int, default=2, help="unmeasured warmups per stage")
    parser.add_argument("--output-dir", required=False, help="new, non-existing directory for JSON, logs, and scratch artifacts")
    parser.add_argument("--baseline", help="read-only baseline JSON for a fail-closed comparison")
    parser.add_argument("--create-baseline", help="explicitly create this baseline JSON; the path must not exist")
    parser.add_argument("--python-source", default=str(DEFAULT_PYTHON_SOURCE), help="pinned Python fincore source checkout")
    parser.add_argument("--native-bin", help="use an already-built native benchmark executable")
    parser.add_argument("--build-dir", help="private /tmp/fincore-phase87-* build directory")
    parser.add_argument("--rss-process-samples", type=int, default=3, help="independent native process peaks per case (minimum 3)")
    parser.add_argument("--_python-worker", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--worker-case", choices=CASE_NAMES, help=argparse.SUPPRESS)
    parser.add_argument("--worker-output", help=argparse.SUPPRESS)
    parser.add_argument("--worker-scratch", help=argparse.SUPPRESS)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args._python_worker:
        if not all((args.worker_case, args.worker_output, args.worker_scratch)):
            parser.error("internal Python worker arguments are incomplete")
        if args.repeats < 1 or args.warmups < 0:
            parser.error("internal worker repeat counts are invalid")
        return _python_worker_main(args)
    if args.repeats < 7:
        parser.error("--repeats must be at least 7")
    if args.warmups < 0:
        parser.error("--warmups must not be negative")
    if args.rss_process_samples < 3:
        parser.error("--rss-process-samples must be at least 3 for noise-aware RSS comparison")
    if not args.output_dir:
        parser.error("--output-dir is required")
    if args.baseline and args.create_baseline:
        parser.error("choose --baseline or --create-baseline, not both")
    try:
        result = run_suite(args)
    except Exception as error:
        print(f"benchmark runner error: {error}", file=sys.stderr)
        return 2
    print(json.dumps({"status": result["suite_status"], "output_dir": str(Path(args.output_dir).resolve()),
                      "baseline_comparison": result.get("baseline_comparison")}, indent=2, sort_keys=True))
    return 0 if result["suite_status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
