#!/usr/bin/env python3
"""Build and validate the Iteration 8 native/optional route split.

The 952-row ledger already contains the 669 canonical Python routes, the 273
runtime-operation rows and the 10 reviewed consumer scenarios.  The operation
inventory is joined by ``canonical_implementation`` instead of being appended,
so no row is counted twice.

Classification is behavioural, not path-text based: a route is optional only
when the pinned source executes a PyMC posterior/sampling API or a live
provider SDK/network call.  Provider contracts, offline validation helpers and
provider-callable wrappers (Fama-French/style) are native targets because the
pinned source takes an injected provider and imports no SDK.
"""

from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LEDGER = ROOT / "docs/iterations/capability-dispositions.json"
DEFAULT_OPERATIONS = (
    ROOT
    / "docs/evidence/revised-plan-20261023/capability-inventory/operation-catalog-reconciliation.tsv"
)
DEFAULT_OUT_DIR = ROOT / "docs/iterations/iteration-8-full-python-parity"
DEFAULT_SURVIVAL = ROOT / "docs/evidence/iteration-8/p100/new-pin-survival.json"
DEFAULT_ORACLE_ROOT = Path("/Users/yunjinqi/Documents/new_projects/fincore")

ORACLE_PIN = "3e9ef205ef2023f92e89fddd1052d46b16429c64"
LEDGER_PIN = "4b8f31b1b59707480d5db9603704d83bbf7e6946"
EXPECTED_ROWS = 952
EXPECTED_CANONICAL = 669
EXPECTED_OPERATIONS = 273
EXPECTED_SCENARIOS = 10

NATIVE = "NATIVE_TARGET"
OPTIONAL = "EXTERNAL_OPTIONAL"
SPLITS = {NATIVE, OPTIONAL}
RECORD_KINDS = ("canonical_route", "runtime_operation", "consumer_scenario")

PYMC_ROUTES = {
    "fincore.metrics.bayesian.model_best",
    "fincore.metrics.bayesian.model_returns_normal",
    "fincore.metrics.bayesian.model_returns_t",
    "fincore.metrics.bayesian.model_returns_t_alpha_beta",
    "fincore.metrics.bayesian.model_stoch_vol",
    "fincore.metrics.bayesian.run_model",
}

LIVE_PROVIDER_ROUTES = {
    "fincore.data.providers.get_provider",
    "fincore.data.providers.fetch_price_data",
    "fincore.data.providers.fetch_multiple_prices",
}
for _provider, _sdk in (
    ("YahooFinanceProvider", "yfinance + live network"),
    ("AlphaVantageProvider", "requests + AlphaVantage API key"),
    ("TushareProvider", "tushare + token"),
    ("AkShareProvider", "akshare + live network"),
):
    for _suffix in ("", ".fetch", ".get_info"):
        LIVE_PROVIDER_ROUTES.add(f"fincore.data.providers.{_provider}{_suffix}")

PREREQUISITES = {
    **{route: "pymc" for route in PYMC_ROUTES},
    "fincore.data.providers.YahooFinanceProvider": "yfinance + live network",
    "fincore.data.providers.YahooFinanceProvider.fetch": "yfinance + live network",
    "fincore.data.providers.YahooFinanceProvider.get_info": "yfinance + live network",
    "fincore.data.providers.AlphaVantageProvider": "requests + AlphaVantage API key",
    "fincore.data.providers.AlphaVantageProvider.fetch": "requests + AlphaVantage API key",
    "fincore.data.providers.AlphaVantageProvider.get_info": "requests + AlphaVantage API key",
    "fincore.data.providers.TushareProvider": "tushare + token",
    "fincore.data.providers.TushareProvider.fetch": "tushare + token",
    "fincore.data.providers.TushareProvider.get_info": "tushare + token",
    "fincore.data.providers.AkShareProvider": "akshare + live network",
    "fincore.data.providers.AkShareProvider.fetch": "akshare + live network",
    "fincore.data.providers.AkShareProvider.get_info": "akshare + live network",
    "fincore.data.providers.get_provider": "live provider SDK selected by name",
    "fincore.data.providers.fetch_price_data": "live provider SDK/network (default yfinance)",
    "fincore.data.providers.fetch_multiple_prices": "live provider SDK/network (default yfinance)",
}

PHASE_BY_DOMAIN = {
    "metrics": "102",
    "portfolio": "102",
    "performance": "102",
    "risk": "103",
    "optimization": "103",
    "simulation": "103",
    "attribution": "103",
    "factor_analysis": "104",
    "runtime": "105",
    "data": "105",
    "viz": "106",
    "extensions": "107",
    "exceptions": "101",
    "report": "108",
    "consumer_scenario": "109",
    "__version__": "110",
}

CATEGORY_BY_DOMAIN = {
    "metrics": "native_compute",
    "portfolio": "native_compute",
    "performance": "native_compute",
    "risk": "native_compute",
    "optimization": "native_compute",
    "simulation": "native_compute",
    "attribution": "native_compute",
    "factor_analysis": "native_factor_surface",
    "runtime": "native_runtime_catalog",
    "viz": "native_visualization_artifact",
    "extensions": "native_plugin_abi",
    "exceptions": "native_error_semantics",
    "report": "native_report_artifact",
    "__version__": "native_metadata",
}

REASON_BY_CATEGORY = {
    "native_compute": "R3 native behavioural parity: computational/tabular behaviour representable with DataFrame/vector types",
    "native_factor_surface": "R3 native factor surface: computational/tabular/chart-data behaviour on native panels",
    "native_runtime_catalog": "R3 native runtime catalog: typed inputs, deterministic digests and registration",
    "native_visualization_artifact": "R3 native artifact: chart data plus ImPlot/Plotly-compatible/Bokeh-compatible/PDF equivalent",
    "native_plugin_abi": "R3 native plugin ABI: discovery, registration, fingerprint and lifecycle behaviour",
    "native_error_semantics": "R3 structured native error: category, code, report map and aliases",
    "native_report_artifact": "R3 native report artifact: HTML/ECharts/XLSX/PDF with value-equivalent tables and series",
    "native_provider_model": "R3 native provider model: interface/offline validation; no live SDK call in this route",
    "native_data_model": "R3 native data model: snapshot/validation behaviour on native containers",
    "native_metadata": "R3 installed-consumer metadata identity",
    "consumer_scenario": "R3 reviewed consumer scenario; not a Python API route",
}

EVIDENCE_BY_CATEGORY = {
    "native_compute": "native_test+oracle_fixture",
    "native_factor_surface": "native_test+oracle_fixture",
    "native_runtime_catalog": "typed_input_contract+digest+native_test",
    "native_visualization_artifact": "chart_data_fixture+offscreen_or_document_value_check",
    "native_plugin_abi": "plugin_abi_lifecycle_test",
    "native_error_semantics": "error_contract_test",
    "native_report_artifact": "report_artifact_fixture+value_check",
    "native_provider_model": "offline_validation_test",
    "native_data_model": "data_model_contract_test",
    "native_metadata": "installed_consumer_metadata_check",
    "consumer_scenario": "end_to_end_scenario_test",
}

CLASSIFICATION_SOURCES = (
    ("fincore/metrics/bayesian.py", "dda33c4eeff9bfe1e50269d0f9bf38b67d2c0b60ce545432a038616dbb49afb6"),
    ("fincore/data/providers.py", "895f51cd6fd60615abeafed13b39ef001251b3d02141c0da9eda139e7674e92f"),
    ("fincore/data/contracts.py", "5c72f9bcde298c37d3a2c1acee9010d634aaa29c5ce89168663178e921af5232"),
    ("fincore/attribution/fama_french.py", "cea545427783bc14fda44d188ee79f0e4cd66dc8a70c4d8cc2f34c9306276792"),
    ("fincore/attribution/style.py", "cdd6ff570e1d61baeb3ed24ced593380183833af0470afcc9ffed749b982fcce"),
)

TSV_COLUMNS = [
    "row_id",
    "record_kind",
    "python_route",
    "ledger_id",
    "canonical_implementation",
    "old_disposition",
    "old_verification_status",
    "new_pin_survival_status",
    "split",
    "category",
    "implementation_phase",
    "owner_phase",
    "reason",
    "evidence_expected",
    "external_prerequisite",
    "notes",
]


def sha256_file(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def oracle_state(root: Path) -> dict:
    if not root.exists():
        return {
            "root": str(root),
            "head": "NOT_MATERIALIZED",
            "head_matches_pin": False,
            "sources": [
                {"path": path, "sha256": digest, "observed": "NOT_MATERIALIZED"}
                for path, digest in CLASSIFICATION_SOURCES
            ],
        }
    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    sources = []
    for path, digest in CLASSIFICATION_SOURCES:
        source = root / path
        observed = sha256_file(source) if source.exists() else "MISSING"
        sources.append(
            {
                "path": path,
                "sha256": digest,
                "observed": observed,
                "matches_expected": observed == digest,
            }
        )
    return {
        "root": str(root),
        "head": head,
        "head_matches_pin": head == ORACLE_PIN,
        "sources": sources,
    }


def domain_of(route: str) -> str:
    if route.startswith("revised-plan-scenario:"):
        return "consumer_scenario"
    stripped = route[len("fincore.") :] if route.startswith("fincore.") else route
    return stripped.split(".")[0].split(":")[0]


def load_ledger(path: Path) -> dict:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("source_pin") != LEDGER_PIN:
        raise SystemExit(f"unexpected ledger pin {payload.get('source_pin')!r}")
    routes = payload.get("routes", [])
    if len(routes) != EXPECTED_ROWS:
        raise SystemExit(f"expected {EXPECTED_ROWS} ledger rows, got {len(routes)}")
    return payload


def load_operations(path: Path) -> list[dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    if len(rows) != EXPECTED_OPERATIONS:
        raise SystemExit(f"expected {EXPECTED_OPERATIONS} operations, got {len(rows)}")
    return rows


def load_survival(path: Path) -> dict[str, str]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if payload.get("new_pin") != ORACLE_PIN:
        raise SystemExit(f"unexpected survival pin {payload.get('new_pin')!r}")
    mapping = {
        "SURVIVED_IDENTICAL": "SOURCE_UNCHANGED",
        "SURVIVED_BODY_CHANGED": "SOURCE_BODY_CHANGED",
        "PLAN_DERIVED_SCENARIO": "PLAN_DERIVED",
    }
    return {
        row["python_route"]: mapping.get(row["verdict"], "NOT_RUN")
        for row in payload["rows"]
    }


def optional_evidence(reason: str, prerequisite: str) -> str:
    return f"offline_model_evidence_or_named_absence; prerequisite={prerequisite}; no PASS claim"


def build_rows(ledger: dict, operations: list[dict],
               survival: dict[str, str] | None = None) -> list[dict]:
    survival = survival or {}
    routes = ledger["routes"]
    canonical = {row["python_route"]: row for row in routes if row["python_route"].startswith("fincore.")}
    operations_by_id = {row["operation_id"]: row for row in operations}
    expected_canonical = {row["python_route"] for row in routes if row["python_route"].startswith("fincore.")}
    if set(canonical) != expected_canonical:
        raise SystemExit("duplicate canonical routes in ledger")
    if set(operations_by_id) != {
        row["python_route"]
        for row in routes
        if not row["python_route"].startswith("fincore.")
        and not row["python_route"].startswith("revised-plan-scenario:")
    }:
        raise SystemExit("ledger operation rows and operation inventory differ")

    rows: list[dict] = []
    for route in routes:
        python_route = route["python_route"]
        notes: list[str] = []
        if python_route.startswith("revised-plan-scenario:"):
            record_kind = "consumer_scenario"
            canonical_route = ""
            category = "consumer_scenario"
            split = NATIVE
            implementation_phase = "109"
            owner_phase = "109"
            reason = REASON_BY_CATEGORY[category]
            evidence = EVIDENCE_BY_CATEGORY[category]
            prerequisite = ""
        elif python_route.startswith("fincore."):
            record_kind = "canonical_route"
            canonical_route = python_route
            category = category_for(python_route)
            split = OPTIONAL if python_route in PYMC_ROUTES | LIVE_PROVIDER_ROUTES else NATIVE
            implementation_phase = (
                "101" if category == "native_error_semantics" else PHASE_BY_DOMAIN[domain_of(python_route)]
            )
            owner_phase = "100" if split == OPTIONAL else owner_phase_for(python_route, category)
            if split == OPTIONAL:
                prerequisite = PREREQUISITES[python_route]
                if python_route in PYMC_ROUTES:
                    category = "external_pymc_posterior"
                    reason = "Requires PyMC model construction/sampling/posterior objects; no native object identity"
                else:
                    category = "external_live_provider"
                    reason = f"Requires live provider SDK/network ({prerequisite}); native offline model only"
                evidence = optional_evidence(reason, prerequisite)
            else:
                prerequisite = ""
                reason = REASON_BY_CATEGORY[category]
                evidence = EVIDENCE_BY_CATEGORY[category]
                if route["verification_status"] == "PASS_CURRENT_NATIVE" and category in {
                    "native_compute",
                    "native_factor_surface",
                    "native_data_model",
                    "native_provider_model",
                    "native_runtime_catalog",
                }:
                    evidence = "existing_native_test+oracle_recheck"
                elif route["verification_status"].startswith("PARTIAL_"):
                    evidence = "member_level_native_test+oracle_fixture"
                    notes.append("PARTIAL class: accept member-by-member (R3 requirements section 7)")
        else:
            record_kind = "runtime_operation"
            operation = operations_by_id[python_route]
            canonical_id = operation["canonical_implementation"]
            target = next((row for row in routes if row["canonical_id"] == canonical_id), None)
            if target is None:
                raise SystemExit(f"unresolved operation canonical {python_route} -> {canonical_id}")
            canonical_route = target["python_route"]
            category = "runtime_operation"
            split = OPTIONAL if canonical_route in PYMC_ROUTES | LIVE_PROVIDER_ROUTES else NATIVE
            implementation_phase = PHASE_BY_DOMAIN[domain_of(canonical_route)]
            owner_phase = "100" if split == OPTIONAL else "105"
            if split == OPTIONAL:
                prerequisite = PREREQUISITES[canonical_route]
                reason = f"Runtime operation of optional route {canonical_route}; not registered"
                evidence = optional_evidence(reason, prerequisite)
            else:
                prerequisite = ""
                reason = f"Runtime operation of {canonical_route}; inherits NATIVE_TARGET"
                evidence = EVIDENCE_BY_CATEGORY["native_runtime_catalog"]
            notes.append(f"inherits split of {canonical_route}")

        survival_status = survival.get(
            python_route, route["new_pin_survival_status"])
        if survival_status == "SOURCE_BODY_CHANGED":
            notes.append("new-pin body changed; behavior recheck required")
        elif survival_status == "PLAN_DERIVED":
            notes.append("plan-derived scenario; text preservation recorded (10/10)")
        elif survival_status != "SOURCE_UNCHANGED":
            notes.append("new-pin survival NOT_RUN; P100 must verify before closeout")
        rows.append(
            {
                "record_kind": record_kind,
                "python_route": python_route,
                "ledger_id": route["canonical_id"],
                "canonical_implementation": canonical_route if record_kind == "runtime_operation" else "",
                "old_disposition": route["disposition"],
                "old_verification_status": route["verification_status"],
                "new_pin_survival_status": survival_status,
                "split": split,
                "category": category,
                "implementation_phase": implementation_phase,
                "owner_phase": owner_phase,
                "reason": reason,
                "evidence_expected": evidence,
                "external_prerequisite": prerequisite,
                "notes": "; ".join(notes),
            }
        )

    order = {kind: index for index, kind in enumerate(RECORD_KINDS)}
    rows.sort(key=lambda row: (order[row["record_kind"]], row["python_route"], row["ledger_id"]))
    for index, row in enumerate(rows, start=1):
        row["row_id"] = f"I8-{index:04d}"
    return rows


def category_for(route: str) -> str:
    domain = domain_of(route)
    if domain == "factor_analysis":
        if ".exceptions." in route:
            return "native_error_semantics"
        if ".tears." in route:
            return "native_report_artifact"
        if ".render_matplotlib." in route:
            leaf = route.rsplit(".", 1)[-1]
            return "native_report_artifact" if leaf.startswith("build_") else "native_visualization_artifact"
        if route.endswith(".operations") or ".operations." in route:
            return "native_runtime_catalog"
        return "native_factor_surface"
    if domain == "data":
        return "native_provider_model" if ".providers." in route or ".contracts." in route else "native_data_model"
    return CATEGORY_BY_DOMAIN[domain]


def owner_phase_for(route: str, category: str) -> str:
    if category == "native_error_semantics":
        return "101"
    if domain_of(route) == "factor_analysis":
        if category == "native_report_artifact":
            return "108"
        if category == "native_visualization_artifact":
            return "106"
        if category == "native_runtime_catalog":
            return "105"
    return PHASE_BY_DOMAIN[domain_of(route)]


def validate(rows: list[dict], ledger: dict, operations: list[dict]) -> tuple[list[str], dict]:
    errors: list[str] = []
    canonical = {row["python_route"]: row for row in rows if row["record_kind"] == "canonical_route"}
    operation_rows = [row for row in rows if row["record_kind"] == "runtime_operation"]
    scenario_rows = [row for row in rows if row["record_kind"] == "consumer_scenario"]
    fields = ("row_id", "record_kind", "python_route", "split", "category", "reason", "evidence_expected")

    if len(rows) != EXPECTED_ROWS:
        errors.append(f"row_count={len(rows)} expected {EXPECTED_ROWS}")
    if len(canonical) != EXPECTED_CANONICAL:
        errors.append(f"canonical_count={len(canonical)} expected {EXPECTED_CANONICAL}")
    if len(operation_rows) != EXPECTED_OPERATIONS:
        errors.append(f"operation_count={len(operation_rows)} expected {EXPECTED_OPERATIONS}")
    if len(scenario_rows) != EXPECTED_SCENARIOS:
        errors.append(f"scenario_count={len(scenario_rows)} expected {EXPECTED_SCENARIOS}")
    if len({row["row_id"] for row in rows}) != len(rows):
        errors.append("duplicate row_id")
    if len({(row["record_kind"], row["python_route"]) for row in rows}) != len(rows):
        errors.append("duplicate (record_kind, python_route)")
    for row in rows:
        for field in fields:
            if not str(row.get(field, "")).strip():
                errors.append(f"empty {field} for {row.get('python_route', '<unknown>')}")

    ledger_ids = {row["canonical_id"] for row in ledger["routes"]}
    for row in operation_rows:
        if row["canonical_implementation"] not in canonical:
            errors.append(f"unresolved operation canonical {row['python_route']} -> {row['canonical_implementation']}")
        if row["split"] != canonical.get(row["canonical_implementation"], {}).get("split"):
            errors.append(f"operation split differs from canonical for {row['python_route']}")

    expected_optional_canonical = PYMC_ROUTES | LIVE_PROVIDER_ROUTES
    present_optional_canonical = {row["python_route"] for row in canonical.values() if row["split"] == OPTIONAL}
    if present_optional_canonical != expected_optional_canonical:
        errors.append(
            "optional canonical set differs: "
            f"missing={sorted(expected_optional_canonical - present_optional_canonical)} "
            f"extra={sorted(present_optional_canonical - expected_optional_canonical)}"
        )
    optional_rows = [row for row in rows if row["split"] == OPTIONAL]
    expected_optional_operations = {
        row["python_route"]
        for row in operation_rows
        if row["canonical_implementation"] in expected_optional_canonical
    }
    if {row["python_route"] for row in optional_rows if row["record_kind"] == "runtime_operation"} != expected_optional_operations:
        errors.append("optional operation set differs from canonical-derived set")
    for row in optional_rows:
        if not row["external_prerequisite"]:
            errors.append(f"optional row lacks prerequisite: {row['python_route']}")
    for row in rows:
        if row["split"] == NATIVE and row["external_prerequisite"]:
            errors.append(f"native row carries prerequisite: {row['python_route']}")

    partial_classes = {
        row["python_route"]
        for row in ledger["routes"]
        if row["verification_status"].startswith("PARTIAL_")
    }
    if len(partial_classes) != 10:
        errors.append(f"partial class count={len(partial_classes)} expected 10")
    for route in partial_classes:
        row = canonical.get(route)
        if row is None:
            errors.append(f"missing partial class row {route}")
        elif "member_level" not in row["evidence_expected"]:
            errors.append(f"partial class not member-level {route}")

    for scenario_id in (f"revised-plan-scenario:{index:02d}" for index in range(1, 11)):
        if scenario_id not in {row["python_route"] for row in scenario_rows}:
            errors.append(f"missing scenario {scenario_id}")

    for operation in operations:
        if operation["operation_id"] not in {row["python_route"] for row in operation_rows}:
            errors.append(f"operation missing from split {operation['operation_id']}")
        if operation["canonical_implementation"] not in ledger_ids:
            errors.append(f"operation canonical not in ledger {operation['canonical_implementation']}")

    split_counts = Counter(row["split"] for row in rows)
    if split_counts[NATIVE] + split_counts[OPTIONAL] != EXPECTED_ROWS:
        errors.append("split counts do not sum to ledger")
    expected_native = EXPECTED_ROWS - (len(expected_optional_canonical) + len(expected_optional_operations))
    if split_counts[NATIVE] != expected_native:
        errors.append(f"native count={split_counts[NATIVE]} expected {expected_native}")
    if split_counts[OPTIONAL] != len(expected_optional_canonical) + len(expected_optional_operations):
        errors.append(f"optional count={split_counts[OPTIONAL]} unexpected")

    stats = {
        "rows": len(rows),
        "canonical_routes": len(canonical),
        "runtime_operations": len(operation_rows),
        "consumer_scenarios": len(scenario_rows),
        "split": dict(sorted(split_counts.items())),
        "split_by_kind": {
            kind: dict(sorted(Counter(row["split"] for row in rows if row["record_kind"] == kind).items()))
            for kind in RECORD_KINDS
        },
        "categories": dict(sorted(Counter(row["category"] for row in rows).items())),
        "implementation_phases": dict(
            sorted(Counter(row["implementation_phase"] for row in rows).items(), key=lambda item: int(item[0]))
        ),
        "owner_phases": dict(sorted(Counter(row["owner_phase"] for row in rows).items(), key=lambda item: int(item[0]))),
        "old_dispositions": dict(sorted(Counter(row["old_disposition"] for row in rows).items())),
        "old_verification_status": dict(sorted(Counter(row["old_verification_status"] for row in rows).items())),
        "new_pin_survival": dict(sorted(Counter(row["new_pin_survival_status"] for row in rows).items())),
        "partial_classes": len(partial_classes),
        "optional_canonical": len(expected_optional_canonical),
        "optional_operations": len(expected_optional_operations),
    }
    return errors, stats


def self_test(rows: list[dict], ledger: dict, operations: list[dict]) -> int:
    """Every planted defect must make the validator fail."""
    base_errors, _ = validate(rows, ledger, operations)
    if base_errors:
        print("self-test baseline is already invalid:")
        for error in base_errors:
            print("  -", error)
        return 1

    def planted(name: str, mutation) -> bool:
        candidate = copy.deepcopy(rows)
        mutation(candidate)
        errors, _ = validate(candidate, ledger, operations)
        print(f"  {name}: {'FAILS AS EXPECTED' if errors else 'NOT DETECTED'}")
        if not errors:
            print(f"    validator missed planted defect: {name}")
        return bool(errors)

    def duplicate_id(candidate: list[dict]) -> None:
        candidate[1]["row_id"] = candidate[0]["row_id"]

    def bad_split(candidate: list[dict]) -> None:
        candidate[0]["split"] = "PLANNED"

    def unresolved_operation(candidate: list[dict]) -> None:
        next(row for row in candidate if row["record_kind"] == "runtime_operation")[
            "canonical_implementation"
        ] = "missing.route"

    def drop_prerequisite(candidate: list[dict]) -> None:
        next(row for row in candidate if row["split"] == OPTIONAL)["external_prerequisite"] = ""

    def drop_scenario(candidate: list[dict]) -> None:
        candidate.remove(next(row for row in candidate if row["record_kind"] == "consumer_scenario"))

    def downgrade_partial(candidate: list[dict]) -> None:
        next(
            row
            for row in candidate
            if row["old_verification_status"].startswith("PARTIAL_") and row["record_kind"] == "canonical_route"
        )["evidence_expected"] = "native_test+oracle_fixture"

    def promote_native(candidate: list[dict]) -> None:
        next(row for row in candidate if row["split"] == NATIVE)["split"] = OPTIONAL

    defects = [
        ("duplicate row id", duplicate_id),
        ("non-approved split", bad_split),
        ("unresolved operation canonical", unresolved_operation),
        ("optional without prerequisite", drop_prerequisite),
        ("missing consumer scenario", drop_scenario),
        ("class-level partial acceptance", downgrade_partial),
        ("unexpected optional row", promote_native),
    ]
    results = [planted(name, mutation) for name, mutation in defects]
    return 0 if all(results) else 1


def write_outputs(rows: list[dict], summary: dict, out_dir: Path) -> tuple[Path, Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    tsv_path = out_dir / "route-split.tsv"
    with tsv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=TSV_COLUMNS, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    json_path = out_dir / "route-split-summary.json"
    json_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return tsv_path, json_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", type=Path, default=DEFAULT_LEDGER)
    parser.add_argument("--operations", type=Path, default=DEFAULT_OPERATIONS)
    parser.add_argument("--survival", type=Path, default=DEFAULT_SURVIVAL)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--oracle-root", type=Path, default=DEFAULT_ORACLE_ROOT)
    parser.add_argument("--self-test", action="store_true", help="plant defects and require every check to fire")
    args = parser.parse_args()

    oracle = oracle_state(args.oracle_root)
    if oracle["head"] != "NOT_MATERIALIZED" and not oracle["head_matches_pin"]:
        print(
            f"oracle HEAD {oracle['head']} does not match pinned {ORACLE_PIN}",
            file=sys.stderr,
        )
        return 1
    if oracle["head_matches_pin"] and not all(source["matches_expected"] for source in oracle["sources"]):
        print("oracle source hashes do not match the pinned classification evidence", file=sys.stderr)
        return 1

    ledger = load_ledger(args.ledger)
    operations = load_operations(args.operations)
    survival = load_survival(args.survival) if args.survival.exists() else {}
    rows = build_rows(ledger, operations, survival)
    errors, stats = validate(rows, ledger, operations)
    if errors:
        print("route split validation failed:", file=sys.stderr)
        for error in errors:
            print("  -", error, file=sys.stderr)
        return 1

    optional_manifest = [
        {
            "python_route": row["python_route"],
            "record_kind": row["record_kind"],
            "canonical_implementation": row["canonical_implementation"],
            "category": row["category"],
            "external_prerequisite": row["external_prerequisite"],
            "offline_model": (
                "inherits canonical provider data/validation model"
                if row["record_kind"] == "runtime_operation"
                else "provider data/validation model"
                if row["category"] == "external_live_provider"
                else "Bayesian model configuration (posterior objects not native)"
            ),
            "accepted_as_pass": False,
        }
        for row in rows
        if row["split"] == OPTIONAL
    ]
    optional_manifest.sort(key=lambda item: (item["record_kind"], item["python_route"]))

    summary = {
        "schema_version": "1",
        "iteration": 8,
        "phase": "P100",
        "status": "FROZEN_FOR_ITERATION_8_EXECUTION",
        "oracle_pin": ORACLE_PIN,
        "ledger_pin": LEDGER_PIN,
        "ordering": "row_id is assigned after sorting by (record_kind, python_route, ledger_id); ledger inputs are hashed below",
        "inputs": {
            "ledger": {"path": str(args.ledger.relative_to(ROOT)), "sha256": sha256_file(args.ledger)},
            "operations": {"path": str(args.operations.relative_to(ROOT)), "sha256": sha256_file(args.operations)},
            "survival": {"path": str(args.survival.relative_to(ROOT)), "sha256": sha256_file(args.survival)} if args.survival.exists() else {"path": "NOT_PROVIDED"},
            "generator": {"path": str(Path(__file__).resolve().relative_to(ROOT)), "sha256": sha256_file(Path(__file__).resolve())},
        },
        "classification_evidence": {
            "source_pin": ORACLE_PIN,
            "oracle": oracle,
            "rules": [
                "PyMC optional: bayesian.py functions that call _pymc()/pm.Model/pm.sample: model_best, model_returns_normal, model_returns_t, model_returns_t_alpha_beta, model_stoch_vol, run_model",
                "PyMC native: compute_bayes_cone, compute_consistency_score, simulate_paths, summarize_paths, forecast_cone_bootstrap use NumPy only",
                "Live-provider optional: providers.py classes whose constructors call load_optional_module for yfinance/requests/tushare/akshare, their fetch/get_info, and module-level get_provider/fetch_price_data/fetch_multiple_prices",
                "Provider-model native: BatchFetchError, DataProvider and its abstract fetch/get_info contracts, fetch_multiple, to_returns, validate_dates; contracts.RequestPolicy",
                "Injected-provider native: attribution.fama_french.fetch_ff_factors and attribution.style.fetch_style_factors take a caller-supplied provider, import no SDK and return a deep copy",
            ],
        },
        "counts": stats,
        "optional_manifest": optional_manifest,
        "checks": {"passed": True, "checks": ["row/kind counts", "unique ids", "all rows classified", "operation canonical resolution", "optional set exact", "prerequisites only on optional", "partial member-level evidence", "10 scenarios present"]},
        "notes": [
            "The 273 runtime-operation rows are already part of the 952 ledger; joining them to canonical routes avoids counting 952+273 rows.",
            "The ledger contains 669 canonical routes plus 273 runtime operations plus 10 reviewed consumer scenarios.",
            "Path-text mentions of 'provider SDK' in historical scope decisions are not used for classification; only pinned runtime behaviour is.",
            "Fama-French/style fetch wrappers are native because the pinned source uses an injected provider (tests/test_attribution/test_fama_french_provider.py) and imports no SDK.",
            "Optional rows are not PASS; the native offline model is still in scope, and the prerequisite is named.",
            "Owner authorized Iteration 8 execution on 2026-10-24; the native/optional split and its 24-row optional list are frozen at this identity for the acceptance record.",
            "New-pin survival: 935 SOURCE_UNCHANGED (871 pre-reviewed + 64 rechecked identical), 7 SOURCE_BODY_CHANGED (factor analysis performance fixes; behavior recheck required), 10 PLAN_DERIVED scenarios with 10/10 text preservation recorded; zero missing or signature-changed routes.",
        ],
    }

    tsv_path, json_path = write_outputs(rows, summary, args.out_dir)
    print(f"wrote {tsv_path.relative_to(ROOT)} ({len(rows)} rows)")
    print(f"wrote {json_path.relative_to(ROOT)}")
    print(json.dumps(stats["split"], sort_keys=True))
    print(f"optional rows: {[item['python_route'] for item in optional_manifest]}")
    if args.self_test:
        print("validator self-test:")
        return self_test(rows, ledger, operations)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
