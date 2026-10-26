"""Single-file, offline Alpha003 research reports and parity comparison."""
from __future__ import annotations

from html.parser import HTMLParser
import json
import math
from pathlib import Path
import re
from typing import Any

from .contract import TABLES


SCHEMA_VERSION = "alpha003-html-v1"
REPORT_FILENAME = "research.html"
TABLE_NAMES = (
    "ic", "ic_summary", "quantile_returns", "turnover", "spreads",
    "stability", "coverage", "diagnostics",
)
_TOP_LEVEL_KEYS = {"schema_version", "engine", "workload", "run_info", "timings", "tables"}
_TABLE_FLOATS = {name: set(TABLES[name].floats) for name in TABLE_NAMES}
_TABLE_INTEGERS = {name: set(TABLES[name].integers) for name in TABLE_NAMES}
_UTC_TIME = re.compile(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d{1,9})?Z\Z")
_PLACEHOLDER = "__FINCORE_REPORT_DATA__"


def _plain_json_value(value: Any, *, path: str = "payload") -> Any:
    """Return JSON-native data, mapping NaN to null and rejecting infinity."""
    if value is None or isinstance(value, (str, bool, int)):
        return value
    if isinstance(value, float):
        if math.isnan(value):
            return None
        if math.isinf(value):
            raise ValueError(f"{path} contains an infinite value")
        return value
    if isinstance(value, dict):
        if any(not isinstance(key, str) for key in value):
            raise TypeError(f"{path} object keys must be strings")
        return {key: _plain_json_value(item, path=f"{path}.{key}")
                for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_plain_json_value(item, path=f"{path}[{index}]")
                for index, item in enumerate(value)]
    # Support scalar values from NumPy and similar numeric libraries without
    # importing them into the report writer.
    item = getattr(value, "item", None)
    if callable(item):
        converted = item()
        if converted is not value:
            return _plain_json_value(converted, path=path)
    raise TypeError(f"{path} contains a non-JSON value of type {type(value).__name__}")


def _validate_json_tree(value: Any, *, path: str) -> None:
    if value is None or isinstance(value, (str, bool, int)):
        return
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValueError(f"{path} contains a non-finite number; use null")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            _validate_json_tree(item, path=f"{path}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            if not isinstance(key, str):
                raise ValueError(f"{path} object keys must be strings")
            _validate_json_tree(item, path=f"{path}.{key}")
        return
    raise ValueError(f"{path} contains a non-JSON value of type {type(value).__name__}")


def _validate_payload(payload: Any) -> dict[str, Any]:
    if not isinstance(payload, dict):
        raise ValueError("report payload must be an object")
    if set(payload) != _TOP_LEVEL_KEYS:
        missing = sorted(_TOP_LEVEL_KEYS - set(payload))
        extra = sorted(set(payload) - _TOP_LEVEL_KEYS)
        raise ValueError(f"report payload keys differ (missing={missing}, extra={extra})")
    if payload["schema_version"] != SCHEMA_VERSION:
        raise ValueError(f"unsupported schema_version: {payload['schema_version']!r}")
    if not isinstance(payload["engine"], str) or not payload["engine"]:
        raise ValueError("engine must be a non-empty string")
    for name in ("workload", "run_info", "timings"):
        if not isinstance(payload[name], dict):
            raise ValueError(f"{name} must be an object")
        _validate_json_tree(payload[name], path=name)

    tables = payload["tables"]
    if not isinstance(tables, dict) or set(tables) != set(TABLE_NAMES):
        raise ValueError(f"tables must contain exactly: {', '.join(TABLE_NAMES)}")
    for table_name in TABLE_NAMES:
        table = tables[table_name]
        if not isinstance(table, dict) or set(table) != {"columns", "rows"}:
            raise ValueError(f"tables.{table_name} must contain columns and rows")
        columns = table["columns"]
        rows = table["rows"]
        expected = list(TABLES[table_name].columns)
        if columns != expected:
            raise ValueError(f"tables.{table_name}.columns must equal {expected!r}")
        if not isinstance(rows, list):
            raise ValueError(f"tables.{table_name}.rows must be an array")
        for row_index, row in enumerate(rows):
            if not isinstance(row, list) or len(row) != len(expected):
                raise ValueError(f"tables.{table_name}.rows[{row_index}] has the wrong shape")
            for column, value in zip(expected, row):
                cell_path = f"tables.{table_name}.rows[{row_index}].{column}"
                if column in _TABLE_FLOATS[table_name]:
                    if value is not None and (type(value) not in (int, float)
                                              or not math.isfinite(value)):
                        raise ValueError(f"{cell_path} must be a finite number or null")
                elif column in _TABLE_INTEGERS[table_name]:
                    if type(value) is not int:
                        raise ValueError(f"{cell_path} must be an integer")
                elif (table_name == "diagnostics" and column == "factor"
                      and value is None):
                    continue
                elif not isinstance(value, str):
                    raise ValueError(f"{cell_path} must be a string")
                if column in ("time", "date") and not _UTC_TIME.fullmatch(value):
                    raise ValueError(f"{cell_path} must use canonical UTC ISO8601 Z text")
    return payload


def _json_for_script(payload: dict[str, Any]) -> str:
    serialized = json.dumps(payload, ensure_ascii=False, allow_nan=False,
                            separators=(",", ":"))
    # Script elements are parsed as raw text by HTML. Escaping these characters
    # prevents data such as </script> from ending the element before JSON.parse.
    return (serialized.replace("&", r"\u0026").replace("<", r"\u003c")
            .replace(">", r"\u003e").replace("\u2028", r"\u2028")
            .replace("\u2029", r"\u2029"))


def write_report(path: Path, payload: dict) -> None:
    """Write one offline HTML report, refusing to replace an existing file."""
    destination = Path(path)
    normalized = _plain_json_value(payload)
    _validate_payload(normalized)
    template = Path(__file__).with_name("report_template.html").read_text(encoding="utf-8")
    if template.count(_PLACEHOLDER) != 1:
        raise RuntimeError("report template must contain exactly one data placeholder")
    document = template.replace(_PLACEHOLDER, _json_for_script(normalized))
    stream = destination.open("x", encoding="utf-8", newline="\n")
    try:
        with stream:
            stream.write(document)
    except BaseException:
        # Exclusive creation succeeded, so only this invocation's report can
        # be removed. An open failure (including FileExistsError) never enters
        # this block and cannot delete a pre-existing report.
        try:
            destination.unlink()
        except OSError:
            pass
        raise


class _ReportDataParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=False)
        self.id_count = 0
        self.script_count = 0
        self.script_type: str | None = None
        self.script_closed = False
        self._capturing = False
        self._chunks: list[str] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attrs_map: dict[str, str | None] = {}
        for key, value in attrs:
            if key in attrs_map:
                raise ValueError(f"duplicate HTML attribute {key!r}")
            attrs_map[key] = value
        if attrs_map.get("id") != "fincore-report-data":
            return
        self.id_count += 1
        if tag.lower() != "script":
            return
        self.script_count += 1
        self.script_type = attrs_map.get("type")
        if self._capturing:
            raise ValueError("nested report data script")
        self._capturing = True

    def handle_data(self, data: str) -> None:
        if self._capturing:
            self._chunks.append(data)

    def handle_entityref(self, name: str) -> None:
        if self._capturing:
            self._chunks.append(f"&{name};")

    def handle_charref(self, name: str) -> None:
        if self._capturing:
            self._chunks.append(f"&#{name};")

    def handle_endtag(self, tag: str) -> None:
        if self._capturing and tag.lower() == "script":
            self._capturing = False
            self.script_closed = True


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"non-standard JSON constant {value!r} is forbidden")


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON object key {key!r}")
        result[key] = value
    return result


def read_report(path: Path) -> dict:
    """Read and validate the single embedded JSON report payload."""
    source = Path(path).read_text(encoding="utf-8")
    parser = _ReportDataParser()
    parser.feed(source)
    parser.close()
    if (parser.id_count != 1 or parser.script_count != 1
            or parser.script_type != "application/json" or not parser.script_closed
            or parser._capturing):
        raise ValueError("HTML must contain one closed application/json report data script")
    try:
        payload = json.loads("".join(parser._chunks), object_pairs_hook=_unique_object,
                             parse_constant=_reject_json_constant)
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise ValueError(f"invalid embedded report JSON: {error}") from error
    return _validate_payload(payload)


def _same_json_type(left: Any, right: Any) -> bool:
    if type(left) is not type(right):
        return False
    if isinstance(left, dict):
        return (left.keys() == right.keys()
                and all(_same_json_type(left[key], right[key]) for key in left))
    if isinstance(left, list):
        return (len(left) == len(right)
                and all(_same_json_type(a, b) for a, b in zip(left, right)))
    if isinstance(left, float):
        return left == right
    return left == right


def _check(name: str, *, status: str, rows_python: int, rows_cpp: int,
           max_abs_diff: float = 0.0, tolerance: float = 0.0,
           failures: list[str] | None = None) -> dict[str, Any]:
    return {
        "name": name,
        "kind": "primary",
        "status": status,
        "rows_python": rows_python,
        "rows_cpp": rows_cpp,
        # Keep the historical rows mapping consumed by benchmark summaries.
        "rows": {"python": rows_python, "cpp": rows_cpp,
                 "common": min(rows_python, rows_cpp)},
        "max_abs_diff": max_abs_diff,
        "tolerance": tolerance,
        "failures": (failures or [])[:20],
    }


def _failed_read_check(name: str, error: Exception) -> dict[str, Any]:
    return _check(name, status="FAIL", rows_python=0, rows_cpp=0,
                  failures=[f"{type(error).__name__}: {error}"])


def _compare_table(name: str, left: dict[str, Any], right: dict[str, Any],
                   tolerance: float) -> dict[str, Any]:
    left_rows, right_rows = left["rows"], right["rows"]
    failures: list[str] = []
    maximum = 0.0
    if left["columns"] != right["columns"]:
        failures.append("column schema differs")
    if len(left_rows) != len(right_rows):
        failures.append("row count differs")
    for row_index, (a_row, b_row) in enumerate(zip(left_rows, right_rows)):
        for column_index, column in enumerate(left["columns"]):
            a, b = a_row[column_index], b_row[column_index]
            if column in _TABLE_FLOATS[name]:
                if (a is None) != (b is None):
                    failures.append(f"row {row_index} {column}: null mask differs")
                    continue
                if a is None:
                    continue
                difference = abs(float(a) - float(b))
                maximum = max(maximum, difference)
                if difference > tolerance:
                    failures.append(
                        f"row {row_index} {column}: absolute error {difference:.9g}"
                    )
            elif type(a) is not type(b) or a != b:
                failures.append(f"row {row_index} {column}: exact value/order differs")
            if len(failures) >= 20:
                break
        if len(failures) >= 20:
            break
    return _check(name, status="FAIL" if failures else "PASS",
                  rows_python=len(left_rows), rows_cpp=len(right_rows),
                  max_abs_diff=maximum, tolerance=tolerance, failures=failures)


def compare_reports(py_dir: Path, cpp_dir: Path, tolerance: float = 1e-6) -> dict:
    """Compare two HTML reports using the Alpha003 table and workload contract."""
    if not isinstance(tolerance, (int, float)) or isinstance(tolerance, bool):
        raise ValueError("tolerance must be a finite number in (0, 1e-6]")
    if not math.isfinite(tolerance) or not 0 < tolerance <= 1e-6:
        raise ValueError("tolerance must be a finite number in (0, 1e-6]")
    try:
        py_payload = read_report(Path(py_dir) / REPORT_FILENAME)
    except (OSError, ValueError) as error:
        py_payload = None
        py_error = error
    else:
        py_error = None
    try:
        cpp_payload = read_report(Path(cpp_dir) / REPORT_FILENAME)
    except (OSError, ValueError) as error:
        cpp_payload = None
        cpp_error = error
    else:
        cpp_error = None

    checks: list[dict[str, Any]] = []
    if py_payload is None or cpp_payload is None:
        errors = []
        if py_error is not None:
            errors.append(f"python report: {type(py_error).__name__}: {py_error}")
        if cpp_error is not None:
            errors.append(f"cpp report: {type(cpp_error).__name__}: {cpp_error}")
        checks.append(_check("reports", status="FAIL", rows_python=0, rows_cpp=0,
                             failures=errors))
        return {
            "all_pass": False, "all_primary_pass": False, "all_secondary_pass": True,
            "checks": checks,
            "coverage": {"policy": "report unavailable; parity was not established",
                         "python_rows": {}, "cpp_rows": {}},
        }

    schema_ok = py_payload["schema_version"] == cpp_payload["schema_version"]
    checks.append(_check(
        "schema_version", status="PASS" if schema_ok else "FAIL", rows_python=1,
        rows_cpp=1, failures=[] if schema_ok else ["schema_version differs"],
    ))
    workload_ok = _same_json_type(py_payload["workload"], cpp_payload["workload"])
    checks.append(_check(
        "workload", status="PASS" if workload_ok else "FAIL", rows_python=1,
        rows_cpp=1, failures=[] if workload_ok else ["complete workload object differs"],
    ))
    for table_name in TABLE_NAMES:
        checks.append(_compare_table(table_name, py_payload["tables"][table_name],
                                     cpp_payload["tables"][table_name], tolerance))
    passed = all(check["status"] == "PASS" for check in checks)
    return {
        "all_pass": passed,
        "all_primary_pass": passed,
        "all_secondary_pass": True,
        "checks": checks,
        "coverage": {
            "policy": "workload, ordered rows, exact strings/integers and null masks; finite floats use absolute tolerance",
            "python_rows": {name: len(py_payload["tables"][name]["rows"])
                            for name in TABLE_NAMES},
            "cpp_rows": {name: len(cpp_payload["tables"][name]["rows"])
                         for name in TABLE_NAMES},
            "engine_identity_compared": False,
            "run_info_compared": False,
            "timings_compared": False,
        },
    }
