"""Tests for the alpha003 benchmark runner: CLI wiring, result files and digests.

The runner is exercised with temporary directories and patched subprocess calls, so
no benchmark binary or network data is needed to run this module.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from benchmarks.alpha003_a_stocks import run_benchmark as runner


class CampaignTests(unittest.TestCase):
    def arguments(self, root: Path, *, repeats: int = 3,
                  skip_run: bool = False) -> argparse.Namespace:
        return argparse.Namespace(
            data_dir=root / "data", results_dir=root / "results",
            cpp_exe=root / "native.exe", repeats=repeats, max_symbols=500,
            cpu=0, end_offset=22, preprocess="cross_section_rank",
            fincore_root=root / "fincore", tolerance=1e-6,
            skip_run=skip_run,
        )

    @staticmethod
    def comparison(*, passed: bool = True) -> dict:
        return {
            "all_pass": passed,
            "all_primary_pass": passed,
            "all_secondary_pass": True,
            "checks": [
                {"name": name, "status": "PASS" if passed else "FAIL",
                 "max_abs_diff": 0.0, "failures": [] if passed else ["mismatch"]}
                for name in runner.html_report.TABLE_NAMES
            ],
            "coverage": {},
        }

    def _patch_campaign(self, args: argparse.Namespace, fake_run,
                        fake_compare) -> list[mock._patch]:
        return [
            mock.patch.object(runner, "parse_args", return_value=args),
            mock.patch.object(runner, "find_cpp", return_value=args.cpp_exe),
            mock.patch.object(runner, "input_identity", return_value={"fixed": True}),
            mock.patch.object(runner, "host_identity", return_value={"platform": "test"}),
            mock.patch.object(runner, "run_command", side_effect=fake_run),
            mock.patch.object(runner, "compare_results", side_effect=fake_compare),
            mock.patch.object(runner, "collect_timings", return_value={
                "load_seconds": 0.1, "research_seconds": 0.2,
            }),
            mock.patch("builtins.print"),
        ]

    def _success_run(self, events: list[str]):
        def fake_run(command, stdout_path, stderr_path, cpu_index=None):
            out = Path(command[command.index("--out") + 1])
            out.mkdir(parents=True)
            engine = "cpp" if "--report-template" in command else "python"
            (out / "research.html").write_text(
                f"{engine}:{command[command.index('--run-id') + 1]}", encoding="utf-8")
            events.append(f"run:{engine}")
            self.assertEqual(cpu_index, 0)
            return {
                "command": command, "returncode": 0,
                "wall_seconds": 1.0 if engine == "python" else 0.5,
                "cpu_seconds": 0.4, "peak_working_set_bytes": 1024,
                "cpu_index": cpu_index, "affinity_verified": True,
                "stdout_excerpt": {"text": "{}", "truncated": False},
                "stderr_excerpt": {"text": "", "truncated": False},
            }
        return fake_run

    def test_success_keeps_only_three_html_outputs_and_compares_abba_pairs(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            args = self.arguments(root)
            events: list[str] = []

            def fake_compare(*_args, **_kwargs):
                events.append("compare")
                return self.comparison()

            patches = self._patch_campaign(args, self._success_run(events), fake_compare)
            with patches[0], patches[1], patches[2], patches[3], patches[4], patches[5], patches[6], patches[7]:
                self.assertEqual(runner.main(), 0)

            report = runner.read_comparison(args.results_dir / "comparison.html")
            self.assertTrue(report["campaign_complete"], report["failure"])
            pair_events = [event for event in events if event.startswith("run:")]
            expected_orders = [
                ("python", "cpp"), ("cpp", "python"),
                ("python", "cpp"), ("cpp", "python"), ("python", "cpp"),
            ]
            expected_events = [event for order in expected_orders for event in
                               (f"run:{order[0]}", f"run:{order[1]}", "compare")]
            self.assertEqual(pair_events, [
                "run:python", "run:cpp", "run:cpp", "run:python",
                "run:python", "run:cpp", "run:cpp", "run:python",
                "run:python", "run:cpp",
            ])
            self.assertEqual(events, expected_events)
            self.assertEqual({path.name for path in args.results_dir.iterdir()},
                             {"comparison.html", "python", "cpp"})
            for engine in ("python", "cpp"):
                self.assertEqual([path.name for path in (args.results_dir / engine).iterdir()],
                                 ["research.html"])
            self.assertEqual(list(args.results_dir.glob(".runs-*")), [])
            self.assertTrue(report["all_pairs_pass"])
            self.assertEqual(len(report["measurements"]["warmups"]), 2)
            self.assertEqual(len(report["measurements"]["pairs"]), 3)
            self.assertEqual(report["last_retained_pair"], {"kind": "pair", "index": 3})
            self.assertEqual(set(report["retained_reports"]), {"python", "cpp"})

    def test_process_failure_stops_and_keeps_bounded_logs_in_html(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            args = self.arguments(root)
            launches: list[str] = []

            def failed_run(command, stdout_path, stderr_path, cpu_index=None):
                out = Path(command[command.index("--out") + 1])
                out.mkdir(parents=True)
                launches.append("python" if "--report-template" not in command else "cpp")
                return {
                    "command": command, "returncode": 2, "wall_seconds": 0.2,
                    "stdout_excerpt": {"text": "partial stdout", "truncated": False},
                    "stderr_excerpt": {
                        "text": "finite </script><script>alert(1)</script>",
                        "truncated": False,
                    },
                }

            compare = mock.Mock(return_value=self.comparison())
            patches = self._patch_campaign(args, failed_run, compare)
            with patches[0], patches[1], patches[2], patches[3], patches[4], patches[5], patches[6], patches[7]:
                self.assertEqual(runner.main(), 1)

            self.assertEqual(launches, ["python"])
            compare.assert_not_called()
            self.assertEqual({path.name for path in args.results_dir.iterdir()}, {"comparison.html"})
            self.assertEqual(list(args.results_dir.glob(".runs-*")), [])
            report = runner.read_comparison(args.results_dir / "comparison.html")
            self.assertEqual(report["failure"]["phase"], "warmup_process")
            process = report["measurements"]["warmups"][0]["python"]
            self.assertEqual(process["stdout_excerpt"]["text"], "partial stdout")
            self.assertEqual(process["stderr_excerpt"]["text"],
                             "finite </script><script>alert(1)</script>")
            source = (args.results_dir / "comparison.html").read_text(encoding="utf-8")
            script = source.split('<script id="fincore-benchmark-data" type="application/json">', 1)[1]
            script = script.split("</script>", 1)[0]
            self.assertNotIn("</script>", script.lower())

    def test_comparison_failure_stops_before_next_pair_and_retains_failed_pair(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            args = self.arguments(root)
            events: list[str] = []
            failed = self.comparison(passed=False)

            def fake_compare(*_args, **_kwargs):
                events.append("compare")
                return failed

            patches = self._patch_campaign(args, self._success_run(events), fake_compare)
            with patches[0], patches[1], patches[2], patches[3], patches[4], patches[5], patches[6], patches[7]:
                self.assertEqual(runner.main(), 1)

            self.assertEqual(events, ["run:python", "run:cpp", "compare"])
            report = runner.read_comparison(args.results_dir / "comparison.html")
            self.assertEqual(report["failure"]["phase"], "warmup_comparison", report["failure"])
            self.assertEqual(report["last_retained_pair"], {"kind": "warmup", "index": 1},
                             report["measurements"]["warmups"][0])
            self.assertTrue((args.results_dir / "python" / "research.html").is_file())
            self.assertTrue((args.results_dir / "cpp" / "research.html").is_file())
            self.assertEqual(list(args.results_dir.glob(".runs-*")), [])

    def test_extra_child_output_stops_and_is_recorded(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            args = self.arguments(root)
            launches: list[str] = []

            def run_with_sidecar(command, stdout_path, stderr_path, cpu_index=None):
                out = Path(command[command.index("--out") + 1])
                out.mkdir(parents=True)
                engine = "cpp" if "--report-template" in command else "python"
                launches.append(engine)
                (out / "research.html").write_text(engine, encoding="utf-8")
                (out / "legacy.csv").write_text("unexpected", encoding="utf-8")
                return {
                    "command": command, "returncode": 0,
                    "wall_seconds": 0.2, "cpu_seconds": 0.1,
                    "stdout_excerpt": {"text": "", "truncated": False},
                    "stderr_excerpt": {"text": "", "truncated": False},
                }

            compare = mock.Mock(return_value=self.comparison())
            patches = self._patch_campaign(args, run_with_sidecar, compare)
            with patches[0], patches[1], patches[2], patches[3], patches[4], patches[5], patches[6], patches[7]:
                self.assertEqual(runner.main(), 1)

            self.assertEqual(launches, ["python"])
            compare.assert_not_called()
            self.assertEqual({path.name for path in args.results_dir.iterdir()},
                             {"comparison.html"})
            self.assertEqual(list(args.results_dir.glob(".runs-*")), [])
            report = runner.read_comparison(args.results_dir / "comparison.html")
            self.assertEqual(report["failure"]["phase"], "warmup_output_contract")
            output_issue = report["failure"]["output"]
            self.assertEqual(output_issue["reason"],
                             "each child output directory must contain only research.html")
            self.assertEqual(output_issue["entries"], ["legacy.csv", "research.html"])

    def test_existing_results_are_never_deleted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            args = self.arguments(Path(temporary))
            args.results_dir.mkdir()
            sentinel = args.results_dir / "keep.txt"
            sentinel.write_text("historical evidence", encoding="utf-8")
            with mock.patch.object(runner, "parse_args", return_value=args), \
                    mock.patch.object(runner, "find_cpp") as find_cpp:
                with self.assertRaises(FileExistsError):
                    runner.main()
            find_cpp.assert_not_called()
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "historical evidence")

    def test_invalid_tolerance_fails_before_creating_results(self) -> None:
        for tolerance in (float("nan"), float("inf"), 0.0, -1e-9, 1.1e-6):
            with self.subTest(tolerance=tolerance), tempfile.TemporaryDirectory() as temporary:
                args = self.arguments(Path(temporary))
                args.tolerance = tolerance
                with mock.patch.object(runner, "parse_args", return_value=args), \
                        mock.patch.object(runner, "find_cpp") as find_cpp:
                    with self.assertRaisesRegex(ValueError, "--tolerance"):
                        runner.main()
                find_cpp.assert_not_called()
                self.assertFalse(args.results_dir.exists())

    def test_skip_run_rechecks_only_hashed_pair_and_rejects_tampering(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            args = self.arguments(root)
            events: list[str] = []

            def fake_compare(*_args, **_kwargs):
                return self.comparison()

            patches = self._patch_campaign(args, self._success_run(events), fake_compare)
            with patches[0], patches[1], patches[2], patches[3], patches[4], patches[5], patches[6], patches[7]:
                self.assertEqual(runner.main(), 0)

            paths = [args.results_dir / "comparison.html",
                     args.results_dir / "python" / "research.html",
                     args.results_dir / "cpp" / "research.html"]
            before = {path: hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
            args.skip_run = True
            saved_comparison = self.comparison()
            with mock.patch.object(runner, "parse_args", return_value=args), \
                    mock.patch.object(runner.html_report, "compare_reports",
                                      return_value=saved_comparison) as compare_pair, \
                    mock.patch("builtins.print") as output:
                self.assertEqual(runner.main(), 0)
            compare_pair.assert_called_once()
            summary = json.loads(output.call_args.args[0])
            self.assertEqual(summary["historical_pairs_rechecked"], 0)
            self.assertTrue(summary["retained_pair_rechecked"])
            self.assertEqual(before, {path: hashlib.sha256(path.read_bytes()).hexdigest()
                                      for path in paths})

            python_report = args.results_dir / "python" / "research.html"
            python_report.write_text("tampered", encoding="utf-8")
            tampered_hash = hashlib.sha256(python_report.read_bytes()).hexdigest()
            with mock.patch.object(runner, "parse_args", return_value=args), \
                    mock.patch.object(runner.html_report, "compare_reports") as compare_pair, \
                    mock.patch("builtins.print"):
                self.assertEqual(runner.main(), 1)
            compare_pair.assert_not_called()
            self.assertEqual(hashlib.sha256(python_report.read_bytes()).hexdigest(), tampered_hash)
            self.assertEqual({path.name for path in args.results_dir.iterdir()},
                             {"comparison.html", "python", "cpp"})


if __name__ == "__main__":
    unittest.main()
