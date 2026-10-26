"""Run a paired, single-CPU C++ reference/candidate regression campaign.

This is a small outer-process campaign driver for the existing synthetic C++
benchmark cases.  It reuses the benchmark executable's JSON contract and the
P111 measurement helpers; the legacy ``tools/run_benchmarks.py`` mode remains
unchanged.  Correctness fields are validated and compared after each child
exits, before the next child is started.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import sys
from dataclasses import asdict
from pathlib import Path
from typing import Any, Sequence


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from benchmarks.alpha003_a_stocks import measurement  # noqa: E402
from tools.run_benchmarks import (  # noqa: E402
    CASE_NAMES,
    PANEL_CASES,
    RETURNS_CASES,
)


GENERIC_CASES = (
    "dense",
    "sparse",
    "scaling-small",
    "scaling-large",
    "scalar-sharpe",
    "batched-sharpe",
    "rolling-sharpe",
    "portfolio",
)
OPTIONAL_CASES = ("rolling-engine",)
ROLLING_METRICS = frozenset(
    {"mean_return", "volatility", "sharpe", "max_drawdown", "beta"}
)
if not set(GENERIC_CASES).issubset(CASE_NAMES):
    raise RuntimeError("the legacy native runner no longer defines the complete generic-eight set")
DEFAULT_CASES = GENERIC_CASES
CASE_CHOICES = tuple(dict.fromkeys((*GENERIC_CASES, *OPTIONAL_CASES, "all")))
INNER_REPEATS = 7
INNER_WARMUPS = 0
SEMANTIC_ABSOLUTE_TOLERANCE = 1e-12
SEMANTIC_RELATIVE_TOLERANCE = 1e-9
_COMMIT_RE = re.compile(r"^[0-9a-fA-F]{40,64}$")
_RETURN_TIMING_STAGES = frozenset({"generation", "conversion", "compute"})
_PANEL_TIMING_STAGES = frozenset({
    "generation",
    "conversion",
    "compute",
    "model_copy",
    "report_projection",
    "html_render",
    "xlsx_render",
    "file_io",
})
_PORTFOLIO_TIMING_STAGES = frozenset({
    "generation", "conversion", "portfolio_weights", "portfolio_returns"
})
REQUIRED_TIMING_STAGES = {
    **{name: _PANEL_TIMING_STAGES for name in PANEL_CASES if name != "portfolio"},
    **{name: _RETURN_TIMING_STAGES for name in RETURNS_CASES},
    "portfolio": _PORTFOLIO_TIMING_STAGES,
    "rolling-engine": _RETURN_TIMING_STAGES,
}
_ROLLING_SCORING_SCOPE = "child_compute_stage"
_ROLLING_SCORING_FIELD = "benchmark_timing_summaries_ms.compute.median_ms"
_PROCESS_SCORING_SCOPE = "external_process_wall"
_PROCESS_SCORING_FIELD = "measurement.wall_seconds"


class CampaignError(RuntimeError):
    """A fail-closed campaign or sample validation error."""


def sha256_file(path: str | os.PathLike[str]) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_pair_count(pair_count: int) -> str:
    """Return the mode; only 5 development pairs or even formal batches are valid."""

    if isinstance(pair_count, bool) or not isinstance(pair_count, int):
        raise ValueError("pairs must be an integer")
    if pair_count == 5:
        return "DEVELOPMENT_ONLY"
    if pair_count < 10 or pair_count % 2:
        raise ValueError("pairs must be exactly 5 for development or an even number >= 10")
    return "FORMAL"


def select_cases(
    requested: Sequence[str] | None, *, include_rolling_engine: bool = False
) -> tuple[str, ...]:
    selected = list(DEFAULT_CASES if requested is None else requested)
    if "all" in selected:
        if len(selected) != 1:
            raise ValueError("--case all cannot be combined with named cases")
        selected = list(DEFAULT_CASES)
    if len(selected) != len(set(selected)):
        raise ValueError("case names must be unique")
    unknown = set(selected) - set(CASE_CHOICES)
    if unknown:
        raise ValueError(f"unsupported case names: {sorted(unknown)}")
    if not selected:
        raise ValueError("at least one case is required")
    if include_rolling_engine and "rolling-engine" not in selected:
        selected.append("rolling-engine")
    return tuple(selected)


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"non-standard JSON numeric constant is forbidden: {value}")


def _validate_json_numbers(value: Any, path: str = "$") -> None:
    if isinstance(value, bool) or value is None or isinstance(value, (str, int)):
        return
    if isinstance(value, float):
        if not math.isfinite(value):
            raise CampaignError(f"non-finite JSON number at {path}")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            _validate_json_numbers(item, f"{path}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            _validate_json_numbers(item, f"{path}.{key}")
        return
    raise CampaignError(f"unsupported JSON value at {path}: {type(value).__name__}")


def _read_benchmark_json(path: Path) -> dict[str, Any]:
    try:
        text = path.read_text(encoding="utf-8")
        lines = [line for line in text.splitlines() if line.strip()]
        if len(lines) != 1:
            raise CampaignError("benchmark child must emit exactly one non-empty JSON line")
        payload = json.loads(lines[0], parse_constant=_reject_json_constant)
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        if isinstance(error, CampaignError):
            raise
        raise CampaignError(f"cannot parse benchmark JSON from {path}: {error}") from error
    if not isinstance(payload, dict):
        raise CampaignError("benchmark JSON root must be an object")
    _validate_json_numbers(payload)
    return payload


def _nonempty_string(value: Any) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _validate_semantic_values(values: Any, label: str) -> None:
    if not isinstance(values, list):
        raise CampaignError(f"{label} must be a full semantic_values array")
    if any(
        value is not None
        and (
            isinstance(value, bool)
            or not isinstance(value, (int, float))
            or not math.isfinite(float(value))
        )
        for value in values
    ):
        raise CampaignError(f"{label} must contain only finite numbers or null NaN markers")


def _validate_timing_samples(payload: dict[str, Any], case_name: str) -> None:
    measurements = payload.get("measurements_ms")
    if not isinstance(measurements, dict) or not measurements:
        raise CampaignError("benchmark timing samples are missing")
    expected_stages = REQUIRED_TIMING_STAGES.get(case_name)
    if expected_stages is None:
        raise CampaignError(f"no fixed native timing-stage contract for {case_name!r}")
    actual_stages = set(measurements)
    if actual_stages != expected_stages:
        missing = sorted(expected_stages - actual_stages)
        unexpected = sorted(actual_stages - expected_stages)
        raise CampaignError(
            f"benchmark timing stages for {case_name} differ from the native contract; "
            f"missing={missing}, unexpected={unexpected}"
        )
    for name, summary in measurements.items():
        if not isinstance(summary, dict):
            raise CampaignError(f"measurement {name} is malformed")
        samples = summary.get("samples_ms")
        if not isinstance(samples, list) or len(samples) != INNER_REPEATS:
            raise CampaignError(f"measurement {name} must contain {INNER_REPEATS} timing samples")
        if any(
            isinstance(value, bool)
            or not isinstance(value, (int, float))
            or not math.isfinite(float(value))
            or value < 0
            for value in samples
        ):
            raise CampaignError(f"measurement {name} has invalid timing samples")
        if type(summary.get("stage_invocations_per_sample")) is not int:
            raise CampaignError(f"measurement {name} stage invocation count must be an integer")
        numeric = [float(value) for value in samples]
        expected = {
            "median_ms": float(statistics.median(numeric)),
            "min_ms": min(numeric),
            "max_ms": max(numeric),
            "spread_ms": max(numeric) - min(numeric),
        }
        for field, value in expected.items():
            actual = summary.get(field)
            if (
                isinstance(actual, bool)
                or not isinstance(actual, (int, float))
                or not math.isfinite(float(actual))
                or not math.isclose(float(actual), value, rel_tol=1e-12, abs_tol=1e-12)
            ):
                raise CampaignError(f"measurement {name}.{field} does not match raw samples")
        if summary["stage_invocations_per_sample"] != 1:
            raise CampaignError(f"measurement {name} must invoke its stage once per sample")


def validate_benchmark_result(payload: dict[str, Any], case_name: str) -> None:
    """Validate process success separately from actual benchmark output validity."""

    if payload.get("case") != case_name:
        raise CampaignError(f"expected case {case_name!r}, got {payload.get('case')!r}")
    if payload.get("schema_version") != 3:
        raise CampaignError("unsupported C++ benchmark JSON schema_version")
    for field in ("source_schema", "digest_scheme", "input_digest", "output_digest"):
        if not _nonempty_string(payload.get(field)):
            raise CampaignError(f"benchmark JSON has missing/empty {field}")
    if (
        type(payload.get("warmups")) is not int
        or type(payload.get("repeats")) is not int
        or payload["warmups"] != INNER_WARMUPS
        or payload["repeats"] != INNER_REPEATS
    ):
        raise CampaignError("benchmark JSON does not match fixed inner warmup/repeat settings")
    if not isinstance(payload.get("workload"), dict) or not payload["workload"]:
        raise CampaignError("benchmark JSON workload is missing or empty")
    if not isinstance(payload.get("details"), dict) or not payload["details"]:
        raise CampaignError("benchmark JSON details are missing or empty")
    checks = payload.get("checks")
    if not isinstance(checks, dict) or not checks:
        raise CampaignError("benchmark JSON checks are missing or empty")
    failed_checks = [name for name, value in checks.items() if value is not True and value != "true"]
    if failed_checks:
        raise CampaignError(f"benchmark correctness checks failed: {failed_checks}")
    _validate_timing_samples(payload, case_name)
    rss_peak = payload.get("rss_peak_mb")
    if isinstance(rss_peak, bool) or not isinstance(rss_peak, (int, float)) or rss_peak < 0:
        raise CampaignError("benchmark RSS peak is missing or invalid")
    allocation = payload.get("allocation_instrumentation")
    if not isinstance(allocation, dict) or allocation.get("enabled") is not False:
        raise CampaignError("scored campaign requires allocation profiling explicitly disabled")
    environment = payload.get("environment")
    if not isinstance(environment, dict) or not environment:
        raise CampaignError("benchmark environment metadata is missing")

    details = payload["details"]
    if case_name in RETURNS_CASES:
        values = details.get("semantic_values")
        count = details.get("output_count")
        if not isinstance(values, list) or not isinstance(count, int) or isinstance(count, bool):
            raise CampaignError("returns benchmark lacks full semantic_values/output_count")
        if count != len(values):
            raise CampaignError("returns output_count does not match semantic_values length")
        _validate_semantic_values(values, "returns semantic_values")
    elif case_name == "portfolio":
        for field in ("rows", "weight_rows", "return_values"):
            if type(details.get(field)) is not int or details[field] < 0:
                raise CampaignError(f"portfolio output is missing or has invalid {field}")
        for field in ("weights_digest", "returns_digest"):
            if not _nonempty_string(details.get(field)):
                raise CampaignError(f"portfolio output is missing {field}")
        if "semantic_values" in details:
            _validate_semantic_values(details["semantic_values"], "portfolio semantic_values")
    elif case_name in PANEL_CASES:
        if type(details.get("rows")) is not int or details["rows"] < 0:
            raise CampaignError("panel output is missing or has invalid rows")
        for field in ("model_fingerprint", "factor_returns_digest"):
            if not _nonempty_string(details.get(field)):
                raise CampaignError(f"panel output is missing {field}")
        for field in ("html_bytes", "xlsx_bytes"):
            if type(details.get(field)) is not int or details[field] < 0:
                raise CampaignError(f"panel output is missing or has invalid {field}")
        for field in ("html_digest", "xlsx_digest"):
            if not _nonempty_string(details.get(field)):
                raise CampaignError(f"panel output has invalid {field}")
        if "semantic_values" in details:
            _validate_semantic_values(details["semantic_values"], "panel semantic_values")
    elif case_name == "rolling-engine":
        required_checks = {
            "metric_names_valid",
            "output_count_valid",
            "oracle_indexes_valid",
            "oracle_nan_masks_valid",
            "oracle_values_valid",
            "output_stable_across_repeats",
        }
        if not required_checks.issubset(checks):
            raise CampaignError("rolling-engine output lacks its full oracle checks")
        metrics = details.get("metrics")
        if not isinstance(metrics, dict) or set(metrics) != ROLLING_METRICS:
            raise CampaignError("rolling-engine output must contain all five metric series")
        counts = details.get("output_count")
        if not isinstance(counts, dict) or set(counts) != ROLLING_METRICS:
            raise CampaignError("rolling-engine output_count must contain all five metrics")
        for metric_name in sorted(ROLLING_METRICS):
            series = metrics[metric_name]
            if not isinstance(series, dict):
                raise CampaignError(f"rolling-engine metric {metric_name} is malformed")
            index = series.get("index")
            values = series.get("values")
            if not isinstance(index, list) or not isinstance(values, list):
                raise CampaignError(f"rolling-engine metric {metric_name} lacks index/values")
            if len(index) != len(values) or counts.get(metric_name) != len(values):
                raise CampaignError(f"rolling-engine metric {metric_name} length/count mismatch")
            if type(counts.get(metric_name)) is not int or counts[metric_name] <= 0:
                raise CampaignError(f"rolling-engine metric {metric_name} count must be positive")
            if any(
                type(value) is not int or value < -(1 << 63) or value >= (1 << 63)
                for value in index
            ):
                raise CampaignError(f"rolling-engine metric {metric_name} index must be int64 values")
            if any(right <= left for left, right in zip(index, index[1:])):
                raise CampaignError(f"rolling-engine metric {metric_name} index must increase strictly")
            for value in values:
                if value is None or value in ("inf", "-inf"):
                    continue
                if (
                    isinstance(value, bool)
                    or not isinstance(value, (int, float))
                    or not math.isfinite(float(value))
                ):
                    raise CampaignError(
                        f"rolling-engine metric {metric_name} has an invalid semantic value"
                    )


def _semantic_details(details: dict[str, Any]) -> dict[str, Any]:
    """Drop only run diagnostics/counters; keep all business outputs and shapes."""

    def project(value: Any) -> Any:
        if isinstance(value, dict):
            return {
                key: project(item)
                for key, item in value.items()
                if "diagnostic" not in key.lower()
                and not key.lower().endswith("_digest")
                and key.lower() not in {"metric_output_digests", "output_digests"}
            }
        if isinstance(value, list):
            return [project(item) for item in value]
        return value

    return project(details)


def _semantic_equal(left: Any, right: Any, path: str = "details") -> tuple[bool, str | None, float]:
    """Compare business JSON recursively with the existing semantic tolerance."""

    if isinstance(left, bool) or isinstance(right, bool):
        return (left is right, None if left is right else path, 0.0)
    if left is None or right is None:
        return (left is right, None if left is right else path, 0.0)
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        if not math.isfinite(float(left)) or not math.isfinite(float(right)):
            return False, path, math.inf
        error = abs(float(left) - float(right))
        limit = SEMANTIC_ABSOLUTE_TOLERANCE + SEMANTIC_RELATIVE_TOLERANCE * max(
            abs(float(left)), abs(float(right))
        )
        return error <= limit, None if error <= limit else path, error
    if isinstance(left, dict) and isinstance(right, dict):
        if set(left) != set(right):
            return False, f"{path} keys", 0.0
        max_error = 0.0
        for key in sorted(left):
            equal, mismatch, error = _semantic_equal(left[key], right[key], f"{path}.{key}")
            if not equal:
                return False, mismatch, error
            max_error = max(max_error, error)
        return True, None, max_error
    if isinstance(left, list) and isinstance(right, list):
        if len(left) != len(right):
            return False, f"{path} length", 0.0
        max_error = 0.0
        for index, (left_item, right_item) in enumerate(zip(left, right)):
            equal, mismatch, error = _semantic_equal(left_item, right_item, f"{path}[{index}]")
            if not equal:
                return False, mismatch, error
            max_error = max(max_error, error)
        return True, None, max_error
    equal = type(left) is type(right) and left == right
    return equal, None if equal else path, 0.0


def compare_semantic_outputs(
    case_name: str, reference: dict[str, Any], candidate: dict[str, Any]
) -> dict[str, Any]:
    """Compare stable case inputs and real business outputs, never timing fields."""

    identity_fields = (
        "schema_version",
        "source_schema",
        "digest_scheme",
        "case",
        "workload",
        "input_digest",
        "environment",
    )
    mismatches = [field for field in identity_fields if reference.get(field) != candidate.get(field)]
    if reference.get("case") != case_name or candidate.get("case") != case_name:
        mismatches.append("case")
    if reference.get("checks") != candidate.get("checks"):
        mismatches.append("checks")
    semantic_error = 0.0
    if case_name in RETURNS_CASES:
        left = reference.get("details", {}).get("semantic_values")
        right = candidate.get("details", {}).get("semantic_values")
        # Null markers are compared positionally, so NaN-mask regressions fail.
        equal, mismatch, semantic_error = _semantic_equal(left, right, "semantic_values")
        if not equal:
            mismatches.append(mismatch or "semantic_values")
    elif case_name == "portfolio":
        left_details = reference.get("details", {})
        right_details = candidate.get("details", {})
        equal, mismatch, semantic_error = _semantic_equal(
            left_details, right_details, "details"
        )
        if not equal:
            mismatches.append(mismatch or "details")
    elif case_name in PANEL_CASES:
        left_details = reference.get("details", {})
        right_details = candidate.get("details", {})
        # This one field is explicitly diagnostic instrumentation. Keep every
        # other business/output field, including future fields such as model
        # copy counts, in the comparison instead of an allowlist projection.
        left_semantic = {
            key: value for key, value in left_details.items()
            if key != "factor_compute_diagnostics"
        }
        right_semantic = {
            key: value for key, value in right_details.items()
            if key != "factor_compute_diagnostics"
        }
        equal, mismatch, semantic_error = _semantic_equal(
            left_semantic, right_semantic, "details"
        )
        if not equal:
            mismatches.append(mismatch or "details")
    elif case_name == "rolling-engine":
        left_details = reference.get("details", {})
        right_details = candidate.get("details", {})
        left_metrics = left_details.get("metrics", {})
        right_metrics = right_details.get("metrics", {})
        for metric_name in sorted(ROLLING_METRICS):
            left_series = left_metrics[metric_name]
            right_series = right_metrics[metric_name]
            if left_series["index"] != right_series["index"]:
                mismatches.append(f"details.metrics.{metric_name}.index")
            equal, mismatch, error = _semantic_equal(
                left_series["values"],
                right_series["values"],
                f"details.metrics.{metric_name}.values",
            )
            semantic_error = max(semantic_error, error)
            if not equal:
                mismatches.append(mismatch or f"details.metrics.{metric_name}.values")
        left_other = {key: value for key, value in left_details.items() if key != "metrics"}
        right_other = {key: value for key, value in right_details.items() if key != "metrics"}
        equal, mismatch, error = _semantic_equal(
            _semantic_details(left_other),
            _semantic_details(right_other),
            "details",
        )
        semantic_error = max(semantic_error, error)
        if not equal:
            mismatches.append(mismatch or "details")
    else:
        equal, mismatch, semantic_error = _semantic_equal(
            _semantic_details(reference.get("details", {})),
            _semantic_details(candidate.get("details", {})),
        )
        if not equal:
            mismatches.append(mismatch or "details")

    # `output_digest` is retained and validated, but not used as a substitute
    # for full semantic_values. For panel/portfolio cases, compare the stable
    # component output fields that the existing native baseline protocol exposes.
    digest_equal: bool | None = None
    if case_name == "portfolio" or case_name in PANEL_CASES:
        # Component digests above are the existing semantic contract. The
        # aggregate output_digest is derived from them and is not an
        # independent correctness check.
        digest_equal = reference.get("output_digest") == candidate.get("output_digest")
    return {
        "status": "PASS" if not mismatches else "MISMATCH",
        "mismatches": mismatches,
        "max_absolute_error": semantic_error,
        "aggregate_output_digest_equal_diagnostic_only": digest_equal,
        "numeric_tolerance": {
            "absolute": SEMANTIC_ABSOLUTE_TOLERANCE,
            "relative": SEMANTIC_RELATIVE_TOLERANCE,
        },
        "nan_positions_compared": case_name in RETURNS_CASES or case_name == "rolling-engine",
    }


def _write_campaign(path: Path, result: dict[str, Any]) -> None:
    path.write_text(
        json.dumps(result, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
    )


def _run_one(
    *,
    binary: Path,
    binary_digest: str,
    case_name: str,
    side: str,
    phase: str,
    pair_index: int,
    order_index: int,
    output_dir: Path,
    cpu_index: int,
    timeout: float,
) -> tuple[dict[str, Any], dict[str, Any] | None]:
    run_key = f"{phase}-pair-{pair_index:03d}-order-{order_index}-{side}"
    case_dir = output_dir / "cases" / case_name
    run_dir = case_dir / "runs" / run_key
    run_dir.mkdir(parents=True, exist_ok=False)
    stdout_path = run_dir / "stdout.jsonl"
    stderr_path = run_dir / "stderr.log"
    scratch_dir = case_dir / "scratch" / run_key
    command = (
        str(binary), "--case", case_name,
        "--repeats", str(INNER_REPEATS),
        "--warmups", str(INNER_WARMUPS),
        "--scratch-dir", str(scratch_dir),
    )
    record: dict[str, Any] = {
        "side": side,
        "phase": phase,
        "pair_index": pair_index,
        "order_index": order_index,
        "command": list(command),
        "stdout_path": str(stdout_path.relative_to(output_dir)),
        "stderr_path": str(stderr_path.relative_to(output_dir)),
        "scratch_dir": str(scratch_dir.relative_to(output_dir)),
    }
    current_digest = sha256_file(binary)
    record["binary_sha256_before"] = current_digest
    if current_digest != binary_digest:
        record["error"] = f"{side} binary changed after campaign start"
        record["binary_sha256_after"] = sha256_file(binary)
        return record, None
    try:
        measured = measurement.run_command(
            command,
            cpu_index=cpu_index,
            cwd=ROOT,
            stdout_path=stdout_path,
            stderr_path=stderr_path,
            timeout=timeout,
        )
    except Exception as error:
        record["error"] = f"{type(error).__name__}: {error}"
        record["binary_sha256_after"] = sha256_file(binary)
        return record, None
    record["measurement"] = asdict(measured)
    if (
        measured.command != tuple(command)
        or measured.affinity_method != "inherited_parent_affinity_before_create_process"
        or isinstance(measured.wall_seconds, bool)
        or not isinstance(measured.wall_seconds, (int, float))
        or not math.isfinite(measured.wall_seconds)
        or measured.wall_seconds <= 0.0
        or isinstance(measured.cpu_seconds, bool)
        or not isinstance(measured.cpu_seconds, (int, float))
        or not math.isfinite(measured.cpu_seconds)
        or measured.cpu_seconds < 0.0
        or isinstance(measured.peak_working_set_bytes, bool)
        or not isinstance(measured.peak_working_set_bytes, int)
        or measured.peak_working_set_bytes < 0
        or type(measured.cpu_index) is not int
        or measured.cpu_index != cpu_index
        or type(measured.affinity_mask) is not int
        or measured.affinity_mask != (1 << cpu_index)
        or measured.affinity_verified is not True
        or measured.thread_environment != measurement.THREAD_ENVIRONMENT
        or type(measured.returncode) is not int
    ):
        record["error"] = "process measurement did not satisfy the requested affinity/thread protocol"
        record["binary_sha256_after"] = sha256_file(binary)
        return record, None
    if measured.returncode != 0:
        record["error"] = f"{side} {case_name} exited with code {measured.returncode}"
        record["binary_sha256_after"] = sha256_file(binary)
        return record, None
    try:
        payload = _read_benchmark_json(stdout_path)
        validate_benchmark_result(payload, case_name)
    except Exception as error:
        record["error"] = f"{type(error).__name__}: {error}"
        record["binary_sha256_after"] = sha256_file(binary)
        return record, None
    record["benchmark_json_sha256"] = sha256_file(stdout_path)
    record["benchmark_identity"] = {
        "schema_version": payload.get("schema_version"),
        "source_schema": payload.get("source_schema"),
        "digest_scheme": payload.get("digest_scheme"),
        "case": payload.get("case"),
        "input_digest": payload.get("input_digest"),
        "output_digest": payload.get("output_digest"),
        "workload": payload.get("workload"),
        "checks": payload.get("checks"),
        "environment": payload.get("environment"),
        "rss_peak_mb": payload.get("rss_peak_mb"),
        "detail_fields": sorted(payload.get("details", {})),
        "allocation_profile_enabled": payload["allocation_instrumentation"]["enabled"],
    }
    record["benchmark_timing_summaries_ms"] = payload["measurements_ms"]
    record["binary_sha256_after"] = sha256_file(binary)
    if record["binary_sha256_after"] != binary_digest:
        record["error"] = f"{side} binary changed during campaign"
    return record, payload


def _case_summary(
    case_report: dict[str, Any], pairs: int, mode: str, *, case_name: str
) -> None:
    measured_runs = [
        run for run in case_report["runs"]
        if run.get("phase") == "measured" and "measurement" in run
    ]
    reference = [run for run in measured_runs if run.get("side") == "reference"]
    candidate = [run for run in measured_runs if run.get("side") == "candidate"]
    if len(reference) != len(candidate) or len(reference) != pairs:
        case_report["status"] = "INCOMPLETE"
        return
    reference_wall = [float(run["measurement"]["wall_seconds"]) for run in reference]
    candidate_wall = [float(run["measurement"]["wall_seconds"]) for run in candidate]
    reference_cpu = [float(run["measurement"]["cpu_seconds"]) for run in reference]
    candidate_cpu = [float(run["measurement"]["cpu_seconds"]) for run in candidate]
    reference_peak = [int(run["measurement"]["peak_working_set_bytes"]) for run in reference]
    candidate_peak = [int(run["measurement"]["peak_working_set_bytes"]) for run in candidate]
    paired_ratios = [right / left for left, right in zip(reference_wall, candidate_wall)]
    wall_reference_summary = measurement.summarize_samples(reference_wall)
    wall_candidate_summary = measurement.summarize_samples(candidate_wall)
    if case_name == "rolling-engine":
        scoring_scope = _ROLLING_SCORING_SCOPE
        scoring_timing_field = _ROLLING_SCORING_FIELD
        reference_scores = [
            float(run["benchmark_timing_summaries_ms"]["compute"]["median_ms"])
            for run in reference
        ]
        candidate_scores = [
            float(run["benchmark_timing_summaries_ms"]["compute"]["median_ms"])
            for run in candidate
        ]
    else:
        scoring_scope = _PROCESS_SCORING_SCOPE
        scoring_timing_field = _PROCESS_SCORING_FIELD
        reference_scores = reference_wall
        candidate_scores = candidate_wall
    reference_score_summary = measurement.summarize_samples(reference_scores)
    candidate_score_summary = measurement.summarize_samples(candidate_scores)
    paired_score_ratios = [right / left for left, right in zip(reference_scores, candidate_scores)]
    summary: dict[str, Any] = {
        "scoring_scope": scoring_scope,
        "scoring_timing_field": scoring_timing_field,
        "reference_scoring_samples": reference_score_summary,
        "candidate_scoring_samples": candidate_score_summary,
        "paired_scoring_ratios_candidate_over_reference": measurement.summarize_samples(
            paired_score_ratios
        ),
        "reference_wall_seconds": wall_reference_summary,
        "candidate_wall_seconds": wall_candidate_summary,
        "reference_cpu_seconds": measurement.summarize_samples(reference_cpu),
        "candidate_cpu_seconds": measurement.summarize_samples(candidate_cpu),
        "reference_peak_working_set_bytes": measurement.summarize_samples(reference_peak),
        "candidate_peak_working_set_bytes": measurement.summarize_samples(candidate_peak),
        "paired_wall_ratios_candidate_over_reference": measurement.summarize_samples(paired_ratios),
        "point_ratio_candidate_median_over_reference_median": (
            candidate_score_summary["median"] / reference_score_summary["median"]
        ),
        "noise_status": (
            "INCONCLUSIVE"
            if reference_score_summary["status"] == "INCONCLUSIVE"
            or candidate_score_summary["status"] == "INCONCLUSIVE"
            else "VALID"
        ),
    }
    if mode == "DEVELOPMENT_ONLY":
        summary["regression"] = {
            "status": "DEVELOPMENT_ONLY",
            "reason": "five pairs are diagnostic only; the 5% formal gate requires at least ten",
            "resamples": measurement.DEFAULT_BOOTSTRAP_RESAMPLES,
            "seed": measurement.DEFAULT_BOOTSTRAP_SEED,
        }
        case_report["status"] = "DEVELOPMENT_ONLY"
    elif summary["noise_status"] == "INCONCLUSIVE":
        summary["regression"] = {
            "status": "INCONCLUSIVE",
            "reason": "reference or candidate wall IQR/median exceeds 10%",
            "resamples": measurement.DEFAULT_BOOTSTRAP_RESAMPLES,
            "seed": measurement.DEFAULT_BOOTSTRAP_SEED,
        }
        case_report["status"] = "INCONCLUSIVE"
    else:
        summary["regression"] = measurement.paired_bootstrap_regression(
            reference_scores, candidate_scores
        )
        case_report["status"] = summary["regression"]["status"]
    case_report["summary"] = summary


def _case_set_metadata(selected_cases: Sequence[str]) -> dict[str, Any]:
    selected = set(selected_cases)
    selected_generic = [case for case in GENERIC_CASES if case in selected]
    missing_generic = [case for case in GENERIC_CASES if case not in selected]
    return {
        "required_generic_cases": list(GENERIC_CASES),
        "selected_generic_cases": selected_generic,
        "missing_generic_cases": missing_generic,
        "complete_generic_eight": not missing_generic,
        "selected_optional_cases": [case for case in OPTIONAL_CASES if case in selected],
    }


def _generic_eight_status(
    *, mode: str, completeness: dict[str, Any], cases: dict[str, Any], integrity_failure: bool = False
) -> str:
    if not completeness["complete_generic_eight"]:
        return "NOT_RUN_COMPLETE_SET"
    if integrity_failure:
        return "FAILED"
    statuses = [cases[case]["status"] for case in GENERIC_CASES]
    if mode == "DEVELOPMENT_ONLY":
        if all(status == "DEVELOPMENT_ONLY" for status in statuses):
            return "DEVELOPMENT_ONLY"
    if "FAILED" in statuses:
        return "FAILED"
    if "INCOMPLETE" in statuses:
        return "FAILED"
    if "FAIL" in statuses:
        return "FAIL"
    if "INCONCLUSIVE" in statuses:
        return "INCONCLUSIVE"
    if all(status == "PASS" for status in statuses):
        return "PASS"
    return "INCONCLUSIVE"


def run_campaign(
    *,
    reference_bin: str | os.PathLike[str],
    candidate_bin: str | os.PathLike[str],
    reference_commit: str,
    candidate_commit: str,
    output_dir: str | os.PathLike[str],
    pairs: int = 10,
    cpu_index: int,
    cases: Sequence[str] | None = None,
    include_rolling_engine: bool = False,
    timeout: float = 1800.0,
) -> dict[str, Any]:
    mode = validate_pair_count(pairs)
    selected_cases = select_cases(cases, include_rolling_engine=include_rolling_engine)
    for label, commit in (("reference", reference_commit), ("candidate", candidate_commit)):
        if not isinstance(commit, str) or not _COMMIT_RE.fullmatch(commit):
            raise ValueError(f"{label}_commit must be a full 40-64 character git commit hash")
    if isinstance(cpu_index, bool) or not isinstance(cpu_index, int) or cpu_index < 0:
        raise ValueError("cpu must be a non-negative logical CPU index")
    if (
        isinstance(timeout, bool)
        or not isinstance(timeout, (int, float))
        or not math.isfinite(timeout)
        or timeout <= 0
    ):
        raise ValueError("timeout must be a finite positive number")

    source_binaries = {
        "reference": Path(reference_bin).resolve(strict=True),
        "candidate": Path(candidate_bin).resolve(strict=True),
    }
    for label, binary in source_binaries.items():
        if not binary.is_file():
            raise ValueError(f"{label} binary must be a file: {binary}")
    source_hashes = {label: sha256_file(path) for label, path in source_binaries.items()}
    target_dir = Path(output_dir).resolve()
    if target_dir.exists():
        raise FileExistsError(f"campaign output directory already exists: {target_dir}")
    target_dir.mkdir(parents=True, exist_ok=False)
    binary_dir = target_dir / "binaries"
    binary_dir.mkdir()
    archive_binaries = {
        label: binary_dir / f"{label}{path.suffix or '.exe'}"
        for label, path in source_binaries.items()
    }
    binary_hashes = source_hashes.copy()
    for label, source in source_binaries.items():
        shutil.copy2(source, archive_binaries[label])
        if sha256_file(archive_binaries[label]) != source_hashes[label]:
            raise CampaignError(f"{label} executable changed while its campaign copy was made")
    # Execute the original build output so Windows can resolve adjacent DLLs;
    # the byte-identical copy is retained with the results for reproduction.
    binaries = source_binaries
    for case in selected_cases:
        (target_dir / "cases" / case / "runs").mkdir(parents=True)
        (target_dir / "cases" / case / "scratch").mkdir()
    completeness = _case_set_metadata(selected_cases)

    result: dict[str, Any] = {
        "schema_version": 1,
        "protocol": measurement.MEASUREMENT_PROTOCOL,
        "created_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "status": "RUNNING",
        "mode": mode,
        "pairs": pairs,
        "cpu_index_requested": cpu_index,
        "inner_repeats": INNER_REPEATS,
        "inner_warmups": INNER_WARMUPS,
        "warmup_order": [list(order) for order in measurement.warmup_order_schedule()],
        "measured_order": [list(order) for order in measurement.pair_order_schedule(pairs)],
        "order_labels": {"A": "reference", "B": "candidate"},
        "case_names": list(selected_cases),
        "case_set_completeness": completeness,
        "generic_eight_status": (
            "RUNNING" if completeness["complete_generic_eight"] else "NOT_RUN_COMPLETE_SET"
        ),
        "binary_identities": {
            label: {
                "path": str(path),
                "archive_path": str(archive_binaries[label]),
                "commit": reference_commit if label == "reference" else candidate_commit,
                "sha256_before": binary_hashes[label],
                "archive_sha256_before": source_hashes[label],
            }
            for label, path in binaries.items()
        },
        "campaign_runner_sha256": sha256_file(Path(__file__)),
        "measurement_module_sha256": sha256_file(Path(measurement.__file__)),
        "cases": {
            case: {
                "status": "RUNNING",
                "scoring_scope": (
                    _ROLLING_SCORING_SCOPE if case == "rolling-engine"
                    else _PROCESS_SCORING_SCOPE
                ),
                "scoring_timing_field": (
                    _ROLLING_SCORING_FIELD if case == "rolling-engine"
                    else _PROCESS_SCORING_FIELD
                ),
                "runs": [],
            }
            for case in selected_cases
        },
    }
    report_path = target_dir / "campaign.json"
    _write_campaign(report_path, result)
    failure: str | None = None
    failure_status: str | None = None
    integrity_failure = False
    active_case_report: dict[str, Any] | None = None
    try:
        for case_name in selected_cases:
            case_report = result["cases"][case_name]
            active_case_report = case_report
            semantic_oracle: dict[str, Any] | None = None
            schedules = (
                ("warmup", measurement.warmup_order_schedule()),
                ("measured", measurement.pair_order_schedule(pairs)),
            )
            for phase, schedule in schedules:
                for pair_index, order in enumerate(schedule, start=1):
                    for order_index, order_label in enumerate(order, start=1):
                        side = result["order_labels"][order_label]
                        try:
                            record, payload = _run_one(
                                binary=binaries[side],
                                binary_digest=binary_hashes[side],
                                case_name=case_name,
                                side=side,
                                phase=phase,
                                pair_index=pair_index,
                                order_index=order_index,
                                output_dir=target_dir,
                                cpu_index=cpu_index,
                                timeout=timeout,
                            )
                            if semantic_oracle is None and payload is not None:
                                if side != "reference":
                                    raise CampaignError("the first semantic oracle must be a reference run")
                                semantic_oracle = payload
                                record["semantic_comparison"] = {"status": "REFERENCE_ORACLE"}
                            elif payload is not None and semantic_oracle is not None:
                                comparison = compare_semantic_outputs(case_name, semantic_oracle, payload)
                                record["semantic_comparison"] = comparison
                            case_report["runs"].append(record)
                            _write_campaign(report_path, result)
                            if record.get("error"):
                                raise CampaignError(str(record["error"]))
                            if record.get("semantic_comparison", {}).get("status") == "MISMATCH":
                                raise CampaignError(
                                    f"{side} {case_name} semantic mismatch: "
                                    f"{record['semantic_comparison']['mismatches']}"
                                )
                        except Exception as error:
                            # Persist the failing phase/side without losing earlier raw runs.
                            case_report["failure"] = f"{type(error).__name__}: {error}"
                            case_report["status"] = "FAILED"
                            if not any(
                                run.get("phase") == phase
                                and run.get("pair_index") == pair_index
                                and run.get("order_index") == order_index
                                for run in case_report["runs"]
                            ):
                                case_report["runs"].append({
                                    "side": side,
                                    "phase": phase,
                                    "pair_index": pair_index,
                                    "order_index": order_index,
                                    "error": f"{type(error).__name__}: {error}",
                                })
                            _write_campaign(report_path, result)
                            raise
            _case_summary(case_report, pairs, mode, case_name=case_name)
            _write_campaign(report_path, result)
            if case_report["status"] in ("FAIL", "INCONCLUSIVE", "INCOMPLETE"):
                failure = failure or f"{case_name}: regression status {case_report['status']}"
                case_failure = (
                    "FAILED" if case_report["status"] == "INCOMPLETE"
                    else case_report["status"]
                )
                priority = {None: 0, "INCONCLUSIVE": 1, "FAIL": 2, "FAILED": 3}
                if priority[case_failure] > priority[failure_status]:
                    failure_status = case_failure
    except Exception as error:
        failure = failure or f"{type(error).__name__}: {error}"
        failure_status = "FAILED"
        if active_case_report is not None and active_case_report["status"] == "RUNNING":
            active_case_report["status"] = "FAILED"
            active_case_report["failure"] = failure
    finally:
        for label, binary in binaries.items():
            try:
                after_digest = sha256_file(binary)
            except OSError as error:
                result["binary_identities"][label]["sha256_after"] = None
                failure = failure or f"could not hash {label} binary after campaign: {error}"
                failure_status = failure_status or "FAILED"
                integrity_failure = True
                continue
            result["binary_identities"][label]["sha256_after"] = after_digest
            if after_digest != binary_hashes[label]:
                failure = failure or f"{label} binary changed during campaign"
                failure_status = failure_status or "FAILED"
                integrity_failure = True
            try:
                archive_after_digest = sha256_file(archive_binaries[label])
            except OSError as error:
                result["binary_identities"][label]["archive_sha256_after"] = None
                failure = failure or f"could not hash archived {label} binary after campaign: {error}"
                failure_status = failure_status or "FAILED"
                integrity_failure = True
                continue
            result["binary_identities"][label]["archive_sha256_after"] = archive_after_digest
            if archive_after_digest != source_hashes[label]:
                failure = failure or f"archived {label} binary changed during campaign"
                failure_status = failure_status or "FAILED"
                integrity_failure = True

    for case_report in result["cases"].values():
        if case_report["status"] == "RUNNING" and failure is not None:
            case_report["status"] = "NOT_RUN_AFTER_EARLIER_FAILURE"
    completeness = _case_set_metadata(selected_cases)
    result["case_set_completeness"] = completeness
    result["generic_eight_status"] = _generic_eight_status(
        mode=mode,
        completeness=completeness,
        cases=result["cases"],
        integrity_failure=integrity_failure,
    )
    if failure_status is not None:
        result["status"] = failure_status
    elif mode == "DEVELOPMENT_ONLY":
        result["status"] = "DEVELOPMENT_ONLY"
    elif not completeness["complete_generic_eight"]:
        result["status"] = "CASE_SUBSET_PASS"
    elif result["generic_eight_status"] == "PASS":
        result["status"] = "PASS"
    else:
        result["status"] = "INCONCLUSIVE"
    if failure:
        result["failure"] = failure
    _write_campaign(report_path, result)
    return result


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-bin", required=True, help="immutable P112/reference benchmark executable")
    parser.add_argument("--candidate-bin", required=True, help="immutable candidate benchmark executable")
    parser.add_argument("--reference-commit", required=True, help="full source commit used to build reference binary")
    parser.add_argument("--candidate-commit", required=True, help="full source commit used to build candidate binary")
    parser.add_argument("--output-dir", required=True, help="new, non-existing directory for this campaign")
    parser.add_argument("--pairs", type=int, default=10, help="5 is development-only; formal batches require even >=10")
    parser.add_argument("--cpu", type=int, required=True, help="logical CPU index shared by both binaries")
    parser.add_argument("--case", action="append", choices=CASE_CHOICES,
                        help="explicit case subset; omit to run all generic eight")
    parser.add_argument("--include-rolling-engine", action="store_true",
                        help="add the separate rolling-engine case to the default generic eight")
    parser.add_argument("--timeout", type=float, default=1800.0,
                        help="per-process timeout in seconds (default: 1800)")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        result = run_campaign(
            reference_bin=args.reference_bin,
            candidate_bin=args.candidate_bin,
            reference_commit=args.reference_commit,
            candidate_commit=args.candidate_commit,
            output_dir=args.output_dir,
            pairs=args.pairs,
            cpu_index=args.cpu,
            cases=args.case,
            include_rolling_engine=args.include_rolling_engine,
            timeout=args.timeout,
        )
    except (OSError, ValueError, CampaignError) as error:
        print(f"paired benchmark campaign failed: {error}", file=sys.stderr)
        return 2
    print(json.dumps({
        "status": result["status"],
        "output_dir": str(Path(args.output_dir).resolve()),
        "pairs": args.pairs,
        "cases": list(result["case_names"]),
    }, indent=2, sort_keys=True))
    if result["status"] == "FAILED":
        return 2
    return 1 if result["status"] in ("FAIL", "INCONCLUSIVE") else 0


if __name__ == "__main__":
    raise SystemExit(main())
