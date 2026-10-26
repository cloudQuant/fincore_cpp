"""Compare the native detailed research exporter with the pinned Python oracle.

This is a tiny semantic probe, not a performance benchmark. It feeds only the
public JSON fixture to the C++ probe, exports into a fresh temporary directory,
and checks every row of the ten factor-local CSV tables in exact order.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import Any


DEFAULT_FIXTURE = (
    Path(__file__).resolve().parents[1]
    / "tests"
    / "data"
    / "research_detailed_oracle.json"
)
FLOAT_ABS_TOLERANCE = 1e-6


class OracleMismatch(AssertionError):
    """A native row, schema, mask, or manifest differed from the oracle."""


def _json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, allow_nan=False, indent=2) + "\n").encode(
        "utf-8"
    )


def _runtime_input(fixture: dict[str, Any]) -> dict[str, Any]:
    source_spec = fixture["spec"]
    spec = {
        key: value
        for key, value in source_spec.items()
        if key != "direction_overrides_by_case"
    }
    basis = spec["price_basis"]
    source_rows = fixture["input"]["factor_rows"]
    cases = []
    for expected_case in fixture["cases"]:
        factor_name = expected_case["factor"]
        factor_rows = []
        for row in source_rows:
            if factor_name not in row["values"]:
                raise OracleMismatch(
                    f"fixture has no value for factor {factor_name!r}"
                )
            factor_rows.append(
                {
                    "time": row["time"],
                    "asset": row["asset"],
                    "value": row["values"][factor_name],
                }
            )
        cases.append(
            {
                "factor": factor_name,
                "direction_overrides": expected_case["direction_overrides"],
                "factor_rows": factor_rows,
                "price_rows": fixture["input"]["prices"][basis],
            }
        )
    return {"spec": spec, "cases": cases}


def _table_rows(
    fixture: dict[str, Any], case: dict[str, Any], table: str
) -> list[dict[str, Any]]:
    payload = case["tables"][table]
    if table == "labels" and payload.get("values_from") == "common_tables.labels":
        return fixture["common_tables"]["labels"]["rows"]
    return payload["rows"]


def _stats_payload(
    fixture: dict[str, Any], case: dict[str, Any], table: str
) -> dict[str, Any]:
    if table == "labels":
        return fixture["common_tables"]["labels"]
    return case["tables"][table]


def _category_counts(
    rows: list[dict[str, Any]], columns: list[str]
) -> dict[str, dict[str, int]]:
    result: dict[str, dict[str, int]] = {}
    for column in (
        "factor",
        "kind",
        "period",
        "quantile",
        "lag",
        "status",
        "stage",
        "severity",
        "code",
    ):
        if column in columns:
            values = Counter(
                "null"
                if row[column] is None or row[column] == ""
                else str(row[column])
                for row in rows
            )
            result[column] = dict(values)
    return result


def _null_counts(rows: list[dict[str, Any]], columns: list[str]) -> dict[str, int]:
    return {
        column: sum(row[column] is None for row in rows) for column in columns
    }


def _check_fixture_stats(
    fixture: dict[str, Any], case: dict[str, Any], table: str, rows: list[dict[str, Any]]
) -> None:
    stats = _stats_payload(fixture, case, table)
    columns = stats["columns"]
    if len(rows) != stats["row_count"]:
        raise OracleMismatch(
            f"fixture {case['factor']}/{table}: stored row_count does not match rows"
        )
    if _null_counts(rows, columns) != stats["null_counts"]:
        raise OracleMismatch(
            f"fixture {case['factor']}/{table}: stored null mask counts do not match rows"
        )
    if _category_counts(rows, columns) != stats["category_counts"]:
        raise OracleMismatch(
            f"fixture {case['factor']}/{table}: stored category counts do not match rows"
        )


def _compare_cell(
    actual_text: str,
    expected: Any,
    *,
    factor: str,
    table: str,
    row_number: int,
    column: str,
) -> None:
    context = f"{factor}/{table}[{row_number}].{column}"
    if expected is None:
        if actual_text != "":
            raise OracleMismatch(f"{context}: expected missing value, got {actual_text!r}")
        return
    if isinstance(expected, bool):
        if actual_text != ("True" if expected else "False"):
            raise OracleMismatch(f"{context}: expected {expected!r}, got {actual_text!r}")
        return
    if isinstance(expected, int):
        try:
            actual = int(actual_text)
        except ValueError as error:
            raise OracleMismatch(f"{context}: expected integer, got {actual_text!r}") from error
        if actual != expected:
            raise OracleMismatch(f"{context}: expected {expected}, got {actual}")
        return
    if isinstance(expected, float):
        try:
            actual = float(actual_text)
        except ValueError as error:
            raise OracleMismatch(f"{context}: expected float, got {actual_text!r}") from error
        if not math.isfinite(actual) or not math.isfinite(expected):
            if not (math.isnan(actual) and math.isnan(expected)):
                raise OracleMismatch(f"{context}: non-finite value mismatch")
        elif abs(actual - expected) > FLOAT_ABS_TOLERANCE:
            raise OracleMismatch(
                f"{context}: expected {expected:.17g}, got {actual:.17g}; "
                f"absolute tolerance is {FLOAT_ABS_TOLERANCE:g}"
            )
        return
    if actual_text != str(expected):
        raise OracleMismatch(f"{context}: expected {expected!r}, got {actual_text!r}")


def _compare_table(
    fixture: dict[str, Any],
    case: dict[str, Any],
    table: str,
    directory: Path,
) -> int:
    expected_rows = _table_rows(fixture, case, table)
    stats = _stats_payload(fixture, case, table)
    _check_fixture_stats(fixture, case, table, expected_rows)
    path = directory / f"{table}.csv"
    if not path.is_file():
        raise OracleMismatch(f"{case['factor']}: missing exported table {path.name}")
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        columns = reader.fieldnames
        if columns != stats["columns"]:
            raise OracleMismatch(
                f"{case['factor']}/{table}: expected columns {stats['columns']!r}, got {columns!r}"
            )
        actual_rows = list(reader)
    if len(actual_rows) != stats["row_count"]:
        raise OracleMismatch(
            f"{case['factor']}/{table}: expected {stats['row_count']} rows, got {len(actual_rows)}"
        )
    actual_null_counts = {
        column: sum(row[column] == "" for row in actual_rows) for column in columns
    }
    if actual_null_counts != stats["null_counts"]:
        raise OracleMismatch(
            f"{case['factor']}/{table}: null mask mismatch: "
            f"expected {stats['null_counts']}, got {actual_null_counts}"
        )
    if _category_counts(actual_rows, columns) != stats["category_counts"]:
        raise OracleMismatch(
            f"{case['factor']}/{table}: category counts differ from oracle"
        )
    for row_number, (actual, expected) in enumerate(
        zip(actual_rows, expected_rows, strict=True)
    ):
        for column in columns:
            _compare_cell(
                actual[column],
                expected[column],
                factor=case["factor"],
                table=table,
                row_number=row_number,
                column=column,
            )
    return len(actual_rows)


def _check_manifest(
    fixture: dict[str, Any], case: dict[str, Any], directory: Path
) -> dict[str, int]:
    manifest_path = directory / "manifest.json"
    if not manifest_path.is_file():
        raise OracleMismatch(f"{case['factor']}: missing manifest.json")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema_id") != "fincore.factor_research_detailed.v2":
        raise OracleMismatch(f"{case['factor']}: unexpected detailed schema_id")
    if manifest.get("schema_version") != 2 or manifest.get("api_version") != "2.0":
        raise OracleMismatch(f"{case['factor']}: unexpected detailed schema/API version")
    if manifest.get("status") != case["status"]:
        raise OracleMismatch(
            f"{case['factor']}: expected status {case['status']}, got {manifest.get('status')}"
        )
    if manifest.get("engine", {}).get("omitted_empty_tables") != [
        "correlation",
        "comparisons",
    ]:
        raise OracleMismatch(f"{case['factor']}: empty cross-factor table metadata differs")

    table_names = fixture["factor_local_tables"]
    descriptors = manifest.get("tables", {})
    if set(descriptors) != set(table_names):
        raise OracleMismatch(
            f"{case['factor']}: manifest tables differ: "
            f"expected {sorted(table_names)}, got {sorted(descriptors)}"
        )
    rows_by_table: dict[str, int] = {}
    for table in table_names:
        expected_rows = _table_rows(fixture, case, table)
        expected_columns = _stats_payload(fixture, case, table)["columns"]
        descriptor = descriptors[table]
        expected_file = f"{table}.csv"
        if descriptor.get("file") != expected_file:
            raise OracleMismatch(f"{case['factor']}/{table}: manifest file name differs")
        if descriptor.get("columns") != expected_columns:
            raise OracleMismatch(f"{case['factor']}/{table}: manifest columns differ")
        if descriptor.get("rows") != len(expected_rows):
            raise OracleMismatch(f"{case['factor']}/{table}: manifest row count differs")
        rows_by_table[table] = len(expected_rows)

    result_manifest = manifest.get("result_manifest", {})
    for table, count in rows_by_table.items():
        key = f"rows_{table}"
        if key not in result_manifest or int(result_manifest[key]) != count:
            raise OracleMismatch(f"{case['factor']}: result_manifest {key} differs")
    for table in ("correlation", "comparisons"):
        key = f"rows_{table}"
        if int(result_manifest.get(key, -1)) != 0:
            raise OracleMismatch(f"{case['factor']}: expected empty {table} table")

    expected_files = {"manifest.json", *(f"{table}.csv" for table in table_names)}
    actual_files = {path.name for path in directory.iterdir() if path.is_file()}
    if actual_files != expected_files:
        raise OracleMismatch(
            f"{case['factor']}: exported file set differs: "
            f"expected {sorted(expected_files)}, got {sorted(actual_files)}"
        )
    return rows_by_table


def check_oracle(probe: Path, fixture_path: Path) -> dict[str, Any]:
    fixture = json.loads(fixture_path.read_text(encoding="utf-8"))
    runtime_input = _runtime_input(fixture)
    cases_summary = []
    with tempfile.TemporaryDirectory(prefix="p112-research-detailed-oracle-") as name:
        temporary = Path(name)
        input_path = temporary / "input.json"
        output_root = temporary / "exports"
        output_root.mkdir()
        input_path.write_bytes(_json_bytes(runtime_input))
        completed = subprocess.run(
            [str(probe.resolve()), str(input_path), str(output_root)],
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        if completed.returncode != 0:
            raise OracleMismatch(
                f"native probe exited {completed.returncode}: {completed.stderr.strip()}"
            )
        for case in fixture["cases"]:
            directory = output_root / case["factor"]
            table_counts = _check_manifest(fixture, case, directory)
            for table in fixture["factor_local_tables"]:
                _compare_table(fixture, case, table, directory)
            cases_summary.append(
                {
                    "factor": case["factor"],
                    "status": case["status"],
                    "tables": table_counts,
                    "diagnostics": case["manifest"]["counts"]["diagnostics"],
                }
            )
    return {
        "result": "PASS",
        "evidence": "actual C++ evaluate_factor_research_detailed + export_factor_research_detailed output compared row-by-row",
        "oracle_id": fixture["oracle_id"],
        "python_reference_commit": fixture["reference"]["commit"],
        "float_absolute_tolerance": FLOAT_ABS_TOLERANCE,
        "cases": cases_summary,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path, help="built native probe executable")
    parser.add_argument("--fixture", type=Path, default=DEFAULT_FIXTURE)
    arguments = parser.parse_args()
    if not arguments.probe.is_file():
        parser.error(f"probe executable does not exist: {arguments.probe}")
    if not arguments.fixture.is_file():
        parser.error(f"oracle fixture does not exist: {arguments.fixture}")
    try:
        result = check_oracle(arguments.probe, arguments.fixture)
    except (OracleMismatch, OSError, ValueError, KeyError) as error:
        print(f"research detailed oracle mismatch: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
