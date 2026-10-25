from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from benchmarks.alpha003_a_stocks.contract import TABLES
from benchmarks.alpha003_a_stocks.html_report import (
    REPORT_FILENAME,
    SCHEMA_VERSION,
    compare_reports,
    read_report,
    write_report,
)


def payload(engine: str = "Python") -> dict:
    rows = {
        "ic": [["2025-01-02T00:00:00Z", "alpha003", "1D", 0.1, 0.08, 20]],
        "ic_summary": [["alpha003", "1D", 0.1, 0.02, 5.0, 0.08, 0.03, 2.666, 1]],
        "quantile_returns": [["2025-01-02T00:00:00Z", "alpha003", "1D", 1, -0.01, 4]],
        "turnover": [["2025-01-02T00:00:00Z", "alpha003", "1D", 1, 0.25]],
        "spreads": [["2025-01-02T00:00:00Z", "alpha003", "1D", 0.03, 5, 1]],
        "stability": [["alpha003", 1, 0.2, 0.1, 20]],
        "coverage": [["alpha003", "factor", 1, 100, 90, 80, 70, "OK"]],
        "diagnostics": [],
    }
    return {
        "schema_version": SCHEMA_VERSION,
        "engine": engine,
        "workload": {"factor": "alpha003", "periods": ["1D"], "assets": 4,
                     "grid_dates": 1},
        "run_info": {"host": engine},
        "timings": {"total_seconds": 0.1},
        "tables": {
            name: {"columns": list(TABLES[name].columns), "rows": rows[name]}
            for name in rows
        },
    }


class Alpha003HtmlReportTests(unittest.TestCase):
    def make_report(self, directory: Path, value: dict) -> Path:
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / REPORT_FILENAME
        write_report(path, value)
        return path

    def test_single_file_roundtrip_escapes_script_breakout_and_allows_empty_diagnostics(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / REPORT_FILENAME
            original = payload()
            original["workload"]["note"] = "</script><img src=x> & \u2028\u2029"
            write_report(path, original)
            html = path.read_text(encoding="utf-8")
            self.assertIn(r"\u003c/script\u003e", html)
            self.assertIn(r"\u0026", html)
            self.assertIn(r"\u2028", html)
            self.assertNotIn("</script><img", html)
            self.assertEqual(read_report(path), original)
            self.assertEqual(read_report(path)["tables"]["diagnostics"]["rows"], [])
            with self.assertRaises(FileExistsError):
                write_report(path, payload())

    def test_write_and_close_failures_remove_only_the_new_partial_report(self) -> None:
        for phase in ("write", "close"):
            with self.subTest(phase=phase), tempfile.TemporaryDirectory() as temporary:
                target = Path(temporary) / REPORT_FILENAME
                real_open = Path.open

                class FailingStream:
                    def __init__(self, stream):
                        self.stream = stream

                    def __enter__(self):
                        return self

                    def write(self, document):
                        if phase == "write":
                            self.stream.write(document[:32])
                            raise OSError("simulated write failure")
                        return self.stream.write(document)

                    def __exit__(self, exc_type, exc, traceback):
                        self.stream.close()
                        if phase == "close":
                            raise OSError("simulated close failure")
                        return False

                def patched_open(path, *args, **kwargs):
                    stream = real_open(path, *args, **kwargs)
                    if path == target:
                        return FailingStream(stream)
                    return stream

                with mock.patch.object(Path, "open", new=patched_open):
                    expected = f"simulated {phase} failure"
                    with self.assertRaisesRegex(OSError, expected):
                        write_report(target, payload())
                self.assertFalse(target.exists())

    def test_exclusive_create_failure_preserves_existing_report(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / REPORT_FILENAME
            path.write_text("keep this existing report", encoding="utf-8")
            with self.assertRaises(FileExistsError):
                write_report(path, payload())
            self.assertEqual(path.read_text(encoding="utf-8"), "keep this existing report")

    def test_engine_and_runtime_timings_are_ignored_for_parity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            py = payload("Python")
            cpp = payload("C++")
            cpp["run_info"] = {"host": "another machine"}
            cpp["timings"] = {"total_seconds": 999.0}
            self.make_report(root / "py", py)
            self.make_report(root / "cpp", cpp)
            result = compare_reports(root / "py", root / "cpp")
            self.assertTrue(result["all_pass"])
            self.assertTrue(result["all_primary_pass"])
            self.assertTrue(result["all_secondary_pass"])
            self.assertEqual(len(result["checks"]), 10)
            self.assertEqual(result["coverage"]["python_rows"]["diagnostics"], 0)

    def test_diagnostic_factor_may_be_null_but_other_string_cells_may_not(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            value = payload()
            value["tables"]["diagnostics"]["rows"] = [[None, "close", "info", "X", "notice"]]
            path = Path(temporary) / REPORT_FILENAME
            write_report(path, value)
            self.assertEqual(read_report(path)["tables"]["diagnostics"]["rows"][0][0], None)

            invalid = payload()
            invalid["tables"]["diagnostics"]["rows"] = [[None, None, "info", "X", "notice"]]
            with self.assertRaisesRegex(ValueError, r"diagnostics.rows\[0\].stage"):
                write_report(Path(temporary) / "invalid.html", invalid)

    def test_bad_schema_is_reported_as_a_failed_comparison(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            py_path = self.make_report(root / "py", payload())
            cpp_path = root / "cpp" / REPORT_FILENAME
            cpp_path.parent.mkdir()
            template = Path(__file__).parents[2] / "benchmarks" / "alpha003_a_stocks" / "report_template.html"
            invalid = payload()
            invalid["schema_version"] = "wrong"
            cpp_path.write_text(template.read_text(encoding="utf-8").replace(
                "__FINCORE_REPORT_DATA__", json.dumps(invalid, separators=(",", ":"))),
                encoding="utf-8")
            with self.assertRaises(ValueError):
                read_report(cpp_path)
            result = compare_reports(py_path.parent, cpp_path.parent)
            self.assertFalse(result["all_pass"])
            self.assertEqual(result["checks"][0]["name"], "reports")

    def test_missing_rows_and_workload_changes_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            py = payload()
            cpp = payload("C++")
            cpp["tables"]["stability"]["rows"] = []
            cpp["workload"]["assets"] = 5
            self.make_report(root / "py", py)
            self.make_report(root / "cpp", cpp)
            result = compare_reports(root / "py", root / "cpp")
            by_name = {check["name"]: check for check in result["checks"]}
            self.assertFalse(result["all_pass"])
            self.assertEqual(by_name["workload"]["status"], "FAIL")
            self.assertEqual(by_name["stability"]["rows_python"], 1)
            self.assertEqual(by_name["stability"]["rows_cpp"], 0)

    def test_nonfinite_writer_value_becomes_null_and_null_mask_is_strict(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            py = payload()
            py["tables"]["ic"]["rows"][0][3] = float("nan")
            cpp = payload("C++")
            self.make_report(root / "py", py)
            self.make_report(root / "cpp", cpp)
            self.assertIsNone(read_report(root / "py" / REPORT_FILENAME)["tables"]["ic"]["rows"][0][3])
            result = compare_reports(root / "py", root / "cpp")
            check = next(item for item in result["checks"] if item["name"] == "ic")
            self.assertEqual(check["status"], "FAIL")
            self.assertTrue(any("null mask" in failure for failure in check["failures"]))

    def test_positive_and_negative_infinity_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            for index, infinite in enumerate((float("inf"), float("-inf"))):
                value = payload()
                value["tables"]["ic"]["rows"][0][3] = infinite
                path = Path(temporary) / f"infinite-{index}.html"
                with self.subTest(infinite=infinite):
                    with self.assertRaisesRegex(ValueError, "infinite value"):
                        write_report(path, value)
                    self.assertFalse(path.exists())

    def test_float_difference_above_tolerance_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            py = payload()
            cpp = payload("C++")
            cpp["tables"]["spreads"]["rows"][0][3] += 2e-6
            self.make_report(root / "py", py)
            self.make_report(root / "cpp", cpp)
            result = compare_reports(root / "py", root / "cpp", tolerance=1e-6)
            check = next(item for item in result["checks"] if item["name"] == "spreads")
            self.assertEqual(check["status"], "FAIL")
            self.assertAlmostEqual(check["max_abs_diff"], 2e-6)


if __name__ == "__main__":
    unittest.main()
