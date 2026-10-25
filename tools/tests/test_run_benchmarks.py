from __future__ import annotations

import json
import sys
import statistics
import tempfile
import types
import unittest
from unittest import mock
from pathlib import Path

from tools.run_benchmarks import (
    PINNED_PYTHON_SOURCE_HEAD,
    compare_results,
    compare_semantic_values,
    imported_fincore_module_paths,
    pinned_python_source_identity,
    _validate_native_instrumentation,
    _json_load,
)


def count_summary(samples: list[int]) -> dict:
    return {
        "samples": samples,
        "median": statistics.median(samples),
        "min": min(samples),
        "max": max(samples),
        "spread": max(samples) - min(samples),
    }


def measurement_summary(samples: list[float]) -> dict:
    return {
        "samples_ms": samples,
        "median_ms": statistics.median(samples),
        "min_ms": min(samples),
        "max_ms": max(samples),
        "spread_ms": max(samples) - min(samples),
        "stage_invocations_per_sample": 1,
        "global_new_calls": count_summary([10, 10, 11, 9, 10, 11, 10]),
        "requested_bytes": count_summary([100, 101, 100, 99, 100, 101, 100]),
    }


def synthetic_result(median: float = 100.0, rss: list[float] | None = None) -> dict:
    samples = [median - 1.0, median + 1.0, median - 0.5, median + 0.5, median, median, median]
    rss_values = rss if rss is not None else [100.0, 101.0, 100.5]
    return {
        "schema_version": 2,
        "source_schema": "factor-benchmark-v5",
        "digest_scheme": "fnv1a64-canonical-decimal-9",
        "suite_status": "PASS",
        "platform": {"system": "Linux", "machine": "x86_64", "release": "test"},
        "build_compatibility": {"compiler": {"id": "Clang", "version": "1"}, "build_type": "Release"},
        "source_identity": {
            "python": {
                "git_head": PINNED_PYTHON_SOURCE_HEAD,
                "tracked_scope_dirty": False,
                "sha256": "a" * 64,
            },
            "runner": {"sha256": "b" * 64},
        },
        "runner_sha256": "b" * 64,
        "native_binary_sha256": "c" * 64,
        "cases": {
            "scalar-sharpe": {
                "native": {
                    "status": "PASS",
                    "case": "scalar-sharpe",
                    "source_schema": "factor-benchmark-v5",
                    "workload": {"id": "scalar-sharpe-v1", "rows": 100},
                    "input_digest": "fnv1a64:input",
                    "repeats": 7,
                    "warmups": 2,
                    "measurements_ms": {"compute": measurement_summary(samples)},
                    "details": {"semantic_values": [1.0]},
                    "rss_samples_mb": rss_values,
                    "rss_median_mb": statistics.median(rss_values),
                    "rss_spread_mb": max(rss_values) - min(rss_values),
                }
            }
        },
    }


class CompareResultsTests(unittest.TestCase):
    def test_profiled_and_unprofiled_runs_cannot_be_compared(self) -> None:
        current, baseline = synthetic_result(), synthetic_result()
        current["cases"]["scalar-sharpe"]["native"]["allocation_instrumentation"] = {"enabled": False}
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("incompatible allocation profiling modes" in reason
                            for reason in comparison["reasons"]))

    def test_disabled_allocation_fields_are_not_interpreted_as_measurements(self) -> None:
        current, baseline = synthetic_result(), synthetic_result()
        for result in (current, baseline):
            result["cases"]["scalar-sharpe"]["native"]["allocation_instrumentation"] = {"enabled": False}
            stage = result["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"]
            stage["global_new_calls"] = None
            stage["requested_bytes"] = None
        stage = current["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"]
        stage["global_new_calls"] = count_summary([9999] * 7)
        stage["requested_bytes"] = count_summary([99999] * 7)
        self.assertEqual(compare_results(current, baseline)["status"], "PASS")

    def test_native_stage_instrumentation_requires_explicit_copy_and_kernel_counts(self) -> None:
        native = {
            "allocation_instrumentation": {
                "counted_calls": "successful global operator new/new[] and aligned forms",
                "bytes": "cumulative requested bytes, not live bytes or RSS",
            },
            "details": {
                "model_copy_invocations_per_measured_sample": 1,
                "factor_compute_diagnostics": {"available": False},
            },
        }
        _validate_native_instrumentation(native, "dense", 7)

        native["details"]["factor_compute_diagnostics"] = {
            "available": True,
            "weight_kernel_calls_per_analyze": [1] * 7,
            "information_coefficient_kernel_calls_per_analyze": [2] * 7,
        }
        _validate_native_instrumentation(native, "dense", 7)
        native["details"]["model_copy_invocations_per_measured_sample"] = 2
        with self.assertRaisesRegex(RuntimeError, "exactly one model copy"):
            _validate_native_instrumentation(native, "dense", 7)

    def test_semantic_values_use_tight_float_tolerance_and_preserve_nan_positions(self) -> None:
        equivalent = compare_semantic_values([None, 0.25, -2.0], [None, 0.2500000000001, -2.0])
        self.assertEqual(equivalent["status"], "PASS")
        self.assertLessEqual(equivalent["max_absolute_error"], 1e-12)

        different = compare_semantic_values([None, 0.25], [0.0, 0.25])
        self.assertEqual(different["status"], "MISMATCH")

    def test_compatible_run_passes(self) -> None:
        result = compare_results(synthetic_result(), synthetic_result())
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["cases"]["scalar-sharpe"]["status"], "PASS")

    def test_timing_and_rss_regressions_are_reported(self) -> None:
        baseline = synthetic_result(100.0, [100.0, 100.5, 100.25])
        current = synthetic_result(114.0, [116.0, 115.0, 115.5])
        result = compare_results(current, baseline)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any("compute median regression" in reason for reason in result["reasons"]))
        self.assertTrue(any("RSS median regression" in reason for reason in result["reasons"]))

    def test_allocation_regression_is_measured_beyond_noise(self) -> None:
        baseline = synthetic_result()
        current = synthetic_result()
        metric = current["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"]
        metric["global_new_calls"] = count_summary([114, 115, 114, 113, 114, 115, 114])
        result = compare_results(current, baseline)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any("global_new_calls median regression" in reason for reason in result["reasons"]))

    def test_regression_inside_measured_spread_passes(self) -> None:
        baseline = synthetic_result(100.0)
        current = synthetic_result(109.0)
        measurement = current["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"]
        measurement["samples_ms"] = [103.0, 115.0, 105.0, 113.0, 109.0, 109.0, 109.0]
        measurement["min_ms"] = 103.0
        measurement["max_ms"] = 115.0
        measurement["spread_ms"] = 12.0
        result = compare_results(current, baseline)
        self.assertEqual(result["status"], "PASS")

    def test_platform_and_workload_mismatch_fail_closed(self) -> None:
        baseline = synthetic_result()
        current = synthetic_result()
        current["platform"]["release"] = "different"
        current["cases"]["scalar-sharpe"]["native"]["workload"]["rows"] = 101
        result = compare_results(current, baseline)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any("incompatible platform" == reason for reason in result["reasons"]))
        self.assertTrue(any("incompatible native workload" in reason for reason in result["reasons"]))

    def test_missing_case_stage_or_rss_samples_fail_closed(self) -> None:
        baseline = synthetic_result()
        current = synthetic_result()
        current["cases"].pop("scalar-sharpe")
        result = compare_results(current, baseline)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any("case set mismatch" in reason for reason in result["reasons"]))

        current = synthetic_result()
        current_native = current["cases"]["scalar-sharpe"]["native"]
        current_native["measurements_ms"].pop("compute")
        current_native["rss_samples_mb"] = [100.0]
        result = compare_results(current, baseline)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any("measurement map is missing" in reason for reason in result["reasons"]))
        self.assertTrue(any("fewer than 3 independent process samples" in reason for reason in result["reasons"]))

        current = synthetic_result()
        current["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"].pop("requested_bytes")
        result = compare_results(current, baseline)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any("requested_bytes is missing" in reason for reason in result["reasons"]))

    def test_nan_json_is_rejected_and_nan_measurement_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nan.json"
            path.write_text('{"value": NaN}', encoding="utf-8")
            with self.assertRaises(ValueError):
                _json_load(path)

        baseline = synthetic_result()
        current = synthetic_result()
        current["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"]["median_ms"] = float("nan")
        result = compare_results(current, baseline)
        self.assertEqual(result["status"], "FAIL")
        self.assertTrue(any("missing or non-finite comparison values" in reason for reason in result["reasons"]))

    def test_missing_equal_native_identity_fields_fail_closed(self) -> None:
        baseline = synthetic_result()
        current = synthetic_result()
        for result in (baseline, current):
            native = result["cases"]["scalar-sharpe"]["native"]
            native["workload"] = None
            native["input_digest"] = None
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("workload is missing or empty" in reason for reason in comparison["reasons"]))
        self.assertTrue(any("input_digest is missing or empty" in reason for reason in comparison["reasons"]))

    def test_corrupt_summary_and_boolean_samples_fail_closed(self) -> None:
        baseline = synthetic_result()
        current = synthetic_result()
        measurement = current["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"]
        measurement["spread_ms"] = 500.0
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("spread_ms summary does not match samples" in reason for reason in comparison["reasons"]))

        current = synthetic_result()
        current["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"]["samples_ms"][0] = True
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("contains missing or non-finite samples" in reason for reason in comparison["reasons"]))

        current = synthetic_result()
        current["cases"]["scalar-sharpe"]["native"]["measurements_ms"]["compute"]["global_new_calls"]["samples"][0] = True
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("global_new_calls samples must be nonnegative integers" in reason for reason in comparison["reasons"]))

        current = synthetic_result()
        current["cases"]["scalar-sharpe"]["native"]["rss_median_mb"] = 900.0
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("rss_median_mb summary does not match samples" in reason for reason in comparison["reasons"]))

        current = synthetic_result()
        current["cases"]["scalar-sharpe"]["native"]["rss_samples_mb"][0] = True
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("RSS samples contain missing or non-finite values" in reason for reason in comparison["reasons"]))

    def test_runner_and_python_source_identity_mismatch_fail_closed(self) -> None:
        baseline = synthetic_result()
        current = synthetic_result()
        current["source_identity"]["runner"]["sha256"] = "d" * 64
        current["runner_sha256"] = "d" * 64
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertIn("incompatible benchmark runner source hash", comparison["reasons"])

        current = synthetic_result()
        current["source_identity"]["python"]["sha256"] = "e" * 64
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertIn("incompatible pinned Python source hash", comparison["reasons"])

    def test_native_shared_semantic_outputs_must_match_baseline(self) -> None:
        baseline = synthetic_result()
        current = synthetic_result()
        current["cases"]["scalar-sharpe"]["native"]["details"]["semantic_values"] = [1.01]
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("native semantic output differs from baseline" in reason for reason in comparison["reasons"]))

        for result in (baseline, current):
            result["cases"] = {
                "dense": {
                    "native": {
                        "status": "PASS", "case": "dense", "source_schema": "factor-benchmark-v5",
                        "workload": {"id": "dense-v1"}, "input_digest": "fnv1a64:input",
                        "warmups": 2, "repeats": 7,
                        "measurements_ms": {"compute": measurement_summary([1.0] * 7)},
                        "rss_samples_mb": [100.0, 100.0, 100.0],
                        "rss_median_mb": 100.0, "rss_spread_mb": 0.0,
                        "details": {"factor_returns_digest": "fnv1a64:returns"},
                    }
                }
            }
        current["cases"]["dense"]["native"]["details"]["factor_returns_digest"] = "fnv1a64:changed"
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("factor-return digest differs" in reason for reason in comparison["reasons"]))

        for result in (baseline, current):
            result["cases"] = {
                "portfolio": {
                    "native": {
                        "status": "PASS", "case": "portfolio", "source_schema": "factor-benchmark-v5",
                        "workload": {"id": "portfolio-v1"}, "input_digest": "fnv1a64:input",
                        "warmups": 2, "repeats": 7,
                        "measurements_ms": {"weights": measurement_summary([1.0] * 7)},
                        "rss_samples_mb": [100.0, 100.0, 100.0],
                        "rss_median_mb": 100.0, "rss_spread_mb": 0.0,
                        "details": {"weights_digest": "fnv1a64:weights", "returns_digest": "fnv1a64:returns"},
                    }
                }
            }
        current["cases"]["portfolio"]["native"]["details"]["weights_digest"] = "fnv1a64:changed"
        comparison = compare_results(current, baseline)
        self.assertEqual(comparison["status"], "FAIL")
        self.assertTrue(any("portfolio weights_digest differs" in reason for reason in comparison["reasons"]))

    def test_python_source_pin_and_import_paths_fail_closed(self) -> None:
        identity = {"git_head": PINNED_PYTHON_SOURCE_HEAD, "tracked_scope_dirty": False, "sha256": "a" * 64}
        with mock.patch("tools.run_benchmarks.source_identity", return_value=identity):
            self.assertEqual(pinned_python_source_identity(Path("/fake")), identity)
        with mock.patch("tools.run_benchmarks.source_identity", return_value={**identity, "git_head": "wrong"}):
            with self.assertRaisesRegex(ValueError, "must be pinned"):
                pinned_python_source_identity(Path("/fake"))

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            names = (
                "fincore", "fincore.factor_analysis.analysis", "fincore.factor_analysis.performance",
                "fincore.metrics.ratios", "fincore.metrics.rolling",
            )
            modules = {
                name: types.ModuleType(name)
                for name in names
            }
            for index, module in enumerate(modules.values()):
                path = root / "fincore" / f"module_{index}.py"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
                module.__file__ = str(path)
            with mock.patch.dict(sys.modules, modules):
                self.assertEqual(set(imported_fincore_module_paths(root)), set(names))
                modules[names[-1]].__file__ = str(Path(directory).parent / "outside.py")
                with self.assertRaisesRegex(RuntimeError, "outside --python-source"):
                    imported_fincore_module_paths(root)


if __name__ == "__main__":
    unittest.main()
