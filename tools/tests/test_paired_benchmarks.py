from __future__ import annotations

import json
import statistics
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from benchmarks.alpha003_a_stocks import measurement
from tools import run_paired_benchmarks as paired


def benchmark_result(
    case: str = "scalar-sharpe",
    *,
    values: list[float | None] | None = None,
    output_digest: str = "fnv1a64:output-a",
) -> dict:
    samples = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0]
    details: dict
    if case in paired.RETURNS_CASES:
        semantic = [1.0, None, 2.0] if values is None else values
        details = {"output_count": len(semantic), "semantic_values": semantic}
        workload = {"id": case, "rows": 3, "columns": 1}
    elif case == "portfolio":
        details = {
            "rows": 6,
            "weight_rows": 3,
            "return_values": 3,
            "weights_digest": "fnv1a64:weights-a",
            "returns_digest": "fnv1a64:returns-a",
        }
        workload = {"id": "factor-portfolio-v1", "dates": 2, "assets": 3}
    elif case in paired.PANEL_CASES:
        details = {
            "rows": 6,
            "model_fingerprint": "model-a",
            "model_copy_invocations_per_measured_sample": 1,
            "factor_returns_digest": "fnv1a64:returns-a",
            "html_bytes": 100,
            "xlsx_bytes": 200,
            "html_digest": "sha256:html-a",
            "xlsx_digest": "sha256:xlsx-a",
            "factor_compute_diagnostics": {"available": False},
        }
        workload = {"id": "factor-analysis-panel-v1", "dates": 2, "assets": 3}
    elif case == "rolling-engine":
        names = sorted(paired.ROLLING_METRICS)
        index = [1_000_000_000_000_000_000, 1_000_000_000_000_000_001]
        details = {
            "output_count": {name: 2 for name in names},
            "metrics": {
                name: {
                    "index": list(index),
                    "values": [
                        "inf" if name == "sharpe" else
                        "-inf" if name == "beta" else 1.0,
                        None,
                    ],
                }
                for name in names
            },
            "metric_output_digests": {name: f"fnv1a64:{name}" for name in names},
            "oracle": "scalar-api",
            "oracle_in_measurement": False,
        }
        workload = {"id": "rolling-engine-v1", "rows": 50000, "window": 252}
    else:
        raise AssertionError(case)

    checks = {"output_count_valid": "true"}
    if case == "rolling-engine":
        checks.update({
            "metric_names_valid": "true",
            "oracle_indexes_valid": "true",
            "oracle_nan_masks_valid": "true",
            "oracle_values_valid": "true",
            "output_stable_across_repeats": "true",
        })
    timing_samples = {
        stage: {
            "samples_ms": samples,
            "median_ms": statistics.median(samples),
            "min_ms": min(samples),
            "max_ms": max(samples),
            "spread_ms": max(samples) - min(samples),
            "stage_invocations_per_sample": 1,
        }
        for stage in paired.REQUIRED_TIMING_STAGES[case]
    }
    return {
        "schema_version": 3,
        "source_schema": "factor-benchmark-v5",
        "case": case,
        "digest_scheme": "fnv1a64-canonical-decimal-9",
        "workload": workload,
        "warmups": 0,
        "repeats": 7,
        "input_digest": "fnv1a64:input-a",
        "output_digest": output_digest,
        "measurements_ms": timing_samples,
        "checks": checks,
        "details": details,
        "rss_peak_mb": 10.0,
        "allocation_instrumentation": {"enabled": False},
        "environment": {"platform": "Windows", "compiler": "MSVC"},
    }


class PairProtocolTests(unittest.TestCase):
    def test_default_is_generic_eight_and_rolling_engine_is_opt_in(self) -> None:
        self.assertEqual(paired.select_cases(None), paired.GENERIC_CASES)
        self.assertEqual(paired.select_cases(["rolling-engine"]), ("rolling-engine",))
        self.assertEqual(paired.select_cases(["all"]), paired.GENERIC_CASES)
        self.assertEqual(
            paired.select_cases(None, include_rolling_engine=True),
            (*paired.GENERIC_CASES, "rolling-engine"),
        )
        with self.assertRaisesRegex(ValueError, "cannot be combined"):
            paired.select_cases(["all", "rolling-engine"])

    def test_five_pairs_are_development_and_formal_counts_are_even(self) -> None:
        self.assertEqual(paired.validate_pair_count(5), "DEVELOPMENT_ONLY")
        self.assertEqual(paired.validate_pair_count(10), "FORMAL")
        self.assertEqual(paired.validate_pair_count(12), "FORMAL")
        for count in (0, 6, 9, 11):
            with self.subTest(count=count), self.assertRaises(ValueError):
                paired.validate_pair_count(count)

    def test_formal_schedule_alternates_pair_orientation(self) -> None:
        self.assertEqual(
            measurement.pair_order_schedule(5),
            (("A", "B"), ("B", "A"), ("A", "B"), ("B", "A"), ("A", "B")),
        )
        self.assertEqual(measurement.warmup_order_schedule(), (("A", "B"), ("B", "A")))


class SemanticValidationTests(unittest.TestCase):
    def test_returns_compare_full_values_with_tolerance_and_nan_mask(self) -> None:
        reference = benchmark_result()
        candidate = benchmark_result(
            values=[1.0 + 1e-13, None, 2.0], output_digest="fnv1a64:rounded-differently"
        )
        result = paired.compare_semantic_outputs("scalar-sharpe", reference, candidate)
        self.assertEqual(result["status"], "PASS")
        self.assertIsNone(result["aggregate_output_digest_equal_diagnostic_only"])
        different_nan_mask = benchmark_result(values=[1.0, 0.0, 2.0])
        self.assertEqual(
            paired.compare_semantic_outputs("scalar-sharpe", reference, different_nan_mask)["status"],
            "MISMATCH",
        )

    def test_failed_process_checks_and_enabled_allocation_profile_are_rejected(self) -> None:
        payload = benchmark_result()
        payload["checks"]["output_count_valid"] = "false"
        with self.assertRaisesRegex(paired.CampaignError, "correctness checks failed"):
            paired.validate_benchmark_result(payload, "scalar-sharpe")
        payload = benchmark_result()
        payload["allocation_instrumentation"]["enabled"] = True
        with self.assertRaisesRegex(paired.CampaignError, "allocation profiling explicitly disabled"):
            paired.validate_benchmark_result(payload, "scalar-sharpe")

    def test_comparison_rejects_different_compiler_environment(self) -> None:
        reference = benchmark_result()
        candidate = benchmark_result()
        candidate["environment"]["compiler"] = "different compiler"
        result = paired.compare_semantic_outputs("scalar-sharpe", reference, candidate)
        self.assertEqual(result["status"], "MISMATCH")
        self.assertIn("environment", result["mismatches"])

    def test_protocol_integer_fields_reject_boolean_aliases(self) -> None:
        payload = benchmark_result()
        payload["warmups"] = False
        with self.assertRaisesRegex(paired.CampaignError, "fixed inner warmup/repeat"):
            paired.validate_benchmark_result(payload, "scalar-sharpe")
        payload = benchmark_result()
        payload["measurements_ms"]["compute"]["stage_invocations_per_sample"] = True
        with self.assertRaisesRegex(paired.CampaignError, "stage invocation count must be an integer"):
            paired.validate_benchmark_result(payload, "scalar-sharpe")

    def test_each_case_requires_its_exact_native_timing_stages(self) -> None:
        panel = benchmark_result("dense")
        del panel["measurements_ms"]["file_io"]
        with self.assertRaisesRegex(paired.CampaignError, "timing stages.*file_io"):
            paired.validate_benchmark_result(panel, "dense")

        portfolio = benchmark_result("portfolio")
        del portfolio["measurements_ms"]["portfolio_returns"]
        with self.assertRaisesRegex(paired.CampaignError, "portfolio_returns"):
            paired.validate_benchmark_result(portfolio, "portfolio")

    def test_panel_uses_component_outputs_and_ignores_diagnostic_counters(self) -> None:
        reference = benchmark_result("dense")
        candidate = benchmark_result("dense", output_digest="fnv1a64:aggregate-changed")
        candidate["details"]["factor_compute_diagnostics"] = {
            "available": True,
            "weight_kernel_calls_per_analyze": [1, 2, 3],
        }
        self.assertEqual(
            paired.compare_semantic_outputs("dense", reference, candidate)["status"],
            "PASS",
        )
        candidate = benchmark_result("dense")
        candidate["details"]["model_copy_invocations_per_measured_sample"] = 2
        self.assertEqual(
            paired.compare_semantic_outputs("dense", reference, candidate)["status"],
            "MISMATCH",
        )
        candidate = benchmark_result("dense")
        candidate["details"]["factor_returns_digest"] = "fnv1a64:changed"
        self.assertEqual(
            paired.compare_semantic_outputs("dense", reference, candidate)["status"],
            "MISMATCH",
        )

    def test_portfolio_and_rolling_engine_compare_actual_business_outputs(self) -> None:
        reference = benchmark_result("portfolio")
        candidate = benchmark_result("portfolio", output_digest="fnv1a64:aggregate-diff")
        self.assertEqual(
            paired.compare_semantic_outputs("portfolio", reference, candidate)["status"],
            "PASS",
        )
        candidate["details"]["weights_digest"] = "fnv1a64:changed"
        self.assertEqual(
            paired.compare_semantic_outputs("portfolio", reference, candidate)["status"],
            "MISMATCH",
        )

        reference = benchmark_result("rolling-engine")
        candidate = benchmark_result("rolling-engine", output_digest="fnv1a64:rounded-differently")
        candidate["details"]["metrics"]["mean_return"]["values"][0] += 1e-13
        self.assertEqual(
            paired.compare_semantic_outputs("rolling-engine", reference, candidate)["status"],
            "PASS",
        )
        candidate["details"]["metrics"]["mean_return"]["index"][0] += 1
        self.assertEqual(
            paired.compare_semantic_outputs("rolling-engine", reference, candidate)["status"],
            "MISMATCH",
        )

    def test_rolling_engine_requires_five_complete_indexed_series(self) -> None:
        payload = benchmark_result("rolling-engine")
        del payload["details"]["metrics"]["beta"]["index"]
        with self.assertRaisesRegex(paired.CampaignError, "lacks index/values"):
            paired.validate_benchmark_result(payload, "rolling-engine")
        payload = benchmark_result("rolling-engine")
        payload["details"]["metrics"]["beta"]["values"][1] = "nan"
        with self.assertRaisesRegex(paired.CampaignError, "invalid semantic value"):
            paired.validate_benchmark_result(payload, "rolling-engine")


class CampaignTests(unittest.TestCase):
    def _fake_measurement(self, payload_for_side, calls):
        def run_command(command, **kwargs):
            command = tuple(str(part) for part in command)
            side = "reference" if Path(command[0]).name.startswith("reference") else "candidate"
            stdout_path = Path(kwargs["stdout_path"])
            stdout_path.write_text(json.dumps(payload_for_side(side)), encoding="utf-8")
            calls.append({"command": command, "stdout_path": stdout_path, "side": side})
            return measurement.CommandMeasurement(
                command=command,
                returncode=0,
                wall_seconds=1.0 if side == "reference" else 1.01,
                cpu_seconds=0.5,
                peak_working_set_bytes=1234,
                affinity_mask=1 << kwargs["cpu_index"],
                cpu_index=kwargs["cpu_index"],
                affinity_method="inherited_parent_affinity_before_create_process",
                affinity_verified=True,
                thread_environment=dict(measurement.THREAD_ENVIRONMENT),
            )

        return run_command

    def test_development_campaign_retains_14_raw_samples_and_unique_scratch_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            reference = root / "reference.exe"
            candidate = root / "candidate.exe"
            reference.write_bytes(b"reference")
            candidate.write_bytes(b"candidate")
            calls: list[dict] = []
            with mock.patch.object(
                paired.measurement,
                "run_command",
                side_effect=self._fake_measurement(lambda side: benchmark_result(), calls),
            ):
                result = paired.run_campaign(
                    reference_bin=reference,
                    candidate_bin=candidate,
                    reference_commit="a" * 40,
                    candidate_commit="b" * 40,
                    output_dir=root / "campaign",
                    pairs=5,
                    cpu_index=0,
                    cases=["scalar-sharpe"],
                )

            self.assertEqual(result["status"], "DEVELOPMENT_ONLY")
            case = result["cases"]["scalar-sharpe"]
            self.assertEqual(case["status"], "DEVELOPMENT_ONLY")
            self.assertEqual(len(calls), 14)
            self.assertEqual(
                [call["side"] for call in calls],
                ["reference", "candidate", "candidate", "reference"]
                + ["reference", "candidate", "candidate", "reference", "reference", "candidate",
                   "candidate", "reference", "reference", "candidate"],
            )
            scratch_paths = [
                call["command"][call["command"].index("--scratch-dir") + 1]
                for call in calls
            ]
            self.assertEqual(len(scratch_paths), len(set(scratch_paths)))
            self.assertTrue(all("--repeats" in call["command"] for call in calls))
            self.assertTrue(all(
                call["command"][call["command"].index("--repeats") + 1] == "7"
                and call["command"][call["command"].index("--warmups") + 1] == "0"
                for call in calls
            ))
            self.assertTrue(all(call["stdout_path"].is_file() for call in calls))
            campaign_binaries = result["binary_identities"]
            for side, original_bytes in (("reference", b"reference"), ("candidate", b"candidate")):
                copied = Path(campaign_binaries[side]["archive_path"])
                self.assertEqual(copied.read_bytes(), original_bytes)
                self.assertEqual(
                    campaign_binaries[side]["sha256_before"],
                    campaign_binaries[side]["sha256_after"],
                )
                self.assertEqual(
                    campaign_binaries[side]["archive_sha256_before"],
                    campaign_binaries[side]["archive_sha256_after"],
                )
            self.assertEqual(
                [run["measurement"]["wall_seconds"] for run in case["runs"] if run["phase"] == "measured" and run["side"] == "reference"],
                [1.0] * 5,
            )

    def test_formal_subset_is_never_reported_as_generic_eight_pass(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            reference = root / "reference.exe"
            candidate = root / "candidate.exe"
            reference.write_bytes(b"reference")
            candidate.write_bytes(b"candidate")
            calls: list[dict] = []
            with (
                mock.patch.object(
                    paired.measurement,
                    "run_command",
                    side_effect=self._fake_measurement(lambda side: benchmark_result(), calls),
                ),
                mock.patch.object(
                    paired.measurement,
                    "paired_bootstrap_regression",
                    return_value={"status": "PASS", "seed": 92026, "resamples": 10000},
                ) as bootstrap,
            ):
                result = paired.run_campaign(
                    reference_bin=reference,
                    candidate_bin=candidate,
                    reference_commit="a" * 40,
                    candidate_commit="b" * 40,
                    output_dir=root / "campaign",
                    pairs=10,
                    cpu_index=0,
                    cases=["scalar-sharpe"],
                )

            self.assertEqual(result["cases"]["scalar-sharpe"]["status"], "PASS")
            self.assertEqual(result["status"], "CASE_SUBSET_PASS")
            self.assertEqual(result["generic_eight_status"], "NOT_RUN_COMPLETE_SET")
            self.assertEqual(result["case_set_completeness"]["missing_generic_cases"], [
                case for case in paired.GENERIC_CASES if case != "scalar-sharpe"
            ])
            bootstrap.assert_called_once()
            self.assertEqual(len(bootstrap.call_args.args[0]), 10)
            self.assertEqual(len(bootstrap.call_args.args[1]), 10)

    def test_noisy_formal_case_is_inconclusive_without_running_bootstrap_gate(self) -> None:
        runs = []
        for index in range(10):
            runs.extend((
                {
                    "phase": "measured",
                    "side": "reference",
                    "measurement": {
                        "wall_seconds": float(index + 1),
                        "cpu_seconds": 0.5,
                        "peak_working_set_bytes": 1234,
                    },
                },
                {
                    "phase": "measured",
                    "side": "candidate",
                    "measurement": {
                        "wall_seconds": 1.0,
                        "cpu_seconds": 0.5,
                        "peak_working_set_bytes": 1234,
                    },
                },
            ))
        case_report = {"runs": runs}
        with mock.patch.object(paired.measurement, "paired_bootstrap_regression") as bootstrap:
            paired._case_summary(
                case_report, pairs=10, mode="FORMAL", case_name="scalar-sharpe"
            )
        self.assertEqual(case_report["status"], "INCONCLUSIVE")
        self.assertEqual(case_report["summary"]["noise_status"], "INCONCLUSIVE")
        self.assertEqual(case_report["summary"]["regression"]["status"], "INCONCLUSIVE")
        bootstrap.assert_not_called()

    def test_rolling_engine_scores_child_compute_medians_not_process_wall(self) -> None:
        runs = []
        for index in range(10):
            for side in ("reference", "candidate"):
                runs.append({
                    "phase": "measured",
                    "side": side,
                    "measurement": {
                        "wall_seconds": 1.0 if side == "reference" else 2.0,
                        "cpu_seconds": 0.5,
                        "peak_working_set_bytes": 1234,
                    },
                    "benchmark_timing_summaries_ms": {
                        "compute": {
                            "median_ms": 5.0 + (index * 0.01 if side == "reference" else 0.11)
                        }
                    },
                })
        case_report = {"runs": runs}
        expected_reference = [
            run["benchmark_timing_summaries_ms"]["compute"]["median_ms"]
            for run in runs if run["side"] == "reference"
        ]
        expected_candidate = [
            run["benchmark_timing_summaries_ms"]["compute"]["median_ms"]
            for run in runs if run["side"] == "candidate"
        ]
        with mock.patch.object(
            paired.measurement,
            "paired_bootstrap_regression",
            return_value={"status": "PASS", "seed": 92026, "resamples": 10000},
        ) as bootstrap:
            paired._case_summary(
                case_report, pairs=10, mode="FORMAL", case_name="rolling-engine"
            )
        bootstrap.assert_called_once_with(expected_reference, expected_candidate)
        self.assertEqual(case_report["status"], "PASS")
        summary = case_report["summary"]
        self.assertEqual(summary["scoring_scope"], "child_compute_stage")
        self.assertEqual(
            summary["scoring_timing_field"],
            "benchmark_timing_summaries_ms.compute.median_ms",
        )
        self.assertEqual(summary["reference_wall_seconds"]["median"], 1.0)
        self.assertEqual(summary["candidate_wall_seconds"]["median"], 2.0)

    def test_semantic_mismatch_is_saved_and_stops_before_next_run(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            reference = root / "reference.exe"
            candidate = root / "candidate.exe"
            reference.write_bytes(b"reference")
            candidate.write_bytes(b"candidate")
            calls: list[dict] = []

            def payload_for_side(side: str) -> dict:
                return benchmark_result(values=[1.0, None, 2.0] if side == "reference" else [2.0, None, 2.0])

            with mock.patch.object(
                paired.measurement,
                "run_command",
                side_effect=self._fake_measurement(payload_for_side, calls),
            ):
                result = paired.run_campaign(
                    reference_bin=reference,
                    candidate_bin=candidate,
                    reference_commit="a" * 40,
                    candidate_commit="b" * 40,
                    output_dir=root / "campaign",
                    pairs=5,
                    cpu_index=0,
                    cases=["scalar-sharpe"],
                )

            case = result["cases"]["scalar-sharpe"]
            self.assertEqual(result["status"], "FAILED")
            self.assertEqual(case["status"], "FAILED")
            self.assertEqual(len(calls), 2)
            self.assertEqual(case["runs"][-1]["semantic_comparison"]["status"], "MISMATCH")
            saved = json.loads((root / "campaign" / "campaign.json").read_text(encoding="utf-8"))
            self.assertEqual(saved["cases"]["scalar-sharpe"]["runs"][-1]["semantic_comparison"]["status"], "MISMATCH")

    def test_existing_output_directory_is_never_reused(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            reference = root / "reference.exe"
            candidate = root / "candidate.exe"
            reference.write_bytes(b"reference")
            candidate.write_bytes(b"candidate")
            output = root / "campaign"
            output.mkdir()
            sentinel = output / "preserve.txt"
            sentinel.write_text("keep", encoding="utf-8")
            with self.assertRaises(FileExistsError):
                paired.run_campaign(
                    reference_bin=reference,
                    candidate_bin=candidate,
                    reference_commit="a" * 40,
                    candidate_commit="b" * 40,
                    output_dir=output,
                    pairs=5,
                    cpu_index=0,
                    cases=["scalar-sharpe"],
                )
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")

    def test_process_wall_and_cpu_metrics_reject_boolean_values(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            binary = root / "reference.exe"
            binary.write_bytes(b"reference")
            output = root / "out"
            (output / "cases" / "scalar-sharpe" / "runs").mkdir(parents=True)
            valid_fields = {
                "command": (str(binary),),
                "returncode": 0,
                "wall_seconds": 1.0,
                "cpu_seconds": 0.5,
                "peak_working_set_bytes": 1234,
                "affinity_mask": 1,
                "cpu_index": 0,
                "affinity_method": "inherited_parent_affinity_before_create_process",
                "affinity_verified": True,
                "thread_environment": dict(measurement.THREAD_ENVIRONMENT),
            }
            for field in ("wall_seconds", "cpu_seconds"):
                with self.subTest(field=field):
                    fields = {**valid_fields, field: True}

                    def measured_command(command, **_kwargs):
                        return measurement.CommandMeasurement(
                            **{**fields, "command": tuple(str(part) for part in command)}
                        )

                    with mock.patch.object(
                        paired.measurement,
                        "run_command",
                        side_effect=measured_command,
                    ):
                        record, payload = paired._run_one(
                            binary=binary,
                            binary_digest=paired.sha256_file(binary),
                            case_name="scalar-sharpe",
                            side="reference",
                            phase="measured",
                            pair_index=1 if field == "wall_seconds" else 2,
                            order_index=1,
                            output_dir=output,
                            cpu_index=0,
                            timeout=10.0,
                        )
                    self.assertIsNone(payload)
                    self.assertIn("process measurement", record["error"])


if __name__ == "__main__":
    unittest.main()
