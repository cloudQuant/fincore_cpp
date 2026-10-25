from __future__ import annotations

import csv
import json
from pathlib import Path
import tempfile
import unittest

from benchmarks.alpha003_a_stocks.contract import (
    CONTRACT_VERSION, TABLES, compare_complete_results, compare_csv,
)


class CompleteContractTests(unittest.TestCase):
    def compare(self, name, rows, candidate=None, *, columns=None, chunk_size=2):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / filename for filename in ("python.csv", "cpp.csv")]
            for index, content in enumerate((rows, rows if candidate is None else candidate)):
                with paths[index].open("w", encoding="utf-8", newline="") as stream:
                    writer = csv.writer(stream, lineterminator="\n")
                    writer.writerow(columns if index == 1 and columns else TABLES[name].columns)
                    writer.writerows(content)
            return compare_csv(name, *paths, chunk_size=chunk_size)

    def test_full_label_keys_and_nan_masks(self):
        rows = [("2025-01-01T07:00:00Z", "a", "1D", ""),
                ("2025-01-01T07:00:00Z", "b", "1D", "0.5")]
        result = self.compare("labels", rows)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["missing_counts"]["value"], [1, 1])
        self.assertEqual(self.compare("labels", rows, rows[1:])["status"], "FAIL")
        changed = [(*rows[0][:-1], 0), rows[1]]
        self.assertEqual(self.compare("labels", rows, changed)["status"], "FAIL")

    def test_duplicate_at_chunk_boundary_and_unordered_rows_fail(self):
        rows = [("2025-01-01T07:00:00Z", "a", "1D", 1),
                ("2025-01-01T07:00:00Z", "b", "1D", 2)]
        self.assertEqual(self.compare("labels", rows + [rows[-1]])["status"], "FAIL")
        self.assertEqual(self.compare("labels", list(reversed(rows)))["status"], "FAIL")

    def test_period_order_is_spec_order_not_lexicographic(self):
        rows = [("2025-01-01T07:00:00Z", "a", period, 0.1)
                for period in ("1D", "5D", "20D")]
        self.assertEqual(self.compare("labels", rows)["status"], "PASS")
        self.assertEqual(self.compare("labels", [rows[0], rows[2], rows[1]])["status"], "FAIL")

    def test_pearson_and_asset_counts_are_obligatory(self):
        rows = [("2025-01-01T07:00:00Z", "alpha", "1D", 0.1, 0.2, 10)]
        self.assertEqual(self.compare("ic", rows)["status"], "PASS")
        self.assertEqual(self.compare("ic", rows, [(*rows[0][:3], 0.3, 0.2, 10)])["status"], "FAIL")
        self.assertEqual(self.compare("ic", rows, [(*rows[0][:-1], 11)])["status"], "FAIL")
        self.assertEqual(self.compare("ic", rows, [(*rows[0][:-1], "10.0")])["status"], "FAIL")
        self.assertEqual(self.compare("ic", rows, [rows[0][:-1]],
                                      columns=TABLES["ic"].columns[:-1])["status"], "FAIL")

    def test_exact_categorical_values_are_compared(self):
        rows = [("alpha", "factor", 1, 10, 10, 10, 10, "OK")]
        self.assertEqual(self.compare("coverage", rows)["status"], "PASS")
        self.assertEqual(self.compare("coverage", rows, [(*rows[0][:-1], "PARTIAL")])["status"], "FAIL")

    def test_diagnostic_duplicate_occurrences_and_escaped_text_survive(self):
        row = ("", "prepare", "warning", "missing", 'a,"b"\nsecond line')
        self.assertEqual(self.compare("diagnostics", [row, row])["status"], "PASS")
        self.assertEqual(self.compare("diagnostics", [row, row], [row])["status"], "FAIL")

    def test_numeric_tolerance_and_infinity_sign_masks(self):
        rows = [("2025-01-01T07:00:00Z", "a", "1D", "inf"),
                ("2025-01-01T07:00:00Z", "b", "1D", "0.5")]
        self.assertEqual(self.compare("labels", rows)["status"], "PASS")
        self.assertEqual(self.compare("labels", rows, [(*rows[0][:-1], "-inf"), rows[1]])["status"], "FAIL")
        self.assertEqual(self.compare("labels", rows, [rows[0], (*rows[1][:-1], "0.50000001")])["status"], "PASS")
        self.assertEqual(self.compare("labels", rows, [rows[0], (*rows[1][:-1], "0.500002")])["status"], "FAIL")

    def test_empty_tables_still_require_exact_schema(self):
        self.assertEqual(self.compare("diagnostics", [])["status"], "PASS")
        self.assertEqual(self.compare("diagnostics", [], columns=("index", "message"))["status"], "FAIL")

    def test_time_text_is_utc_and_preserves_subseconds(self):
        row = ("2025-01-01T07:00:00.123456789Z", "a", 0.5)
        self.assertEqual(self.compare("factor_values", [row])["status"], "PASS")
        altered = ("2025-01-01T07:00:00.123456000Z", "a", 0.5)
        self.assertEqual(self.compare("factor_values", [row], [altered])["status"], "FAIL")
        local = ("2025-01-01 15:00:00+08:00", "a", 0.5)
        self.assertEqual(self.compare("factor_values", [local])["status"], "FAIL")

    def test_empty_keys_and_invalid_integer_semantics_fail(self):
        self.assertEqual(self.compare("factor_values", [("2025-01-01T07:00:00Z", "", 0.5)])["status"], "FAIL")
        self.assertEqual(self.compare("coverage", [("alpha", "factor", 0, 1, 1, 1, 1, "OK")])["status"], "FAIL")
        self.assertEqual(self.compare("coverage", [("alpha", "factor", 1, -1, 1, 1, 1, "OK")])["status"], "FAIL")


class CompleteBundleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.py = Path(self.temporary.name) / "python"
        self.cpp = Path(self.temporary.name) / "cpp"
        self.py.mkdir()
        (self.cpp / "tables").mkdir(parents=True)
        tables = {}
        fixture_rows = {
            "factor_values": [("2025-01-01T07:00:00Z", "a", .1),
                              ("2025-01-02T07:00:00Z", "a", .2)],
            "signals": [("2025-01-01T07:00:00Z", "a", "alpha003", 1, .1, .1, "none", "default")],
            "labels": [("2025-01-01T07:00:00Z", "a", "1D", "")],
        }
        for name, schema in TABLES.items():
            for directory in (self.py, self.cpp if name == "factor_values" else self.cpp / "tables"):
                with (directory / f"{name}.csv").open("w", encoding="utf-8", newline="") as stream:
                    writer = csv.writer(stream, lineterminator="\n")
                    writer.writerow(schema.columns)
                    writer.writerows(fixture_rows.get(name, []))
            if name != "factor_values":
                tables[name] = {"rows": len(fixture_rows.get(name, [])), "columns": list(schema.columns)}
        spec = {"stability_lags": [1], "quantiles": 5, "min_assets": 2,
                "min_quantile_assets": 1, "preprocess": "none", "label": "next_open_to_open"}
        self.py_manifest = {"status": "UNEVALUABLE", "result_schema_version": 1,
                            "api_version": "1.0", "labels": ["1D"], "spec": spec, "tables": tables}
        self.cpp_manifest = {"status": "UNEVALUABLE", "schema_version": 2,
                             "api_version": "2.0", "schema_id": "fincore.factor_research_detailed.v2",
                             "tables": tables}
        self.workload = {**spec, "contract": CONTRACT_VERSION, "factor": "alpha003", "periods": ["1D"],
                         "direction": 1, "start": "2025-01-01T07:00:00Z",
                         "research_end": "2025-01-02T07:00:00Z", "grid_dates": 2, "assets": 1,
                         "factor_cells": 2, "label_config": {}, "serialization": {
                             "encoding": "utf-8", "line_ending": "LF", "timestamp": "ISO8601-UTC-Z",
                             "float": "roundtrip-binary64", "nan": "empty"}}
        self.write(self.py / "manifest.json", self.py_manifest)
        self.write(self.cpp / "tables" / "manifest.json", self.cpp_manifest)
        self.write(self.py / "workload.json", self.workload)
        self.write(self.cpp / "workload.json", self.workload)

    @staticmethod
    def write(path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    def compare(self):
        return compare_complete_results(self.py, self.cpp)

    def test_complete_bundle_requires_all_files_and_valid_manifests(self):
        self.assertTrue(self.compare()["all_pass"])
        (self.cpp / "tables" / "labels.csv").unlink()
        self.assertFalse(self.compare()["all_pass"])

    def test_empty_workload_or_misreported_cell_count_is_not_a_valid_benchmark(self):
        for count in (0, 3):
            changed = {**self.workload, "factor_cells": count}
            for directory in (self.py, self.cpp):
                self.write(directory / "workload.json", changed)
            self.assertFalse(self.compare()["all_pass"])

    def test_manifest_rows_are_actual_integer_counts(self):
        self.cpp_manifest["tables"]["labels"]["rows"] = False
        self.write(self.cpp / "tables" / "manifest.json", self.cpp_manifest)
        self.assertFalse(self.compare()["all_pass"])

    def test_workload_requires_config_and_matches_research_spec(self):
        changed = {**self.workload, "min_assets": 3}
        for directory in (self.py, self.cpp):
            self.write(directory / "workload.json", changed)
        self.assertFalse(self.compare()["all_pass"])
        for directory in (self.py, self.cpp):
            self.write(directory / "workload.json", {"contract": CONTRACT_VERSION})
        self.assertFalse(self.compare()["all_pass"])

    def test_missing_or_wrong_schema_manifests_are_a_recorded_failure(self):
        (self.py / "workload.json").unlink()
        self.assertFalse(self.compare()["all_pass"])
        self.write(self.py / "workload.json", self.workload)
        self.cpp_manifest["schema_version"] = 1
        self.write(self.cpp / "tables" / "manifest.json", self.cpp_manifest)
        self.assertFalse(self.compare()["all_pass"])

    def test_malformed_registered_axes_fail_without_crashing(self):
        self.py_manifest["labels"] = [{"1D": True}]
        self.py_manifest["spec"]["stability_lags"] = None
        self.write(self.py / "manifest.json", self.py_manifest)
        self.assertFalse(self.compare()["all_pass"])


if __name__ == "__main__":
    unittest.main()
