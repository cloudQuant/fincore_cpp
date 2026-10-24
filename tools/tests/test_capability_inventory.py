from __future__ import annotations

import sys
import subprocess
import json
import hashlib
from pathlib import Path

import pytest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import capability_inventory as inventory  # noqa: E402


PINNED_REPOSITORY = REPOSITORY_ROOT.parent / "fincore"
PINNED_COMMIT = "4b8f31b1b59707480d5db9603704d83bbf7e6946"


def _git(repository: Path, *args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(repository), *args],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()


def _init_source_repo(root: Path, files: dict[str, str]) -> Path:
    root.mkdir(parents=True)
    for relative, text in files.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    _git(root, "init", "-q")
    _git(root, "config", "user.email", "inventory-test@example.invalid")
    _git(root, "config", "user.name", "Inventory Test")
    _git(root, "add", ".")
    _git(root, "commit", "-q", "-m", "test source")
    return root


def _project_root(root: Path) -> tuple[Path, Path]:
    project = root / "cpp-project"
    header_root = project / "include" / "fincore"
    header_root.mkdir(parents=True)
    (header_root / "api.hpp").write_text("namespace fincore::api { int public_fn(int value); }\n", encoding="utf-8")
    plan = project / "docs" / "plans" / "plan.md"
    plan.parent.mkdir(parents=True)
    plan.write_text(
        "## Bounded implementation slices and exit criteria\n\n"
        "1. Phase 1: scenario one.\n\n"
        "2. Phase 2: scenario two.\n\n"
        "3. Phase 3: scenario three.\n\n"
        "4. Phase 4: scenario four.\n\n"
        "5. Phase 5: scenario five.\n"
        "   2. A separately numbered acceptance detail remains in scenario five.\n\n"
        "## Next\n",
        encoding="utf-8",
    )
    return header_root, plan


def _commit_cpp_candidate(project_root: Path) -> str:
    _git(project_root, "init", "-q")
    _git(project_root, "config", "user.email", "inventory-test@example.invalid")
    _git(project_root, "config", "user.name", "Inventory Test")
    _git(project_root, "add", ".")
    _git(project_root, "commit", "-q", "-m", "tested candidate fixture")
    return _git(project_root, "rev-parse", "HEAD")


def _fixture_files() -> dict[str, str]:
    return {
        "fincore/__init__.py": "from . import api\nfrom .api import Visible as PublicVisible\n__all__ = ['api', 'PublicVisible']\n",
        "fincore/api/__init__.py": "from ._impl import Visible as Visible\nfrom .public import public_fn as renamed_fn\n__all__ = ['Visible', 'renamed_fn']\n",
        "fincore/api/_impl.py": (
            "from dataclasses import dataclass\n"
            "@dataclass\n"
            "class Visible:\n"
            "    value: int = 3\n"
            "    @property\n"
            "    def doubled(self) -> int:\n"
            "        return self.value * 2\n"
            "    @doubled.setter\n"
            "    def doubled(self, value: int) -> None:\n"
            "        self.value = value // 2\n"
            "    def run(self, count: int = 1) -> int:\n"
            "        return self.value * count\n"
        ),
        "fincore/api/public.py": (
            "def public_fn(value: int = 2) -> int:\n"
            "    '''Return the input value.'''\n"
            "    return value\n\n"
            "def duplicate(value: int) -> int:\n"
            "    return value\n"
        ),
        "fincore/api/other.py": "def duplicate(value: int) -> int:\n    return -value\n",
        "fincore/api/unknown.py": "__all__ = ['missing']\n",
        "fincore/api/operations.py": (
            "from .public import public_fn as imported_callable\n"
            "__all__ = ['operations']\n"
            "_BINDINGS = (('api.public.public_fn', imported_callable),)\n"
            "def operations():\n    return ()\n"
        ),
        "fincore/runtime/__init__.py": "",
        "fincore/runtime/builtins.py": "_BUILTIN_PROVIDER_PATHS = ('fincore.api.operations:operations',)\n",
    }


def test_recursively_inventories_modules_aliases_models_and_methods(tmp_path: Path) -> None:
    source_root = _init_source_repo(tmp_path / "source", _fixture_files())
    cpp_root, plan = _project_root(tmp_path)
    runtime_source = cpp_root.parents[1] / "src" / "runtime" / "runtime.cpp"
    runtime_source.parent.mkdir(parents=True)
    runtime_source.write_text('void f() {\n    add("api.public.public_fn", "api", "public_fn", call);\n}\n', encoding="utf-8")
    builder = inventory.InventoryBuilder(
        inventory.GitSource(source_root, "HEAD"),
        cpp_root,
        plan,
        expected_operation_count=1,
    )

    rows, unresolved, summary = builder.build()

    capabilities = [row for row in rows if row["record_kind"] == "capability"]
    by_route = {row["python_route"]: row for row in capabilities}
    assert "fincore.api" in {row["python_route"] for row in rows if row["record_kind"] == "module"}
    visible = by_route["fincore.api._impl.Visible"]
    assert visible["kind"] == "dataclass"
    assert "fincore.api.Visible" in visible["aliases"]
    assert "fincore.PublicVisible" in visible["aliases"]
    assert by_route["fincore.api._impl.Visible.run"]["kind"] == "method"
    assert by_route["fincore.api._impl.Visible.doubled"]["kind"] == "property"
    assert by_route["fincore.api._impl.Visible.doubled.setter"]["kind"] == "property_setter"
    assert "fincore.api.renamed_fn" in by_route["fincore.api.public.public_fn"]["aliases"]
    assert summary["operation_catalog_observed_binding_count"] == 1
    assert summary["native_runtime_catalog_unique_id_count"] == 1
    assert summary["source_catalog_native_reconciliation"]["native_exact_registration_count"] == 1
    assert summary["consumer_scenario_count"] == 5
    assert any(row["issue_code"] == "UNKNOWN_PUBLIC_EXPORT" and row["python_route"].endswith("unknown.missing") for row in unresolved)


def test_duplicate_names_remain_distinct_and_operation_alias_links_back(tmp_path: Path) -> None:
    source_root = _init_source_repo(tmp_path / "source", _fixture_files())
    cpp_root, plan = _project_root(tmp_path)
    rows, _, _ = inventory.InventoryBuilder(
        inventory.GitSource(source_root, "HEAD"), cpp_root, plan, expected_operation_count=1
    ).build()

    duplicates = [row for row in rows if row["record_kind"] == "capability" and row["python_route"].endswith(".duplicate")]
    assert len(duplicates) == 2
    assert len({row["canonical_id"] for row in duplicates}) == 2
    operation = next(row for row in rows if row["record_kind"] == "operation_binding")
    callable_row = next(row for row in rows if row["canonical_id"] == operation["canonical_id"])
    assert operation["python_route"] == "api.public.public_fn"
    assert operation["canonical_id"] == callable_row["canonical_id"]
    assert "api.public.public_fn" in callable_row["aliases"]


def test_pinned_catalog_and_summary_are_deterministic(tmp_path: Path) -> None:
    source = inventory.GitSource(PINNED_REPOSITORY, PINNED_COMMIT)
    cpp_root = REPOSITORY_ROOT / "include" / "fincore"
    plan = REPOSITORY_ROOT / "docs" / "plans" / "2026-10-23-revised-plan-completion.md"
    rows_a, unresolved_a, summary_a = inventory.InventoryBuilder(source, cpp_root, plan).build()
    rows_b, unresolved_b, summary_b = inventory.InventoryBuilder(source, cpp_root, plan).build()

    assert summary_a == summary_b
    assert summary_a["operation_catalog_expected_count"] == 273
    assert summary_a["operation_catalog_observed_binding_count"] == 273
    assert summary_a["operation_catalog_unique_id_count"] == 273
    assert summary_a["native_runtime_catalog_registration_occurrence_count"] == 48
    assert summary_a["native_runtime_catalog_unique_id_count"] == 48
    assert summary_a["native_runtime_catalog_committed_unique_id_count"] == 44
    assert summary_a["native_runtime_catalog_current_tree_added_ids"] == [
        "report.portfolio.build_portfolio_report",
        "report.risk.build_risk_report",
        "risk.diagnostics.walk_forward_var",
        "risk.report.build_risk_validation_report",
    ]
    assert summary_a["source_catalog_native_reconciliation"]["native_exact_registration_count"] == 48
    assert summary_a["source_catalog_native_reconciliation"]["unregistered_source_count"] == 225
    assert summary_a["native_runtime_catalog_source_only_ids"] == []
    assert summary_a["phase92_acceptance_reference"]["asserted_catalog_count"] == 44
    assert summary_a["phase92_acceptance_reference"]["domain_table_source_count_sum"] == 270
    assert summary_a["phase92_acceptance_reference"]["domain_table_registered_count_sum"] == 20
    assert summary_a["phase92_acceptance_reference"]["source_domains_missing_from_document_table"] == ["optimization"]
    assert summary_a["source_catalog_native_reconciliation"]["unmatched_group_counts"]["explicitly_excluded_domain_unmatched"] == len(
        summary_a["source_catalog_native_reconciliation"]["domain_rollup"].get("data", {}).get("unregistered_ids", [])
    )
    assert rows_a == rows_b
    assert unresolved_a == unresolved_b


def test_strict_validation_rejects_missing_fields_and_acceptance_claims() -> None:
    row = {column: "value" for column in inventory.LEDGER_COLUMNS}
    row.update({"row_id": "one", "record_kind": "capability", "canonical_id": "one", "pin": PINNED_COMMIT,
                "source_sha256": "0" * 64, "disposition": "ACCEPTED_EXISTING"})
    with pytest.raises(ValueError, match="must never assert ACCEPTED_EXISTING"):
        inventory.validate_inventory([row], [], PINNED_COMMIT)


def test_strict_validation_requires_every_ledger_field() -> None:
    row = {column: "value" for column in inventory.LEDGER_COLUMNS}
    row.update({
        "row_id": "one",
        "record_kind": "capability",
        "canonical_id": "one",
        "pin": PINNED_COMMIT,
        "source_sha256": "0" * 64,
        "disposition": "PLANNED",
        "source_file": "",
    })
    with pytest.raises(ValueError, match=r"lacks required fields: \['source_file'\]"):
        inventory.validate_inventory([row], [], PINNED_COMMIT)


def test_strict_validation_requires_reconciliation_artifact_fields() -> None:
    with pytest.raises(ValueError, match="lacks required fields"):
        inventory.validate_generated_table(
            [{"operation_id": "sample.operation"}],
            inventory.OPERATION_RECONCILIATION_COLUMNS,
            "operation_id",
        )


def test_source_signature_retains_defaults_and_argument_kinds() -> None:
    module = inventory.ast.parse("def f(value: int = 4, /, *args: str, flag: bool = True, **kwargs: object) -> int: ...")
    function = next(node for node in module.body if isinstance(node, inventory.ast.FunctionDef))
    signature = inventory.source_signature(function)

    assert signature["parameters"][0] == {
        "name": "value", "kind": "positional_only", "annotation": "int", "default": "4"
    }
    assert any(parameter["kind"] == "var_positional" for parameter in signature["parameters"])
    assert any(parameter["kind"] == "keyword_only" and parameter["default"] == "True" for parameter in signature["parameters"])
    assert signature["return_annotation"] == "int"


def test_candidate_native_route_strings_remain_single_routes() -> None:
    native_route = "include/fincore/dataframe.hpp:25-29 and src/dataframe.cpp:198-211"

    assert inventory._candidate_native_routes({"candidate_native_routes": native_route}) == ([native_route], [])
    assert inventory._candidate_native_routes({"candidate_native_routes": list(native_route)}) == ([native_route], [])
    assert inventory._candidate_native_routes({"native_headers": "include/fincore/dataframe.hpp"}) == (
        [], ["include/fincore/dataframe.hpp"]
    )


def test_reviewed_mapping_input_keeps_aliases_grouped_and_unknown_routes_visible(tmp_path: Path) -> None:
    source_root = _init_source_repo(tmp_path / "source", _fixture_files())
    cpp_root, plan = _project_root(tmp_path)
    source = inventory.GitSource(source_root, "HEAD")
    builder = inventory.InventoryBuilder(source, cpp_root, plan, expected_operation_count=1)
    rows, _, _ = builder.build()
    acceptance_path = cpp_root.parents[1] / "docs/acceptance/test.md"
    acceptance_path.parent.mkdir(parents=True)
    acceptance_path.write_text("Historical acceptance candidate evidence.\n", encoding="utf-8")
    candidate_path = tmp_path / "fincore-foundations-mapping-candidates.json"
    candidate_path.write_text(
        json.dumps(
            {
                "python_source": {"pin": source.commit},
                "candidate_groups": [
                    {
                        "routes": ["fincore.api.renamed_fn", "fincore.api.missing_route"],
                        "classification": "mapped renamed leaf",
                        "native_route": "api::public_fn",
                        "native_headers": ["include/fincore/api.hpp"],
                        "tests": {"tests/unit/api_test.cpp": ["Api.MatchesInputs"]},
                        "acceptance_refs": ["docs/acceptance/test.md:1"],
                    }
                ],
                "candidate_module_gap_routes": ["fincore.api"],
            },
            sort_keys=True,
        ),
        encoding="utf-8",
    )

    mapping = inventory.build_reviewed_mapping_input([candidate_path], rows, pin=source.commit)
    inventory.validate_reviewed_mapping_input(mapping, rows, source.commit)
    assert mapping == inventory.build_reviewed_mapping_input([candidate_path], rows, pin=source.commit)

    callable_row = next(row for row in rows if row["python_route"] == "fincore.api.public.public_fn")
    route_record = next(record for record in mapping["routes"] if record["route_id"] == callable_row["row_id"])
    assert "fincore.api.renamed_fn" in route_record["aliases"]
    assert route_record["aliases_count"] == 2
    assert route_record["candidate_mappings"][0]["route_resolution"] == "ALIAS_ROUTE"
    assert route_record["candidate_review"]["proposed_disposition"] == "ACCEPTED_EXISTING"
    assert route_record["candidate_review"]["proposal_status"] == "HISTORICAL_ACCEPTANCE_CANDIDATE_GROUP_EVIDENCE_UNVERIFIED"
    assert mapping["unmatched_candidate_route_count"] == 1
    assert mapping["unmatched_candidate_routes"][0]["candidate_route"] == "fincore.api.missing_route"
    module_record = next(record for record in mapping["routes"] if record["record_kind"] == "module")
    assert module_record["candidate_review"]["proposal_status"] == "NAMESPACE_CONTAINER_NOT_EXECUTABLE"
    assert module_record["candidate_review"]["proposed_disposition"] != "ACCEPTED_EXISTING"
    queue = inventory.build_primary_review_queue(rows, mapping, builder)
    assert any(
        item["queue_kind"] == "HISTORICAL_ACCEPTANCE_CANDIDATE"
        and item["python_route"] == callable_row["python_route"]
        for item in queue
    )
    assert all(item["python_route"] != "fincore.api" for item in queue)


def test_explicit_route_disposition_requires_pinned_source_and_historical_evidence(tmp_path: Path) -> None:
    source_root = _init_source_repo(tmp_path / "source", _fixture_files())
    cpp_root, plan = _project_root(tmp_path)
    acceptance_path = cpp_root.parents[1] / "docs/acceptance/test.md"
    acceptance_path.parent.mkdir(parents=True)
    acceptance_path.write_text("Historical acceptance evidence.\n", encoding="utf-8")
    acceptance_sha = hashlib.sha256(acceptance_path.read_bytes()).hexdigest()
    current_acceptance_path = cpp_root.parents[1] / "docs/iterations/current-acceptance.md"
    current_acceptance_path.parent.mkdir(parents=True)
    current_acceptance_path.write_text("Current native candidate evidence.\n", encoding="utf-8")
    current_acceptance_sha = hashlib.sha256(current_acceptance_path.read_bytes()).hexdigest()
    test_path = cpp_root.parents[1] / "tests/unit/api_test.cpp"
    test_path.parent.mkdir(parents=True)
    test_path.write_text("TEST(Api, MatchesInputs) {}\n", encoding="utf-8")
    tested_candidate_commit = _commit_cpp_candidate(cpp_root.parents[1])
    source = inventory.GitSource(source_root, "HEAD")
    builder = inventory.InventoryBuilder(source, cpp_root, plan, expected_operation_count=1)
    base_rows, _, _ = builder.build()
    target = next(row for row in base_rows if row["python_route"] == "fincore.api.public.public_fn")
    entry = {
        "python_route": target["python_route"],
        "canonical_id": target["canonical_id"],
        "source_sha256": target["source_sha256"],
        "disposition": "ACCEPTED_EXISTING",
        "native_routes": ["api::public_fn"],
        "native_headers": ["include/fincore/api.hpp"],
        "owner_phase": "Phase 1",
        "evidence_test_ids": ["Api.MatchesInputs"],
        "acceptance_refs": [{"path": "docs/acceptance/test.md", "sha256": acceptance_sha, "locator": "lines 1-1"}],
        "tested_candidate_commit": tested_candidate_commit,
        "tested_candidate_dirty_snapshot_id": "CLEAN_TREE",
        "scope_decision": "2026-10-23: exact route acceptance candidate; primary review record.",
        "verification_status": "HISTORICAL_ACCEPTANCE_CITED_NEW_PIN_NOT_RUN",
        "new_pin_survival_status": "NOT_RUN",
    }
    overlay = {
        "schema_version": "2",
        "source_pin": source.commit,
        "primary_reviewed": True,
        "reviewed_by": "test fixture",
        "review_date": "2026-10-23",
        "routes": [entry],
    }

    def new_reviewed_builder() -> inventory.InventoryBuilder:
        return inventory.InventoryBuilder(source, cpp_root, plan, expected_operation_count=1)

    rows, _, summary = new_reviewed_builder().build(overlay)
    accepted = next(row for row in rows if row["canonical_id"] == target["canonical_id"])
    assert accepted["disposition"] == "ACCEPTED_EXISTING"
    assert json.loads(accepted["evidence_test_ids"]) == ["Api.MatchesInputs"]
    assert json.loads(accepted["acceptance_sha"]) == [tested_candidate_commit]
    assert json.loads(accepted["acceptance_doc_sha256"]) == [acceptance_sha]
    assert accepted["tested_candidate_dirty_snapshot_id"] == "CLEAN_TREE"
    assert accepted["new_pin_survival_status"] == "NOT_RUN"
    assert summary["reviewed_dispositions"]["accepted_existing_count"] == 1

    current_entry = dict(
        entry,
        disposition="PLANNED",
        verification_status="PASS_CURRENT_NATIVE",
        acceptance_refs=[{
            "path": "docs/iterations/current-acceptance.md",
            "sha256": current_acceptance_sha,
            "locator": "lines 1-1",
            "role": "current_acceptance",
        }],
    )
    current_rows, _, current_summary = new_reviewed_builder().build({**overlay, "routes": [current_entry]})
    current_route = next(row for row in current_rows if row["canonical_id"] == target["canonical_id"])
    assert current_route["disposition"] == "PLANNED"
    assert current_route["verification_status"] == "PASS_CURRENT_NATIVE"
    assert json.loads(current_route["acceptance_sha"]) == [tested_candidate_commit]
    assert json.loads(current_route["acceptance_doc_sha256"]) == [current_acceptance_sha]
    assert json.loads(current_route["evidence_refs"])[0]["role"] == "current_acceptance"
    assert current_summary["reviewed_dispositions"]["pass_current_native_count"] == 1
    assert current_summary["pass_current_native_row_count"] == 1

    unpaired_candidate = dict(current_entry)
    unpaired_candidate.pop("tested_candidate_commit")
    with pytest.raises(ValueError, match="must be provided together"):
        new_reviewed_builder().build({**overlay, "routes": [unpaired_candidate]})

    missing_dirty_snapshot = dict(entry, tested_candidate_dirty_snapshot_id="NOT_RECORDED")
    with pytest.raises(ValueError, match="must be CLEAN_TREE or DIRTY_SHA256"):
        new_reviewed_builder().build({**overlay, "routes": [missing_dirty_snapshot]})

    unresolvable_candidate = dict(entry, tested_candidate_commit="0" * 40)
    with pytest.raises(ValueError, match=r"must resolve in the C\+\+ project Git repository"):
        new_reviewed_builder().build({**overlay, "routes": [unresolvable_candidate]})

    with pytest.raises(ValueError, match="require primary_reviewed=true"):
        inventory.InventoryBuilder(source, cpp_root, plan, expected_operation_count=1).build(
            {"schema_version": "2", "source_pin": source.commit, "routes": [entry]}
        )

    missing_evidence = dict(entry, evidence_test_ids=[], acceptance_refs=[])
    with pytest.raises(ValueError, match="requires named historical test IDs"):
        inventory.InventoryBuilder(source, cpp_root, plan, expected_operation_count=1).build(
            {
                "schema_version": "2",
                "source_pin": source.commit,
                "primary_reviewed": True,
                "reviewed_by": "test fixture",
                "review_date": "2026-10-23",
                "routes": [missing_evidence],
            }
        )

    false_source_unchanged = dict(entry, new_pin_survival_status="SOURCE_UNCHANGED")
    with pytest.raises(ValueError, match="requires a full historical_source_commit SHA"):
        inventory.InventoryBuilder(source, cpp_root, plan, expected_operation_count=1).build(
            {
                "schema_version": "2",
                "source_pin": source.commit,
                "primary_reviewed": True,
                "reviewed_by": "test fixture",
                "review_date": "2026-10-23",
                "routes": [false_source_unchanged],
            }
        )


def test_source_unchanged_checks_only_exact_python_file_blob(tmp_path: Path) -> None:
    source_root = _init_source_repo(tmp_path / "source", _fixture_files())
    base_pin = _git(source_root, "rev-parse", "HEAD")
    unrelated = source_root / "fincore/api/other.py"
    unrelated.write_text("def duplicate(value: int) -> int:\n    return value + 1\n", encoding="utf-8")
    _git(source_root, "add", "fincore/api/other.py")
    _git(source_root, "commit", "-q", "-m", "change unrelated Python route")
    target_pin = _git(source_root, "rev-parse", "HEAD")
    source = inventory.GitSource(source_root, target_pin)
    route_path = "fincore/api/public.py"
    route_sha = hashlib.sha256(source.read(route_path)).hexdigest()

    inventory._verify_source_unchanged(source, base_pin, route_path, route_sha)

    changed_route = source_root / route_path
    changed_route.write_text(
        "def public_fn(value: int = 2) -> int:\n    return value + 1\n\n"
        "def duplicate(value: int) -> int:\n    return value\n",
        encoding="utf-8",
    )
    _git(source_root, "add", route_path)
    _git(source_root, "commit", "-q", "-m", "change selected route")
    changed_source = inventory.GitSource(source_root, "HEAD")
    with pytest.raises(ValueError, match="source file hash differs at the current Python pin"):
        inventory._verify_source_unchanged(changed_source, base_pin, route_path, route_sha)


def test_class_acceptance_is_rejected_when_public_methods_are_not_accepted(tmp_path: Path) -> None:
    source_root = _init_source_repo(tmp_path / "source", _fixture_files())
    cpp_root, plan = _project_root(tmp_path)
    acceptance_path = cpp_root.parents[1] / "docs/acceptance/test.md"
    acceptance_path.parent.mkdir(parents=True)
    acceptance_path.write_text("Historical acceptance evidence.\n", encoding="utf-8")
    acceptance_sha = hashlib.sha256(acceptance_path.read_bytes()).hexdigest()
    test_path = cpp_root.parents[1] / "tests/unit/api_test.cpp"
    test_path.parent.mkdir(parents=True)
    test_path.write_text("TEST(Api, Visible) {}\n", encoding="utf-8")
    tested_candidate_commit = _commit_cpp_candidate(cpp_root.parents[1])
    source = inventory.GitSource(source_root, "HEAD")
    builder = inventory.InventoryBuilder(source, cpp_root, plan, expected_operation_count=1)
    rows, _, _ = builder.build()
    target = next(row for row in rows if row["python_route"] == "fincore.api._impl.Visible")
    entry = {
        "python_route": target["python_route"],
        "canonical_id": target["canonical_id"],
        "source_sha256": target["source_sha256"],
        "disposition": "ACCEPTED_EXISTING",
        "native_routes": ["api::Visible"],
        "native_headers": ["include/fincore/api.hpp"],
        "owner_phase": "Phase 1",
        "evidence_test_ids": ["Api.Visible"],
        "acceptance_refs": [{"path": "docs/acceptance/test.md", "sha256": acceptance_sha, "locator": "lines 1-1"}],
        "tested_candidate_commit": tested_candidate_commit,
        "tested_candidate_dirty_snapshot_id": "CLEAN_TREE",
        "scope_decision": "2026-10-23: class review candidate.",
        "verification_status": "HISTORICAL_ACCEPTANCE_CITED_NEW_PIN_NOT_RUN",
        "new_pin_survival_status": "NOT_RUN",
    }

    with pytest.raises(ValueError, match="class has unaccepted public members"):
        builder.build(
            {
                "schema_version": "2",
                "source_pin": source.commit,
                "primary_reviewed": True,
                "reviewed_by": "test fixture",
                "review_date": "2026-10-23",
                "routes": [entry],
            }
        )
