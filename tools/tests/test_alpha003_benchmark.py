from __future__ import annotations

import argparse
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from benchmarks.alpha003_a_stocks import run_benchmark as runner


class CampaignTests(unittest.TestCase):
    def arguments(self, root: Path) -> argparse.Namespace:
        return argparse.Namespace(
            data_dir=root / "data", results_dir=root / "new-results",
            cpp_exe=root / "native.exe", repeats=5, max_symbols=25,
            cpu=0, end_offset=22, preprocess="cross_section_rank",
            fincore_root=root / "fincore", tolerance=1e-6,
            stability_tolerance=1e-3, skip_run=False, keep_results=False,
        )

    def test_existing_results_are_never_deleted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            args = self.arguments(Path(temporary))
            args.results_dir.mkdir()
            sentinel = args.results_dir / "keep.txt"
            sentinel.write_text("historical evidence", encoding="utf-8")
            with mock.patch.object(runner, "parse_args", return_value=args), \
                    mock.patch.object(runner, "find_cpp", return_value=args.cpp_exe):
                with self.assertRaises(FileExistsError):
                    runner.main()
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "historical evidence")

    def test_each_pair_is_compared_before_next_launch_and_failure_is_retained(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            args = self.arguments(Path(temporary))
            events: list[str] = []
            comparison_count = 0

            def fake_run(command, log_path, cpu_index=None):
                out = Path(command[command.index("--out") + 1])
                out.mkdir(parents=True)
                info = {"assets": 25, "grid_dates": 243, "factor_cells": 500,
                        "periods": [1, 5, 20], "quantiles": 5,
                        "research_end": "2025-11-27"}
                (out / "run_info.json").write_text(json.dumps(info), encoding="utf-8")
                (out / "timings.json").write_text(json.dumps({
                    key: 0.1 for key in ("load_seconds", "factor_seconds",
                                        "research_seconds", "export_seconds", "total_seconds")
                }), encoding="utf-8")
                events.append("run")
                self.assertEqual(cpu_index, 0)
                return {"wall_seconds": 1.0, "cpu_seconds": 0.8,
                        "peak_working_set_bytes": 1024, "returncode": 0}

            def fake_compare(*_):
                nonlocal comparison_count
                comparison_count += 1
                events.append("compare")
                # Failure on the last pair must not be hidden by run1 success.
                passed = comparison_count != 7
                return {"all_pass": passed, "all_primary_pass": passed,
                        "all_secondary_pass": passed, "coverage": {},
                        "checks": [{"kind": "primary", "max_abs_diff": 0.0}]}

            with mock.patch.object(runner, "parse_args", return_value=args), \
                    mock.patch.object(runner, "find_cpp", return_value=args.cpp_exe), \
                    mock.patch.object(runner, "input_identity", return_value={"fixed": True}), \
                    mock.patch.object(runner, "host_identity", return_value={"platform": "test"}), \
                    mock.patch.object(runner, "run_command", side_effect=fake_run), \
                    mock.patch.object(runner, "compare_results", side_effect=fake_compare), \
                    mock.patch.object(runner, "format_markdown", return_value="report"), \
                    mock.patch("builtins.print"):
                self.assertEqual(runner.main(), 1)
            self.assertEqual(events, ["run", "run", "compare"] * 7)
            record = json.loads((args.results_dir / "measurements.json").read_text())
            self.assertEqual([row["order"] for row in record["pairs"]],
                             [["A", "B"], ["B", "A"], ["A", "B"], ["B", "A"], ["A", "B"]])
            self.assertEqual(len(record["warmups"]), 2)
            self.assertTrue(record["inputs_unchanged"])
            report = json.loads((args.results_dir / "comparison.json").read_text())
            self.assertFalse(report["all_pairs_pass"])
            self.assertEqual(len(report["all_comparisons"]), 5)
            self.assertEqual(report["formal_acceptance"], "DEVELOPMENT_CAMPAIGN_ONLY")

    def test_failed_warmup_stops_before_more_processes_and_retains_comparison(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            args = self.arguments(Path(temporary))
            failure = {"all_pass": False, "checks": [{"name": "labels", "status": "FAIL"}]}
            with mock.patch.object(runner, "parse_args", return_value=args), \
                    mock.patch.object(runner, "find_cpp", return_value=args.cpp_exe), \
                    mock.patch.object(runner, "input_identity", return_value={"fixed": True}), \
                    mock.patch.object(runner, "host_identity", return_value={}), \
                    mock.patch.object(runner, "run_command", return_value={"wall_seconds": 1.0}) as launch, \
                    mock.patch.object(runner, "compare_results", return_value=failure), \
                    mock.patch("builtins.print"):
                self.assertEqual(runner.main(), 1)
            self.assertEqual(launch.call_count, 2)
            record = json.loads((args.results_dir / "measurements.json").read_text(encoding="utf-8"))
            self.assertEqual(record["requested_pairs"], 5)
            self.assertEqual(record["pairs"], [])
            self.assertEqual(record["warmups"][0]["comparison"], failure)
            self.assertEqual(record["stopped_reason"], "warmup_output_mismatch")
            self.assertTrue(record["inputs_unchanged"])


if __name__ == "__main__":
    unittest.main()
