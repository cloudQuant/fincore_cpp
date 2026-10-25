#!/usr/bin/env python3
"""Compare alpha003 loader outputs on deterministic synthetic CSV inputs.

This standalone acceptance checker runs the supplied reference and candidate
executables through their normal command-line interface. It does not compile,
include, or otherwise load repository source files.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys
import time
from typing import Any


REFERENCE_SHA256 = "26e3f4184a668c0d7ddef1111f6e20d5a9a346a5c1d6162b9756984db6e8ade2"
RUN_ID = "p117-loader-r2"
CONTRACT_FILES = (
    "factor_values.csv",
    "workload.json",
    "tables/signals.csv",
    "tables/labels.csv",
    "tables/ic.csv",
    "tables/ic_summary.csv",
    "tables/quantile_returns.csv",
    "tables/turnover.csv",
    "tables/spreads.csv",
    "tables/stability.csv",
    "tables/coverage.csv",
    "tables/diagnostics.csv",
    "tables/manifest.json",
)
CSV_CONTRACTS = (
    "factor_values.csv",
    "tables/signals.csv",
    "tables/labels.csv",
    "tables/ic.csv",
)


class CheckFailure(RuntimeError):
    """A fail-closed acceptance check failed."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def pin_file(path: Path, label: str) -> dict[str, Any]:
    resolved = path.expanduser().resolve(strict=True)
    if not resolved.is_file():
        raise CheckFailure(f"{label} is not a regular file")
    return {"path": str(resolved), "sha256": sha256_file(resolved),
            "size_bytes": resolved.stat().st_size}


def business_sessions(count: int) -> list[dt.date]:
    day = dt.date(2025, 1, 2)
    sessions: list[dt.date] = []
    while len(sessions) < count:
        if day.weekday() < 5:
            sessions.append(day)
        day += dt.timedelta(days=1)
    return sessions


def local_close(day: dt.date, suffix: bool = False) -> str:
    text = f"{day:%Y-%m-%d} 15:00:00"
    return text + ".000" if suffix else text


def write_calendar(directory: Path, sessions: list[dt.date]) -> None:
    metadata = directory / "_metadata"
    metadata.mkdir(parents=True)
    with (metadata / "observed_calendar.csv").open(
            "w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("first_trade_time", "last_trade_time"))
        for day in sessions:
            writer.writerow((f"{day:%Y-%m-%d} 09:30:00",
                             local_close(day)))


def synthetic_rows(sessions: list[dt.date]) -> list[list[list[str]]]:
    rng = random.Random(0xA003117)
    per_asset: list[list[list[str]]] = [[] for _ in range(12)]
    before = sessions[0] - dt.timedelta(days=1)
    after = sessions[-1] + dt.timedelta(days=1)
    for asset in range(12):
        per_asset[asset].append([
            local_close(before, suffix=True), "50000", "50100", "49900",
            "50025", "400000", "20010000000",
        ])
    for day_index, day in enumerate(sessions):
        for asset in range(12):
            opening = 10_000 + rng.randrange(1, 900_000)
            volume = 10_000 + rng.randrange(1, 9_000_000)
            high = opening + rng.randrange(1, 2_000)
            low = max(1, opening - rng.randrange(1, 2_000))
            close = max(1, opening + rng.randrange(-900, 901))
            timestamp = local_close(
                day, suffix=(asset in (0, 6) or
                             (day_index + asset) % 11 == 0))
            per_asset[asset].append([
                timestamp, str(opening), str(high), str(low), str(close),
                str(volume), str(close * volume),
            ])
    for asset in range(12):
        per_asset[asset].append([
            local_close(after, suffix=True), "60000", "60100", "59900",
            "60025", "500000", "30012500000",
        ])
    return per_asset


def write_dataset(directory: Path, *, shuffled_asset: int | None = None,
                  error_case: str | None = None) -> None:
    directory.mkdir(parents=True)
    if error_case is None:
        sessions = business_sessions(60)
        write_calendar(directory, sessions)
        rows_by_asset = synthetic_rows(sessions)
        for asset, rows in enumerate(rows_by_asset):
            if asset == shuffled_asset:
                random.Random(0x117000 + asset).shuffle(rows)
            with (directory / f"asset_{asset:02d}.csv").open(
                    "w", encoding="utf-8", newline="") as stream:
                writer = csv.writer(stream, lineterminator="\n")
                writer.writerow(("trade_time", "open", "high", "low",
                                 "close", "volume", "amount"))
                writer.writerows(rows)
        return

    sessions = [dt.date(2025, 1, 2), dt.date(2025, 1, 3)]
    write_calendar(directory, sessions)
    rows: list[list[str]]
    if error_case == "short_timestamp":
        rows = [["2025-01", "100", "101", "99", "100", "1000", "100000"]]
    elif error_case == "invalid_calendar_date":
        rows = [["2025-02-30 15:00:00", "100", "101", "99", "100",
                 "1000", "100000"]]
    elif error_case == "duplicate_suffix":
        duplicate = local_close(sessions[0])
        rows = [
            [duplicate, "100", "101", "99", "100", "1000", "100000"],
            [duplicate + ".000", "110", "111", "109", "110", "1200",
             "132000"],
        ]
    else:
        raise CheckFailure(f"unknown internal error fixture: {error_case}")
    with (directory / "asset_error.csv").open(
            "w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(("trade_time", "open", "high", "low", "close",
                         "volume", "amount"))
        writer.writerows(rows)


def write_bytes(path: Path, value: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value)


def run_executable(executable: Path, data_dir: Path, invocation_dir: Path,
                   label: str) -> dict[str, Any]:
    invocation_dir.mkdir(parents=True)
    output_dir = invocation_dir / "output"
    command = [
        str(executable), "--data-dir", str(data_dir), "--out",
        str(output_dir), "--max-symbols", "0", "--end-offset", "22",
        "--preprocess", "cross_section_rank", "--run-id", RUN_ID,
    ]
    started = time.perf_counter()
    try:
        completed = subprocess.run(
            command, cwd=Path(__file__).resolve().parents[1],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            timeout=300,
        )
        stdout = completed.stdout
        stderr = completed.stderr
        returncode: int | None = completed.returncode
        timed_out = False
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout or b""
        stderr = error.stderr or b""
        returncode = None
        timed_out = True
    except OSError as error:
        stdout = b""
        stderr = str(error).encode("utf-8", errors="replace")
        returncode = None
        timed_out = False
    elapsed = time.perf_counter() - started
    write_bytes(invocation_dir / "stdout.bin", stdout)
    write_bytes(invocation_dir / "stderr.bin", stderr)
    result = {
        "label": label,
        "return_code": returncode,
        "timed_out": timed_out,
        "wall_seconds": round(elapsed, 6),
        "stdout_bytes": len(stdout),
        "stderr_bytes": len(stderr),
        "output_dir": str(output_dir),
    }
    (invocation_dir / "invocation.json").write_text(
        json.dumps({**result, "command": command}, indent=2, sort_keys=True) +
        "\n", encoding="utf-8")
    if timed_out:
        raise CheckFailure(f"{label} timed out after 300 seconds")
    if returncode is None:
        raise CheckFailure(f"{label} could not be started")
    return result


def require_contracts(output_dir: Path) -> dict[str, int]:
    missing = [relative for relative in CONTRACT_FILES
               if not (output_dir / relative).is_file()]
    if missing:
        raise CheckFailure(
            "missing required contract files: " + ", ".join(missing))
    sizes: dict[str, int] = {}
    for relative in CONTRACT_FILES:
        path = output_dir / relative
        size = path.stat().st_size
        if size == 0:
            raise CheckFailure(f"required contract file is empty: {relative}")
        sizes[relative] = size
    metadata: dict[str, Any] = {}
    for relative in ("timings.json", "run_info.json"):
        path = output_dir / relative
        if not path.is_file() or path.stat().st_size == 0:
            raise CheckFailure(f"required run metadata is missing or empty: {relative}")
        try:
            metadata[relative] = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            raise CheckFailure(f"invalid {relative}: {error}") from error
    if metadata["run_info.json"].get("run_id") != RUN_ID:
        raise CheckFailure("run_info.json does not preserve the shared run id")
    try:
        workload = json.loads(
            (output_dir / "workload.json").read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise CheckFailure(f"invalid workload.json: {error}") from error
    if (workload.get("factor") != "alpha003" or
            workload.get("contract") != "alpha003-complete-v2" or
            workload.get("grid_dates") != 60 or
            workload.get("assets") != 12 or
            workload.get("factor_cells", 0) <= 0):
        raise CheckFailure("workload.json does not describe nonempty alpha003 work")
    for relative in CSV_CONTRACTS:
        try:
            with (output_dir / relative).open(
                    "r", encoding="utf-8", newline="") as stream:
                reader = csv.reader(stream)
                header = next(reader, None)
                first_row = next(reader, None)
        except (OSError, UnicodeError, csv.Error) as error:
            raise CheckFailure(f"cannot read required CSV {relative}: {error}") from error
        if not header or not first_row:
            raise CheckFailure(
                f"required factor/signals/labels/IC workload has no data rows: {relative}")
    return sizes


def compare_contracts(left: Path, right: Path) -> dict[str, Any]:
    left_sizes = require_contracts(left)
    right_sizes = require_contracts(right)
    differences = [relative for relative in CONTRACT_FILES
                   if (left / relative).read_bytes() !=
                   (right / relative).read_bytes()]
    if differences:
        raise CheckFailure(
            "contract bytes differ: " + ", ".join(differences))
    return {"files_compared": len(CONTRACT_FILES),
            "contract_sizes_bytes": left_sizes,
            "candidate_contract_sizes_bytes": right_sizes}


def compare_error_runs(left: dict[str, Any], right: dict[str, Any],
                       left_dir: Path, right_dir: Path,
                       expected_message: str, label: str) -> dict[str, Any]:
    if left["return_code"] != 1 or right["return_code"] != 1:
        raise CheckFailure(f"{label} did not return the expected error exit code 1")
    left_stderr = (left_dir / "stderr.bin").read_bytes()
    right_stderr = (right_dir / "stderr.bin").read_bytes()
    if left_stderr != right_stderr:
        raise CheckFailure(f"{label} stderr differs between reference and candidate")
    expected = expected_message.encode("utf-8")
    if expected not in left_stderr or expected not in right_stderr:
        raise CheckFailure(f"{label} did not reach its intended parser error")
    return {"return_code": 1, "stderr_bytes": len(left_stderr),
            "stderr_sha256": hashlib.sha256(left_stderr).hexdigest(),
            "expected_message": expected_message}


def run_success_pair(root: Path, name: str, data_dir: Path,
                     reference: Path, candidate: Path) -> dict[str, Any]:
    reference_dir = root / "runs" / name / "reference"
    candidate_dir = root / "runs" / name / "candidate"
    reference_run = run_executable(reference, data_dir, reference_dir,
                                   f"{name}.reference")
    candidate_run = run_executable(candidate, data_dir, candidate_dir,
                                   f"{name}.candidate")
    if reference_run["return_code"] != 0 or candidate_run["return_code"] != 0:
        raise CheckFailure(f"{name} success fixture did not exit zero")
    comparison = compare_contracts(
        Path(reference_run["output_dir"]), Path(candidate_run["output_dir"]))
    return {"reference": reference_run, "candidate": candidate_run,
            "comparison": comparison}


def run_error_pair(root: Path, name: str, error_case: str,
                   expected_message: str, reference: Path,
                   candidate: Path) -> dict[str, Any]:
    data_dir = root / "fixtures" / name
    write_dataset(data_dir, error_case=error_case)
    reference_dir = root / "runs" / name / "reference"
    candidate_dir = root / "runs" / name / "candidate"
    reference_run = run_executable(reference, data_dir, reference_dir,
                                   f"{name}.reference")
    candidate_run = run_executable(candidate, data_dir, candidate_dir,
                                   f"{name}.candidate")
    comparison = compare_error_runs(reference_run, candidate_run,
                                    reference_dir, candidate_dir,
                                    expected_message, name)
    return {"reference": reference_run, "candidate": candidate_run,
            "comparison": comparison}


def safe_pin(path: Path) -> dict[str, Any] | None:
    try:
        return pin_file(path, "pinned file")
    except (OSError, RuntimeError):
        return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-exe", required=True, type=Path)
    parser.add_argument("--candidate-exe", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path,
                        help="new, nonexistent directory used for fixtures and logs")
    args = parser.parse_args()

    output_root = args.output_dir.expanduser().resolve()
    if output_root.exists():
        report = {"schema": "p117-alpha003-loader-check-v1",
                  "status": "FAIL", "failure": "output directory must be fresh",
                  "output_dir": str(output_root)}
        print(json.dumps(report, sort_keys=True))
        return 2
    output_root.mkdir(parents=True)

    reference_path = args.reference_exe.expanduser().resolve()
    candidate_path = args.candidate_exe.expanduser().resolve()
    script_path = Path(__file__).resolve()
    report: dict[str, Any] = {
        "schema": "p117-alpha003-loader-check-v1",
        "status": "RUNNING",
        "output_dir": str(output_root),
        "run_id": RUN_ID,
        "contract_files": list(CONTRACT_FILES),
        "excluded_variable_files": ["timings.json", "run_info.json"],
        "pins_pre": {},
        "pins_post": {},
        "success_cases": {},
        "error_cases": {},
    }
    try:
        reference_pre = pin_file(reference_path, "reference executable")
        candidate_pre = pin_file(candidate_path, "candidate executable")
        script_pre = pin_file(script_path, "acceptance script")
        report["pins_pre"] = {
            "reference_exe": reference_pre,
            "candidate_exe": candidate_pre,
            "script": script_pre,
        }
        if reference_pre["sha256"] != REFERENCE_SHA256:
            raise CheckFailure("reference executable SHA-256 does not match the pinned build")
        if reference_path == candidate_path:
            raise CheckFailure("reference and candidate executable paths must differ")

        cross_file_dir = output_root / "fixtures" / "cross_file_dates"
        shuffled_dir = output_root / "fixtures" / "shuffled_one_file"
        write_dataset(cross_file_dir)
        write_dataset(shuffled_dir, shuffled_asset=5)

        report["success_cases"]["cross_file_dates_suffix_outside_grid"] = (
            run_success_pair(output_root, "cross_file_dates_suffix_outside_grid",
                             cross_file_dir, reference_path, candidate_path))
        report["success_cases"]["one_file_shuffled"] = run_success_pair(
            output_root, "one_file_shuffled", shuffled_dir,
            reference_path, candidate_path)

        for name, input_case, expected in (
                ("short_timestamp", "short_timestamp", "cannot parse timestamp"),
                ("invalid_calendar_date", "invalid_calendar_date",
                 "UTC timestamp contains an invalid calendar date"),
                ("duplicate_timestamp_suffix", "duplicate_suffix",
                 "duplicate trade_time:")):
            report["error_cases"][name] = run_error_pair(
                output_root, name, input_case, expected,
                reference_path, candidate_path)

        # Row order must not affect contract output for this deterministic panel.
        for relative in CONTRACT_FILES:
            cross = output_root / "runs" / "cross_file_dates_suffix_outside_grid"
            shuffled = output_root / "runs" / "one_file_shuffled"
            for side in ("reference", "candidate"):
                left = cross / side / "output" / relative
                right = shuffled / side / "output" / relative
                if left.read_bytes() != right.read_bytes():
                    raise CheckFailure(
                        f"shuffling one input file changed {side} contract bytes: {relative}")

        pins_post = {
            "reference_exe": safe_pin(reference_path),
            "candidate_exe": safe_pin(candidate_path),
            "script": safe_pin(script_path),
        }
        report["pins_post"] = pins_post
        if any(value is None for value in pins_post.values()):
            raise CheckFailure("a pinned executable or script disappeared during the check")
        for name in ("reference_exe", "candidate_exe", "script"):
            if report["pins_pre"][name] != pins_post[name]:
                raise CheckFailure(f"{name} changed during the check")
        report["status"] = "PASS_OUTPUT_AND_ERROR_PARITY"
    except Exception as error:  # Preserve a finite report for every run failure.
        report["status"] = "FAIL"
        report["failure"] = {"type": type(error).__name__, "message": str(error)}
        report["pins_post"] = {
            "reference_exe": safe_pin(reference_path),
            "candidate_exe": safe_pin(candidate_path),
            "script": safe_pin(script_path),
        }
    report_path = output_root / "report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 0 if report["status"] == "PASS_OUTPUT_AND_ERROR_PARITY" else 1


if __name__ == "__main__":
    sys.exit(main())
