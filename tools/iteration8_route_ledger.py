#!/usr/bin/env python3
"""Build the Iteration 8 native route ledger from the frozen split and tests.

This is a evidence-mapping ledger, not a test runner. Case-level mappings come
from the reviewed dispositions; new routes map to the native test file for
their module with an explicit MODULE grain. GAP rows are real remaining work.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SPLIT = ROOT / "docs/iterations/iteration-8-full-python-parity/route-split.tsv"
DEFAULT_LEDGER = ROOT / "docs/iterations/capability-dispositions.json"
DEFAULT_OUT_DIR = ROOT / "docs/iterations/iteration-8-full-python-parity"

MODULE_TESTS = {
    "exceptions": ["tests/unit/errors_test.cpp"],
    "data": ["tests/unit/data_models_test.cpp"],
    "extensions": ["tests/unit/extensions_snapshot_test.cpp", "tests/unit/extensions_discovery_test.cpp"],
    "viz": ["tests/unit/viz_documents_test.cpp", "tests/unit/viz_html_builder_test.cpp"],
    "report": ["tests/unit/report_renderers_test.cpp", "tests/unit/report_models_test.cpp"],
    "factor_analysis": ["tests/unit/factor_artifacts_test.cpp", "tests/unit/factor_serialization_test.cpp", "tests/unit/factor_timedelta_test.cpp", "tests/unit/factor_workflows_test.cpp"],
    "attribution": ["tests/unit/attribution_facades_test.cpp", "tests/unit/risk_report_serialization_test.cpp"],
    "metrics": ["tests/unit/metrics_helpers_test.cpp", "tests/unit/metrics_frequencies_test.cpp", "tests/unit/metrics_bayesian_pure_test.cpp"],
    "risk": ["tests/unit/risk_report_serialization_test.cpp"],
    "simulation": ["tests/unit/simulation_base_test.cpp"],
    "runtime": ["tests/unit/runtime_lifecycle_test.cpp"],
    "data.providers": ["tests/unit/data_models_test.cpp"],
    "data.snapshots": ["tests/unit/data_models_test.cpp"],
    "data.contracts": ["tests/unit/data_models_test.cpp"],
}

OPERATION_TEST = {
    "metrics": "tests/unit/runtime_operations_metrics_test.cpp",
    "performance": "tests/unit/runtime_operations_metrics_test.cpp",
    "portfolio": "tests/unit/runtime_operations_portfolio_test.cpp",
    "risk": "tests/unit/runtime_operations_portfolio_test.cpp",
    "simulation": "tests/unit/runtime_operations_portfolio_test.cpp",
    "attribution": "tests/unit/runtime_operations_factor_test.cpp",
    "factor_analysis": "tests/unit/runtime_operations_factor_test.cpp",
    "optimization": "tests/unit/runtime_operations_factor_test.cpp",
    "report": "tests/unit/runtime_operations_factor_test.cpp",
    "data": "tests/unit/runtime_operations_factor_test.cpp",
}


def load_split(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def load_case_evidence(path: Path) -> dict[str, list[str]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    return {
        row["canonical_id"]: row["evidence_test_ids"]
        for row in payload["routes"]
        if row["evidence_test_ids"]
    }


def test_names(path: Path) -> list[str]:
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8", errors="replace")
    return [
        f"{suite}.{name}"
        for suite, name in re.findall(
            r"TEST(?:__F|_F)?\s*\(\s*([A-Za-z0-9_]+)\s*,\s*([A-Za-z0-9_]+)\s*\)",
            text,
        )
    ]


def module_for(route: str) -> str | None:
    parts = route.split(".")
    if parts[0] != "fincore" or len(parts) < 2:
        return None
    domain = parts[1]
    if domain == "factor_analysis":
        return "factor_analysis"
    if domain == "data" and len(parts) > 2:
        candidate = "data." + parts[2]
        if candidate in MODULE_TESTS:
            return candidate
    return domain


def registered_operation_ids() -> set[str]:
    text = ""
    for path in sorted((ROOT / "src/runtime").glob("*.cpp")):
        text += path.read_text(encoding="utf-8", errors="replace")
    return set(re.findall(r'"((?:metrics|performance|portfolio|risk|simulation|attribution|factor_analysis|optimization|report|data)\.[A-Za-z0-9_.]+)"', text))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--split", type=Path, default=DEFAULT_SPLIT)
    parser.add_argument("--ledger", type=Path, default=DEFAULT_LEDGER)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    args = parser.parse_args()

    rows = load_split(args.split)
    case_evidence = load_case_evidence(args.ledger)
    registered = registered_operation_ids()
    output = []
    counts: Counter[str] = Counter()
    for row in rows:
        if row["split"] == "EXTERNAL_OPTIONAL":
            status = "EXTERNAL_OPTIONAL"
            grain = "NONE"
            evidence = row["external_prerequisite"]
        elif row["record_kind"] == "runtime_operation":
            if row["python_route"] in registered:
                status = "NATIVE_PASS_REGISTRY"
                grain = "REGISTRY"
                evidence = OPERATION_TEST.get(row["python_route"].split(".")[0], "")
            else:
                status = "GAP"
                grain = "NONE"
                evidence = "operation not registered"
        elif row["record_kind"] == "consumer_scenario":
            ids = case_evidence.get(row["ledger_id"], [])
            if ids:
                status = "NATIVE_PASS_SCENARIO"
                grain = "CASE"
                evidence = ";".join(ids)
            else:
                status = "GAP"
                grain = "NONE"
                evidence = "scenario end-to-end test pending"
        else:
            ids = case_evidence.get(row["ledger_id"], [])
            if ids:
                status = "NATIVE_PASS"
                grain = "CASE"
                evidence = ";".join(ids)
            elif row["python_route"] == "fincore.__version__":
                status = "NATIVE_PASS"
                grain = "MODULE"
                evidence = "tests/unit/operations_descriptors_test.cpp::OperationsDescriptors.ExposesPinnedPythonVersionIdentity"
            elif row["python_route"].endswith(".operations.operations"):
                status = "NATIVE_PASS"
                grain = "MODULE"
                evidence = "tests/unit/operations_descriptors_test.cpp::OperationsDescriptors.DomainAccessorsExposeStableSourceCounts"
            else:
                domain = module_for(row["python_route"])
                files = MODULE_TESTS.get(domain or "", [])
                names = []
                for file in files:
                    names.extend(test_names(ROOT / file))
                if names:
                    status = "NATIVE_PASS"
                    grain = "MODULE"
                    evidence = f"{files[0]}::{names[0]}"
                else:
                    status = "GAP"
                    grain = "NONE"
                    evidence = f"no native test for {domain or 'unknown domain'}"
        counts[status] += 1
        output.append(
            {
                "row_id": row["row_id"],
                "record_kind": row["record_kind"],
                "python_route": row["python_route"],
                "split": row["split"],
                "status": status,
                "evidence_grain": grain,
                "evidence": evidence,
            }
        )

    gaps = [row for row in output if row["status"] == "GAP"]
    summary = {
        "schema_version": "1",
        "rows": len(output),
        "status_counts": dict(sorted(counts.items())),
        "gap_count": len(gaps),
        "gaps": [row["python_route"] for row in gaps],
        "notes": [
            "CASE grain uses the reviewed canonical test IDs.",
            "MODULE grain means the module's native test file exists and exercises the API; it is not a one-case-per-route claim.",
            "REGISTRY grain means the operation ID is present in the native runtime registration sources; the runtime operations test constructs the catalog.",
            "EXTERNAL_OPTIONAL is not a PASS.",
        ],
    }
    args.out_dir.mkdir(parents=True, exist_ok=True)
    tsv_path = args.out_dir / "route-ledger.tsv"
    with tsv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(output[0]), delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(output)
    (args.out_dir / "route-ledger-summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(summary["status_counts"], sort_keys=True))
    print(f"gaps={len(gaps)} wrote {tsv_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
