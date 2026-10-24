#!/usr/bin/env python3
"""Build a source-pinned inventory of public Python routes and native hints.

The inventory is deliberately a discovery artifact. Native name matches and
historical acceptance references are mapping hints; this tool never marks a
capability accepted.
"""

from __future__ import annotations

import argparse
import ast
import csv
import hashlib
import json
import re
import subprocess
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence


PIN = "4b8f31b1b59707480d5db9603704d83bbf7e6946"
EXPECTED_OPERATION_COUNT = 273
SCHEMA_VERSION = "1"
LEDGER_COLUMNS = [
    "row_id",
    "record_kind",
    "capability_id",
    "canonical_id",
    "python_route",
    "kind",
    "pin",
    "source_file",
    "source_anchor",
    "source_sha256",
    "aliases",
    "contract_json",
    "native_route",
    "native_header",
    "owner_phase",
    "disposition",
    "evidence_test_ids",
    "acceptance_sha",
    "acceptance_doc_sha256",
    "tested_candidate_dirty_snapshot_id",
    "evidence_refs",
    "scope_decision",
    "verification_status",
    "new_pin_survival_status",
]
UNRESOLVED_COLUMNS = [
    "issue_id",
    "capability_id",
    "python_route",
    "issue_code",
    "source_anchor",
    "detail",
    "required_action",
    "status",
]
OPERATION_RECONCILIATION_COLUMNS = [
    "operation_id",
    "provider_module",
    "provider_source_file",
    "provider_source_anchor",
    "provider_source_sha256",
    "canonical_implementation",
    "native_registration_status",
    "native_registration_source",
    "native_registration_line",
    "scope_classification",
    "scope_rationale",
]
ROUTE_SCOPE_COLUMNS = ["python_route", "canonical_id", "source_file", "scope_classification", "scope_rationale", "verification_status"]
REVIEW_QUEUE_COLUMNS = [
    "queue_kind",
    "python_route",
    "canonical_id",
    "kind",
    "source_file",
    "source_sha256",
    "scope_classification",
    "proposed_disposition",
    "candidate_groups",
    "native_routes",
    "evidence_test_ids",
    "acceptance_refs",
    "review_reason",
]
ALLOWED_DISPOSITIONS = {"ACCEPTED_EXISTING", "PLANNED", "DEFERRED", "NON_PORT", "GAP"}
DISPOSITION_SCHEMA_VERSION = "2"
PUBLIC_DOMAIN_PHASES = {
    "metrics": "Iteration 1; historical phases 2A-52b",
    "performance": "Iteration 2; phases 53-57b",
    "portfolio": "Iteration 2 phases 58-59; Iteration 6 phase 91",
    "factor_analysis": "Iteration 5; phases 77-87",
    "simulation": "Historical simulation scope; exact phase owner requires review",
    "optimization": "Iteration 3; phases 60 and 68-71",
    "data": "Iteration 2; provider/snapshot scope decision requires review",
    "risk": "Iteration 3; phases 61-67",
    "attribution": "Historical attribution scope; exact phase owner requires review",
    "report": "Iteration 6; phases 84, 88a-91",
    "runtime": "Iteration 6; phase 92",
    "extensions": "Iteration 6; extension scope decision requires review",
    "viz": "Iteration 4; phases 72-76",
}
CPP_FAMILY = {
    "metrics": "metrics",
    "performance": "performance",
    "portfolio": "portfolio",
    "factor_analysis": "factor",
    "simulation": "simulation",
    "optimization": "optimization",
    "data": "data",
    "risk": "risk",
    "attribution": "attribution",
    "report": "report",
    "runtime": "runtime",
    "extensions": "extensions",
    "viz": "viz",
}
ACCEPTANCE_HINTS = {
    "metrics": ["docs/acceptance/2026-10-23-iteration-1-closeout.md"],
    "performance": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "portfolio": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "factor_analysis": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "simulation": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "optimization": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "data": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "risk": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "attribution": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "report": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "runtime": ["docs/acceptance/2026-10-23-phase-92-runtime-catalog-acceptance.md"],
    "extensions": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
    "viz": ["docs/acceptance/2026-10-23-program-parity-audit.md"],
}


def compact_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def ast_text(node: ast.AST | None) -> str:
    if node is None:
        return ""
    return ast.unparse(node)


def source_signature(node: ast.FunctionDef | ast.AsyncFunctionDef) -> dict[str, Any]:
    args = node.args
    positionals = list(args.posonlyargs) + list(args.args)
    defaults_by_index = len(positionals) - len(args.defaults)
    parameters: list[dict[str, Any]] = []
    for index, arg in enumerate(positionals):
        default_index = index - defaults_by_index
        default = args.defaults[default_index] if default_index >= 0 else None
        parameters.append(
            {
                "name": arg.arg,
                "kind": "positional_only" if index < len(args.posonlyargs) else "positional_or_keyword",
                "annotation": ast_text(arg.annotation),
                "default": ast_text(default) if default is not None else None,
            }
        )
    if args.vararg is not None:
        parameters.append(
            {"name": args.vararg.arg, "kind": "var_positional", "annotation": ast_text(args.vararg.annotation), "default": None}
        )
    for arg, default in zip(args.kwonlyargs, args.kw_defaults):
        parameters.append(
            {
                "name": arg.arg,
                "kind": "keyword_only",
                "annotation": ast_text(arg.annotation),
                "default": ast_text(default) if default is not None else None,
            }
        )
    if args.kwarg is not None:
        parameters.append(
            {"name": args.kwarg.arg, "kind": "var_keyword", "annotation": ast_text(args.kwarg.annotation), "default": None}
        )
    return {
        "signature": ast_text(args),
        "parameters": parameters,
        "return_annotation": ast_text(node.returns),
        "is_async": isinstance(node, ast.AsyncFunctionDef),
    }


def function_contract(node: ast.FunctionDef | ast.AsyncFunctionDef) -> dict[str, Any]:
    return {
        **source_signature(node),
        "source_docstring": ast.get_docstring(node, clean=False),
        "shape_order_index_errors_missing_policy": (
            "See source_docstring and source_anchor; no semantic claims are inferred by this inventory."
        ),
    }


def literal_string_sequence(node: ast.AST | None, env: dict[str, list[str]] | None = None) -> list[str] | None:
    env = env or {}
    if isinstance(node, (ast.List, ast.Tuple, ast.Set)):
        values: list[str] = []
        for item in node.elts:
            if not isinstance(item, ast.Constant) or not isinstance(item.value, str):
                return None
            values.append(item.value)
        return values
    if isinstance(node, ast.Name):
        return list(env[node.id]) if node.id in env else None
    if isinstance(node, ast.BinOp) and isinstance(node.op, ast.Add):
        left = literal_string_sequence(node.left, env)
        right = literal_string_sequence(node.right, env)
        return left + right if left is not None and right is not None else None
    return None


def dotted_name(node: ast.AST | None) -> str:
    if isinstance(node, ast.Name):
        return node.id
    if isinstance(node, ast.Attribute):
        prefix = dotted_name(node.value)
        return f"{prefix}.{node.attr}" if prefix else node.attr
    return ""


@dataclass
class Definition:
    module: str
    name: str
    node: ast.FunctionDef | ast.AsyncFunctionDef | ast.ClassDef
    path: str
    source_sha256: str
    line: int
    end_line: int
    parent_class: str | None = None
    parent_class_line: int | None = None
    member_name: str | None = None
    member_kind: str | None = None

    @property
    def canonical_route(self) -> str:
        if self.parent_class:
            suffix = f".{self.parent_class}.{self.member_name}"
            if self.member_kind in {"property_setter", "property_deleter"}:
                suffix += ".setter" if self.member_kind == "property_setter" else ".deleter"
            return self.module + suffix
        return f"{self.module}.{self.name}" if self.module else self.name

    @property
    def canonical_id(self) -> str:
        return f"{self.canonical_route}@{self.line}"

    @property
    def entity_kind(self) -> str:
        if isinstance(self.node, ast.ClassDef):
            if any(dotted_name(dec).split(".")[-1] == "dataclass" for dec in self.node.decorator_list):
                return "dataclass"
            return "class"
        if self.member_kind:
            return self.member_kind
        if self.parent_class:
            return "async_method" if isinstance(self.node, ast.AsyncFunctionDef) else "method"
        return "async_callable" if isinstance(self.node, ast.AsyncFunctionDef) else "callable"


@dataclass
class Binding:
    kind: str
    target_module: str = ""
    target_name: str = ""
    source: str = ""
    node: ast.AST | None = None


@dataclass
class ModuleInfo:
    module: str
    path: str
    content: bytes
    source_sha256: str
    tree: ast.Module
    is_public: bool
    is_package: bool
    exports: list[str] | None = None
    all_is_dynamic: bool = False
    bindings: dict[str, Binding] = field(default_factory=dict)
    assignments: dict[str, tuple[ast.AST | None, ast.AST | None, int]] = field(default_factory=dict)
    definitions: dict[str, list[Definition]] = field(default_factory=lambda: defaultdict(list))
    classes: dict[str, Definition] = field(default_factory=dict)
    import_star_targets: list[str] = field(default_factory=list)


class GitSource:
    """Read immutable source blobs from a Git commit without touching worktrees."""

    def __init__(self, repository: Path, revision: str) -> None:
        self.repository = repository.resolve()
        self.revision = revision
        resolved = self.git("rev-parse", f"{revision}^{{commit}}").strip()
        if not re.fullmatch(r"[0-9a-f]{40}", resolved):
            raise ValueError(f"Git returned an invalid full commit SHA: {resolved!r}")
        self.commit = resolved
        listing = self.git("ls-tree", "-r", "--name-only", self.commit, "--", "fincore")
        self.paths = sorted(path for path in listing.splitlines() if path.endswith(".py"))
        if not self.paths:
            raise ValueError(f"no Python source files found under fincore at {self.commit}")
        self._cache: dict[str, bytes] = {}

    def git(self, *args: str) -> str:
        result = subprocess.run(
            ["git", "-C", str(self.repository), *args],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return result.stdout

    def read(self, path: str) -> bytes:
        if path not in self._cache:
            result = subprocess.run(
                ["git", "-C", str(self.repository), "show", f"{self.commit}:{path}"],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            self._cache[path] = result.stdout
        return self._cache[path]


def module_name(path: str) -> tuple[str, bool]:
    parts = Path(path).with_suffix("").parts
    is_package = parts[-1] == "__init__"
    if is_package:
        parts = parts[:-1]
    return ".".join(parts), is_package


def is_public_module(path: str) -> bool:
    parts = Path(path).parts
    if not parts or parts[0] != "fincore":
        return False
    return all(not part.startswith("_") for part in parts[1:] if part != "__init__.py")


def module_package(module: str, is_package: bool) -> str:
    return module if is_package else module.rpartition(".")[0]


def resolve_relative_module(module: str, is_package: bool, level: int, target: str | None) -> str:
    if level == 0:
        return target or ""
    base_parts = module_package(module, is_package).split(".")
    keep = max(0, len(base_parts) - (level - 1))
    parts = base_parts[:keep]
    if target:
        parts.extend(target.split("."))
    return ".".join(parts)


def class_member_facts(module_info: ModuleInfo, cls: ast.ClassDef) -> list[Definition]:
    output: list[Definition] = []
    for node in cls.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.name.startswith("_"):
                continue
            property_kind = None
            member_name = node.name
            for decorator in node.decorator_list:
                decorator_text = dotted_name(decorator.func if isinstance(decorator, ast.Call) else decorator)
                if decorator_text.endswith(".setter"):
                    property_kind = "property_setter"
                    member_name = decorator_text.rsplit(".", 2)[-2]
                elif decorator_text.endswith(".deleter"):
                    property_kind = "property_deleter"
                    member_name = decorator_text.rsplit(".", 2)[-2]
                elif decorator_text.split(".")[-1] == "property":
                    property_kind = "property"
                elif decorator_text.split(".")[-1] == "classmethod":
                    property_kind = "classmethod"
                elif decorator_text.split(".")[-1] == "staticmethod":
                    property_kind = "staticmethod"
            member = Definition(
                module=module_info.module,
                name=member_name,
                node=node,
                path=module_info.path,
                source_sha256=module_info.source_sha256,
                line=node.lineno,
                end_line=getattr(node, "end_lineno", node.lineno),
                parent_class=cls.name,
                parent_class_line=cls.lineno,
                member_name=member_name,
                member_kind=property_kind,
            )
            output.append(member)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            for target in targets:
                if isinstance(target, ast.Name) and not target.id.startswith("_"):
                    # Public class attributes are retained as explicit model fields,
                    # but they are not callable method rows.
                    continue
    return output


class InventoryBuilder:
    def __init__(self, source: GitSource, cpp_root: Path, plan_file: Path, expected_operation_count: int = EXPECTED_OPERATION_COUNT) -> None:
        self.source = source
        self.cpp_root = cpp_root.resolve()
        self.project_root = self.cpp_root.parents[1]
        self.plan_file = plan_file.resolve()
        self.expected_operation_count = expected_operation_count
        self.modules: dict[str, ModuleInfo] = {}
        self.definitions: dict[str, Definition] = {}
        self.definition_ids_by_route: dict[str, list[str]] = defaultdict(list)
        self.alias_routes: dict[str, set[str]] = defaultdict(set)
        self.module_alias_routes: dict[str, set[str]] = defaultdict(set)
        self.issues: list[dict[str, str]] = []
        self.unknown_exports: list[dict[str, Any]] = []
        self.exported_values: list[dict[str, Any]] = []
        self.operation_bindings: list[dict[str, Any]] = []
        self.cpp_headers: dict[str, str] = {}
        self.native_runtime_source = "src/runtime/runtime.cpp"
        self.native_runtime_source_sha256 = "UNAVAILABLE"
        self.native_runtime_operation_lines: dict[str, int] = {}
        self.native_runtime_parse_status = "SOURCE_UNAVAILABLE"
        self.native_committed_head = "UNAVAILABLE"
        self.native_committed_runtime_source_sha256 = "UNAVAILABLE"
        self.native_committed_operation_ids: set[str] = set()
        self.rename_hints: dict[str, str] = {}
        self.rename_hints_sha256 = "UNAVAILABLE"

    def build(
        self,
        dispositions: Mapping[str, Any] | None = None,
    ) -> tuple[list[dict[str, str]], list[dict[str, str]], dict[str, Any]]:
        self._load_modules()
        self._collect_export_routes()
        self._collect_operations()
        self._load_cpp_hints()
        rows = (
            self._make_module_rows()
            + self._make_definition_rows()
            + self._make_public_value_rows()
            + self._make_unknown_export_rows()
        )
        rows += self._make_operation_rows()
        rows += self._make_consumer_rows()
        disposition_summary = apply_route_dispositions(
            rows,
            dispositions,
            source=self.source,
            project_root=self.project_root,
        )
        self._add_unresolved_for_rows(rows)
        self._add_source_issues()
        rows.sort(key=lambda row: (row["record_kind"], row["python_route"], row["canonical_id"], row["row_id"]))
        unresolved = sorted(
            self.issues,
            key=lambda row: (row["issue_code"], row["python_route"], row["capability_id"], row["issue_id"]),
        )
        summary = self._make_summary(rows, unresolved)
        if disposition_summary:
            summary["reviewed_dispositions"] = disposition_summary
        validate_inventory(rows, unresolved, self.source.commit, allow_reviewed_dispositions=bool(dispositions))
        return rows, unresolved, summary

    def _load_modules(self) -> None:
        for path in self.source.paths:
            module, package = module_name(path)
            content = self.source.read(path)
            file_sha = sha256(content)
            try:
                tree = ast.parse(content, filename=path)
            except SyntaxError as exc:
                raise ValueError(f"cannot parse pinned source {path}:{exc.lineno}: {exc.msg}") from exc
            info = ModuleInfo(
                module=module,
                path=path,
                content=content,
                source_sha256=file_sha,
                tree=tree,
                is_public=is_public_module(path),
                is_package=package,
            )
            self.modules[module] = info
        for info in self.modules.values():
            self._parse_module(info)

    def _parse_module(self, info: ModuleInfo) -> None:
        all_env: dict[str, list[str]] = {}
        for statement in info.tree.body:
            if isinstance(statement, (ast.FunctionDef, ast.AsyncFunctionDef)):
                definition = Definition(
                    module=info.module,
                    name=statement.name,
                    node=statement,
                    path=info.path,
                    source_sha256=info.source_sha256,
                    line=statement.lineno,
                    end_line=getattr(statement, "end_lineno", statement.lineno),
                )
                info.definitions[statement.name].append(definition)
                info.assignments.pop(statement.name, None)
                info.bindings[statement.name] = Binding("definition", target_name=definition.canonical_id, node=statement)
                self.definitions[definition.canonical_id] = definition
                self.definition_ids_by_route[definition.canonical_route].append(definition.canonical_id)
            elif isinstance(statement, ast.ClassDef):
                definition = Definition(
                    module=info.module,
                    name=statement.name,
                    node=statement,
                    path=info.path,
                    source_sha256=info.source_sha256,
                    line=statement.lineno,
                    end_line=getattr(statement, "end_lineno", statement.lineno),
                )
                info.definitions[statement.name].append(definition)
                info.assignments.pop(statement.name, None)
                info.classes[statement.name] = definition
                info.bindings[statement.name] = Binding("definition", target_name=definition.canonical_id, node=statement)
                self.definitions[definition.canonical_id] = definition
                self.definition_ids_by_route[definition.canonical_route].append(definition.canonical_id)
                for member in class_member_facts(info, statement):
                    self.definitions[member.canonical_id] = member
                    self.definition_ids_by_route[member.canonical_route].append(member.canonical_id)
            elif isinstance(statement, ast.ImportFrom):
                target_module = resolve_relative_module(info.module, info.is_package, statement.level, statement.module)
                if target_module in self.modules or target_module.startswith("fincore"):
                    for imported in statement.names:
                        if imported.name == "*":
                            info.import_star_targets.append(target_module)
                            continue
                        alias = imported.asname or imported.name
                        info.assignments.pop(alias, None)
                        submodule = f"{target_module}.{imported.name}" if target_module else imported.name
                        if submodule in self.modules:
                            info.bindings[alias] = Binding("module", target_module=submodule, source=ast_text(statement), node=statement)
                        else:
                            info.bindings[alias] = Binding(
                                "import",
                                target_module=target_module,
                                target_name=imported.name,
                                source=ast_text(statement),
                                node=statement,
                            )
            elif isinstance(statement, ast.Import):
                for imported in statement.names:
                    if imported.name == "fincore" or imported.name.startswith("fincore."):
                        alias = imported.asname or imported.name.split(".")[0]
                        target = imported.name if imported.asname else "fincore"
                        info.assignments.pop(alias, None)
                        info.bindings[alias] = Binding("module", target_module=target, source=ast_text(statement), node=statement)
            elif isinstance(statement, (ast.Assign, ast.AnnAssign)):
                targets = statement.targets if isinstance(statement, ast.Assign) else [statement.target]
                value = statement.value
                for target in targets:
                    if not isinstance(target, ast.Name):
                        continue
                    if target.id == "__all__":
                        names = literal_string_sequence(value, all_env)
                        info.exports = names
                        info.all_is_dynamic = names is None
                        if names is not None:
                            all_env["__all__"] = names
                    else:
                        static_value = literal_string_sequence(value, all_env)
                        if static_value is not None:
                            all_env[target.id] = static_value
                        else:
                            all_env.pop(target.id, None)
                    if target.id != "__all__" and isinstance(value, (ast.Name, ast.Attribute)):
                        info.assignments.pop(target.id, None)
                        info.bindings[target.id] = Binding("alias", target_name=dotted_name(value), source=ast_text(statement), node=statement)
                    elif target.id not in {"__all__", "__annotations__"} and not isinstance(value, (ast.Name, ast.Attribute)):
                        annotation = statement.annotation if isinstance(statement, ast.AnnAssign) else None
                        info.assignments[target.id] = (value, annotation, getattr(statement, "lineno", 1))
                        info.bindings.pop(target.id, None)
            elif isinstance(statement, ast.AugAssign) and isinstance(statement.target, ast.Name) and statement.target.id == "__all__":
                if isinstance(statement.op, ast.Add):
                    left = info.exports
                    right = literal_string_sequence(statement.value, all_env)
                    info.exports = left + right if left is not None and right is not None else None
                    info.all_is_dynamic = info.exports is None

    def _collect_export_routes(self) -> None:
        for info in self.modules.values():
            for target_module in info.import_star_targets:
                target = self.modules.get(target_module)
                if target is None:
                    self._issue(
                        f"star:{info.module}:{target_module}",
                        info.module,
                        "DYNAMIC_STAR_IMPORT",
                        self._anchor(info.path, 1),
                        f"star import from {target_module!r} could not be statically expanded",
                        "Resolve the exported names at the pinned source and add explicit alias rows.",
                    )
                    continue
                for name in self._exported_names(target):
                    if name not in info.bindings and name in target.bindings:
                        info.bindings[name] = Binding("import", target_module=target_module, target_name=name, source=f"from {target_module} import *")

        for info in self.modules.values():
            if not info.is_public:
                continue
            if info.all_is_dynamic:
                self._issue(
                    f"all:{info.module}",
                    info.module,
                    "DYNAMIC_ALL",
                    self._anchor(info.path, 1),
                    "__all__ is not a statically resolved string sequence.",
                    "Review runtime exports at the pinned source and preserve any additional public routes.",
                )
            for name in self._exported_names(info):
                binding = info.bindings.get(name)
                if binding is None:
                    if name in info.assignments:
                        value, annotation, line = info.assignments[name]
                        self.exported_values.append(
                            {
                                "module": info.module,
                                "name": name,
                                "path": info.path,
                                "line": line,
                                "expression": ast_text(value) if value is not None else "",
                                "annotation": ast_text(annotation),
                                "kind": self._public_value_kind(value, annotation),
                            }
                        )
                        continue
                    self.unknown_exports.append(
                        {
                            "module": info.module,
                            "name": name,
                            "path": info.path,
                            "line": self._binding_line(info, name),
                            "detail": f"Public export {name!r} has no statically resolved local definition or module.",
                        }
                    )
                    self._issue(
                        f"export:{info.module}:{name}",
                        f"{info.module}.{name}",
                        "UNKNOWN_PUBLIC_EXPORT",
                        self._anchor(info.path, self._binding_line(info, name)),
                        f"Public export {name!r} has no statically resolved local definition or module.",
                        "Resolve the imported/dynamic object and add its canonical source route or explicit scope decision.",
                    )
                    continue
                resolved = self._resolve_binding(info.module, name, set())
                route = f"{info.module}.{name}" if info.module else name
                if resolved[0] == "definition":
                    canonical_id = resolved[1]
                    self.alias_routes[canonical_id].add(route)
                elif resolved[0] == "module":
                    self.module_alias_routes[resolved[1]].add(route)
                else:
                    self.unknown_exports.append(
                        {
                            "module": info.module,
                            "name": name,
                            "path": info.path,
                            "line": self._binding_line(info, name),
                            "detail": resolved[1],
                        }
                    )
                    self._issue(
                        f"export:{info.module}:{name}",
                        route,
                        "UNKNOWN_PUBLIC_EXPORT",
                        self._anchor(info.path, self._binding_line(info, name)),
                        resolved[1],
                        "Resolve the exported object at the pinned source and preserve it as a capability row.",
                    )

        for canonical_id, class_routes in list(self.alias_routes.items()):
            definition = self.definitions.get(canonical_id)
            if definition is None or not isinstance(definition.node, ast.ClassDef):
                continue
            own_members: set[str] = set()
            for member in self.definitions.values():
                if member.module != definition.module or member.parent_class != definition.name or member.parent_class_line != definition.line:
                    continue
                suffix = self._member_route_suffix(member)
                own_members.add(suffix.split(".")[0])
                for class_route in class_routes:
                    self.alias_routes[member.canonical_id].add(f"{class_route}.{suffix}")
            for base in definition.node.bases:
                base_id, error = self._resolve_class_expression(definition.module, base)
                if base_id:
                    for member in self._inherited_members(base_id, set()):
                        suffix = self._member_route_suffix(member)
                        if suffix.split(".")[0] in own_members:
                            continue
                        for class_route in class_routes:
                            self.alias_routes[member.canonical_id].add(f"{class_route}.{suffix}")
                else:
                    base_name = dotted_name(base)
                    if base_name.split(".")[-1] not in {
                        "object", "Exception", "BaseException", "RuntimeError", "ValueError",
                        "TypeError", "Warning", "LookupError", "KeyError", "ImportError",
                        "OSError", "IndexError", "ArithmeticError", "AssertionError", "Protocol",
                        "Generic", "NamedTuple", "TypedDict",
                    }:
                        self._issue(
                            f"inherited:{definition.canonical_id}:{base_name}",
                            f"python:{definition.canonical_route}",
                            "INHERITED_METHOD_SURFACE_REVIEW",
                            self._anchor(definition.path, definition.line),
                            error or f"Public class inherits from unresolved base {base_name!r}.",
                            "Review the effective public method surface from the pinned source and record inherited routes required by consumers.",
                        )

    @staticmethod
    def _public_value_kind(value: ast.AST | None, annotation: ast.AST | None) -> str:
        annotation_name = dotted_name(annotation)
        if "TypeAlias" in annotation_name:
            return "type_alias"
        if isinstance(value, ast.Subscript):
            base_name = dotted_name(value.value).split(".")[-1]
            if base_name in {"Literal", "Union", "Optional", "Annotated", "ClassVar", "Final", "TypeAliasType"}:
                return "type_alias"
        if isinstance(value, ast.BinOp) and isinstance(value.op, ast.BitOr):
            return "type_alias"
        if isinstance(value, ast.Call):
            call_name = dotted_name(value.func).lower()
            return "generated_callable" if "operation" in call_name or "provider" in call_name else "dynamic_value"
        if value is None or isinstance(value, (ast.Constant, ast.List, ast.Tuple, ast.Dict, ast.Set)):
            return "constant"
        return "public_value"

    def _exported_names(self, info: ModuleInfo) -> list[str]:
        if info.exports is not None:
            names = list(info.exports)
            # Direct public module-level definitions remain visible as routes
            # even when an __all__ list restricts wildcard import behavior.
            names.extend(name for name in info.definitions if not name.startswith("_"))
        else:
            names = [name for name in info.bindings if not name.startswith("_")]
            names.extend(name for name in info.definitions if not name.startswith("_"))
        return sorted(set(names))

    def _resolve_binding(self, module: str, name: str, seen: set[tuple[str, str]]) -> tuple[str, str]:
        key = (module, name)
        if key in seen:
            return ("unknown", f"cyclic local alias for {module}.{name}")
        seen.add(key)
        info = self.modules.get(module)
        if info is None:
            return ("unknown", f"source module {module!r} is not present in the pinned Python package")
        binding = info.bindings.get(name)
        if binding is None:
            if name in info.classes:
                ids = self.definition_ids_by_route.get(f"{module}.{name}", [])
                return ("definition", ids[-1]) if ids else ("unknown", f"class source for {module}.{name} is unavailable")
            return ("unknown", f"no local binding resolves {module}.{name}")
        if binding.kind == "definition":
            return ("definition", binding.target_name)
        if binding.kind == "module":
            return ("module", binding.target_module)
        if binding.kind == "import":
            target = self.modules.get(binding.target_module)
            if target is None:
                return ("unknown", f"{module}.{name} imports {binding.target_module}.{binding.target_name} outside the pinned package")
            return self._resolve_binding(binding.target_module, binding.target_name, seen)
        if binding.kind == "alias":
            target = binding.target_name
            pieces = target.split(".")
            if len(pieces) == 1:
                return self._resolve_binding(module, target, seen)
            # Resolve the longest local module prefix, then a symbol chain.
            for cut in range(len(pieces), 0, -1):
                candidate_module = ".".join(pieces[:cut])
                if candidate_module in self.modules:
                    if cut == len(pieces):
                        return ("module", candidate_module)
                    return self._resolve_binding(candidate_module, pieces[cut], seen)
            if pieces[0] in info.bindings:
                base = self._resolve_binding(module, pieces[0], seen)
                if base[0] == "definition":
                    parent = self.definitions.get(base[1])
                    if parent is not None and isinstance(parent.node, ast.ClassDef):
                        member = pieces[-1]
                        child = f"{parent.module}.{parent.name}.{member}"
                        ids = self.definition_ids_by_route.get(child, [])
                        if ids:
                            return ("definition", ids[-1])
            return ("unknown", f"local alias {module}.{name} -> {target} could not be resolved")
        return ("unknown", f"unsupported local binding kind {binding.kind!r} for {module}.{name}")

    def _resolve_class_expression(self, module: str, expression: ast.AST) -> tuple[str, str]:
        parts = dotted_name(expression).split(".")
        if not parts or not parts[0]:
            return "", f"class base expression {ast.dump(expression, include_attributes=False)} is dynamic or generic"
        result = self._resolve_binding(module, parts[0], set())
        if result[0] == "module" and len(parts) > 1:
            result = self._resolve_binding(result[1], parts[1], set())
        if result[0] != "definition":
            return "", result[1]
        definition = self.definitions.get(result[1])
        if definition is None or not isinstance(definition.node, ast.ClassDef):
            return "", f"base expression {ast_text(expression)!r} does not resolve to a local class"
        return result[1], ""

    def _inherited_members(self, class_id: str, seen: set[str]) -> list[Definition]:
        if class_id in seen:
            return []
        seen.add(class_id)
        definition = self.definitions.get(class_id)
        if definition is None or not isinstance(definition.node, ast.ClassDef):
            return []
        members = [
            item for item in self.definitions.values()
            if item.module == definition.module and item.parent_class == definition.name and item.parent_class_line == definition.line
        ]
        names = {item.member_name or item.name for item in members}
        for base in definition.node.bases:
            base_id, _ = self._resolve_class_expression(definition.module, base)
            if base_id:
                members.extend(
                    item for item in self._inherited_members(base_id, seen)
                    if (item.member_name or item.name) not in names
                )
                names.update(item.member_name or item.name for item in members)
        return members

    @staticmethod
    def _member_route_suffix(definition: Definition) -> str:
        suffix = definition.member_name or definition.name
        if definition.member_kind == "property_setter":
            suffix += ".setter"
        elif definition.member_kind == "property_deleter":
            suffix += ".deleter"
        return suffix

    def _binding_line(self, info: ModuleInfo, name: str) -> int:
        binding = info.bindings.get(name)
        if binding and binding.node is not None:
            return getattr(binding.node, "lineno", 1)
        return info.definitions[name][-1].line if name in info.definitions else 1

    def _collect_operations(self) -> None:
        runtime = self.modules.get("fincore.runtime.builtins")
        if runtime is None:
            self._issue("catalog-root", "fincore.runtime.builtins", "OPERATION_CATALOG_ROOT_MISSING", "", "builtin catalog composition root is missing", "Inspect the pinned runtime catalog composition and inventory its operation IDs.")
            return
        provider_paths: list[str] | None = None
        for node in runtime.tree.body:
            if isinstance(node, (ast.Assign, ast.AnnAssign)):
                targets = node.targets if isinstance(node, ast.Assign) else [node.target]
                if any(isinstance(target, ast.Name) and target.id == "_BUILTIN_PROVIDER_PATHS" for target in targets):
                    provider_paths = literal_string_sequence(node.value)
                    break
        if provider_paths is None:
            self._issue("catalog-provider-paths", runtime.module, "DYNAMIC_OPERATION_PROVIDERS", self._anchor(runtime.path, 1), "Builtin provider paths are not a static string sequence.", "Resolve the runtime provider list and inventory every provider in catalog order.")
            return
        for provider_path in provider_paths:
            module, separator, function_name = provider_path.partition(":")
            info = self.modules.get(module)
            if not separator or not info or function_name != "operations":
                self._issue(
                    f"provider:{provider_path}",
                    provider_path,
                    "UNKNOWN_OPERATION_PROVIDER",
                    self._anchor(runtime.path, 1),
                    f"Catalog provider {provider_path!r} is malformed or unavailable in the pinned source.",
                    "Resolve this provider and enumerate its exact operation IDs.",
                )
                continue
            binding_node = next(
                (
                    node
                    for node in info.tree.body
                    if isinstance(node, (ast.Assign, ast.AnnAssign))
                    and any(isinstance(target, ast.Name) and target.id == "_BINDINGS" for target in (node.targets if isinstance(node, ast.Assign) else [node.target]))
                ),
                None,
            )
            if binding_node is None:
                if module == "fincore.extensions.operations":
                    continue
                self._issue(
                    f"bindings:{module}",
                    module,
                    "DYNAMIC_OPERATION_BINDINGS",
                    self._anchor(info.path, 1),
                    "Provider does not expose a static _BINDINGS tuple.",
                    "Evaluate the provider against the pinned source and add explicit ID-to-callable bindings.",
                )
                continue
            value = binding_node.value
            if not isinstance(value, (ast.Tuple, ast.List)):
                self._issue(
                    f"bindings:{module}",
                    module,
                    "DYNAMIC_OPERATION_BINDINGS",
                    self._anchor(info.path, getattr(binding_node, "lineno", 1)),
                    "Provider _BINDINGS is not a literal tuple/list.",
                    "Resolve the provider sequence without omitting generated operation IDs.",
                )
                continue
            for entry in value.elts:
                if not isinstance(entry, (ast.Tuple, ast.List)) or len(entry.elts) < 2:
                    self._issue(
                        f"binding:{module}:{getattr(entry, 'lineno', 1)}",
                        module,
                        "MALFORMED_OPERATION_BINDING",
                        self._anchor(info.path, getattr(entry, "lineno", 1)),
                        "_BINDINGS entry is not a tuple/list with an ID and callable expression.",
                        "Resolve the provider binding shape and preserve the catalog ID.",
                    )
                    continue
                operation_id = entry.elts[0].value if isinstance(entry.elts[0], ast.Constant) and isinstance(entry.elts[0].value, str) else None
                if operation_id is None:
                    self._issue(
                        f"binding:{module}:{getattr(entry, 'lineno', 1)}",
                        module,
                        "DYNAMIC_OPERATION_ID",
                        self._anchor(info.path, getattr(entry, "lineno", 1)),
                        "Catalog operation ID is not a string literal.",
                        "Record the exact runtime ID at the pinned source and explain its construction.",
                    )
                    continue
                expression = entry.elts[1]
                resolved = self._resolve_operation_expression(module, expression)
                self.operation_bindings.append(
                    {
                        "operation_id": operation_id,
                        "provider_module": module,
                        "provider_path": provider_path,
                        "source_file": info.path,
                        "source_line": entry.lineno,
                        "source_sha256": info.source_sha256,
                        "callable_expression": ast_text(expression),
                        "canonical_id": resolved[1] if resolved[0] == "definition" else "UNKNOWN",
                        "binding_metadata": [ast_text(item) for item in entry.elts[2:]],
                        "resolve_error": resolved[1] if resolved[0] != "definition" else "",
                    }
                )
                if resolved[0] != "definition":
                    self._issue(
                        f"op:{operation_id}",
                        operation_id,
                        "UNRESOLVED_OPERATION_CALLABLE",
                        self._anchor(info.path, entry.lineno),
                        resolved[1],
                        "Resolve the operation's exact implementation and preserve its canonical implementation mapping.",
                    )
                else:
                    self.alias_routes[resolved[1]].add(operation_id)
        counts = Counter(binding["operation_id"] for binding in self.operation_bindings)
        for operation_id, count in sorted(counts.items()):
            if count > 1:
                self._issue(
                    f"op-duplicate:{operation_id}",
                    operation_id,
                    "DUPLICATE_OPERATION_ID",
                    "",
                    f"Catalog operation ID occurs {count} times across provider bindings.",
                    "Resolve the duplicate provider declaration before treating the catalog as complete.",
                )
        if len(self.operation_bindings) != self.expected_operation_count:
            self._issue(
                "op-count",
                "operation-catalog",
                "OPERATION_COUNT_MISMATCH",
                self._anchor(runtime.path, 1),
                f"Static provider bindings contain {len(self.operation_bindings)} IDs; expected {self.expected_operation_count} at the pinned source.",
                "Compare the static binding extraction with the pinned runtime catalog and update the expected count only through review.",
            )

    def _resolve_operation_expression(self, provider_module: str, expression: ast.AST) -> tuple[str, str]:
        value = dotted_name(expression)
        if not value:
            return ("unknown", f"unsupported callable expression {ast.dump(expression, include_attributes=False)}")
        pieces = value.split(".")
        info = self.modules.get(provider_module)
        if not info:
            return ("unknown", f"provider module {provider_module!r} is not loaded")
        base_result = self._resolve_binding(provider_module, pieces[0], set())
        if base_result[0] == "definition":
            current_id = base_result[1]
            if len(pieces) == 1:
                return base_result
            parent = self.definitions.get(current_id)
            if parent and isinstance(parent.node, ast.ClassDef):
                route = f"{parent.module}.{parent.name}.{pieces[-1]}"
                ids = self.definition_ids_by_route.get(route, [])
                if ids:
                    return ("definition", ids[-1])
            return ("unknown", f"callable expression {value!r} could not be resolved to a class method")
        if base_result[0] == "module":
            target_module = base_result[1]
            if len(pieces) == 1:
                return base_result
            target_symbol = pieces[1]
            resolved = self._resolve_binding(target_module, target_symbol, set())
            if len(pieces) == 2 or resolved[0] != "definition":
                return resolved
            parent = self.definitions.get(resolved[1])
            if parent and isinstance(parent.node, ast.ClassDef):
                ids = self.definition_ids_by_route.get(f"{parent.module}.{parent.name}.{pieces[2]}", [])
                if ids:
                    return ("definition", ids[-1])
            return ("unknown", f"callable expression {value!r} has an unresolved method segment")
        return base_result

    def _load_cpp_hints(self) -> None:
        tool_path = Path(__file__).resolve().parent / "parity_ledger.py"
        if tool_path.exists():
            self.rename_hints_sha256 = sha256(tool_path.read_bytes())
            try:
                tree = ast.parse(tool_path.read_text(encoding="utf-8"))
                for node in tree.body:
                    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "RENAMES" for t in node.targets):
                        value = ast.literal_eval(node.value)
                        if isinstance(value, dict):
                            self.rename_hints = {str(key): str(item) for key, item in value.items()}
            except (OSError, SyntaxError, ValueError):
                self.rename_hints = {}
        if not self.cpp_root.exists():
            pass
        else:
            for path in sorted(self.cpp_root.rglob("*.hpp")):
                try:
                    self.cpp_headers[path.relative_to(self.cpp_root).as_posix()] = path.read_text(encoding="utf-8")
                except UnicodeDecodeError:
                    continue
        runtime_path = self.project_root / self.native_runtime_source
        if runtime_path.is_file():
            content = runtime_path.read_bytes()
            self.native_runtime_source_sha256 = sha256(content)
            text = content.decode("utf-8")
            self.native_runtime_operation_lines = self._parse_runtime_catalog(text)
            self.native_runtime_parse_status = "STATIC_ADD_CALLS_EXTRACTED"
        else:
            self.native_runtime_parse_status = "SOURCE_UNAVAILABLE"
        try:
            head = subprocess.run(
                ["git", "-C", str(self.project_root), "rev-parse", "HEAD"],
                check=True,
                capture_output=True,
                text=True,
            ).stdout.strip()
            committed = subprocess.run(
                ["git", "-C", str(self.project_root), "show", f"{head}:{self.native_runtime_source}"],
                check=True,
                capture_output=True,
                text=True,
            ).stdout
            self.native_committed_head = head
            self.native_committed_runtime_source_sha256 = sha256(committed.encode("utf-8"))
            self.native_committed_operation_ids = set(self._parse_runtime_catalog(committed))
        except (OSError, subprocess.CalledProcessError):
            self.native_committed_head = "COMMITTED_SOURCE_UNAVAILABLE"

    @staticmethod
    def _parse_runtime_catalog(text: str) -> dict[str, int]:
        registrations = list(re.finditer(r'^\s*add\("([^"\n]+)"', text, flags=re.MULTILINE))
        operation_lines: dict[str, int] = {}
        for match in registrations:
            operation_lines.setdefault(
                match.group(1), text.count("\n", 0, match.start()) + 1
            )
        return operation_lines

    def _domain(self, module: str) -> str:
        parts = module.split(".")
        return parts[1] if len(parts) > 1 and parts[0] == "fincore" else ""

    def _native_candidates(self, python_route: str, kind: str) -> tuple[list[str], list[str]]:
        parts = python_route.split(".")
        domain = parts[1] if len(parts) > 1 and parts[0] == "fincore" else (parts[0] if parts and parts[0] in CPP_FAMILY else "")
        family = CPP_FAMILY.get(domain, domain)
        token = parts[-1].split("@")[0].split("/")[0]
        names = [token]
        renamed = self.rename_hints.get(token)
        if renamed:
            names.append(renamed)
        if kind in {"class", "dataclass", "module"}:
            pattern_factory = lambda name: re.compile(r"\b(?:class|struct|using|enum\s+class)\s+" + re.escape(name) + r"\b")
        else:
            pattern_factory = lambda name: re.compile(r"(?<![\w:])" + re.escape(name) + r"\s*\(")
        matches: list[str] = []
        for header, text in self.cpp_headers.items():
            if not header.startswith(family + "/") and header != f"{family}.hpp":
                continue
            for name in names:
                if pattern_factory(name).search(text):
                    matches.append(f"include/fincore/{header}#{name}")
        return sorted(set(matches)), names

    def _module_native_candidates(self, module: str) -> list[str]:
        parts = module.split(".")
        if len(parts) < 2 or parts[0] != "fincore":
            return []
        domain = parts[1]
        family = CPP_FAMILY.get(domain, domain)
        suffix = parts[2:]
        if not suffix:
            candidates = [
                header for header in self.cpp_headers
                if header.startswith(family + "/") or header == f"{family}.hpp"
            ]
            return sorted(f"include/fincore/{header}#namespace candidate fincore::{family}" for header in candidates)
        expected = f"{family}/{'/'.join(suffix)}.hpp"
        if expected in self.cpp_headers:
            return [f"include/fincore/{expected}#module candidate fincore::{family}::{ '::'.join(suffix) }" ]
        return []

    def _phase_for(self, module: str) -> str:
        domain = self._domain(module)
        return PUBLIC_DOMAIN_PHASES.get(domain, "UNRESOLVED")

    def _acceptance_refs(self, module: str) -> list[dict[str, str]]:
        domain = self._domain(module)
        references: list[dict[str, str]] = []
        for relative in ACCEPTANCE_HINTS.get(domain, []):
            path = self.project_root / relative
            if path.exists():
                references.append({"path": relative, "sha256": sha256(path.read_bytes()), "role": "mapping_hint_only"})
        return references

    def _source_href(self, path: str, line: int) -> str:
        return f"https://github.com/cloudQuant/fincore/blob/{self.source.commit}/{path}#L{line}"

    def _anchor(self, path: str, line: int) -> str:
        return f"{path}:L{line} ({self._source_href(path, line)})"

    def _scope_decision(self, disposition: str) -> str:
        return f"2026-10-23: automated inventory and mapping hint only; {disposition} requires primary review before acceptance."

    def _make_module_rows(self) -> list[dict[str, str]]:
        rows: list[dict[str, str]] = []
        for info in self.modules.values():
            if not info.is_public:
                continue
            line = 1
            name = info.module.split(".")[-1] if info.module else "fincore"
            candidates = self._module_native_candidates(info.module)
            domain = self._domain(info.module)
            phase = self._phase_for(info.module)
            disposition = "PLANNED" if candidates else "GAP"
            module_aliases = sorted(self.module_alias_routes.get(info.module, set()))
            contract = {
                "module_docstring": ast.get_docstring(info.tree, clean=False),
                "module_exports": self._exported_names(info),
                "dynamic_all": info.all_is_dynamic,
            }
            rows.append(self._row(
                record_kind="module",
                capability_id=f"python-module:{info.module}",
                canonical_id=f"module:{info.module}",
                python_route=info.module,
                kind="module",
                source_file=info.path,
                source_anchor=self._anchor(info.path, line),
                source_sha256=info.source_sha256,
                aliases=module_aliases,
                contract=contract,
                native_candidates=candidates,
                owner_phase=phase,
                disposition=disposition,
                domain=domain,
                source_line=line,
            ))
        return rows

    def _make_definition_rows(self) -> list[dict[str, str]]:
        rows: list[dict[str, str]] = []
        included_ids = sorted(self.alias_routes)
        for canonical_id in included_ids:
            definition = self.definitions.get(canonical_id)
            if definition is None:
                continue
            kind = definition.entity_kind
            if isinstance(definition.node, ast.ClassDef):
                contract = self._class_contract(definition)
            else:
                contract = function_contract(definition.node)
            aliases = sorted(self.alias_routes[canonical_id])
            if definition.parent_class:
                aliases = self._method_aliases(definition, aliases)
            primary_route = definition.canonical_route
            # Keep the defining route even when only a public import alias exposed it.
            aliases = sorted(set(aliases) | {primary_route})
            candidates, _ = self._native_candidates(primary_route, kind)
            domain = self._domain(definition.module)
            phase = self._phase_for(definition.module)
            disposition = "PLANNED" if candidates else "GAP"
            row = self._row(
                record_kind="capability",
                capability_id=f"python:{primary_route}",
                canonical_id=canonical_id,
                python_route=primary_route,
                kind=kind,
                source_file=definition.path,
                source_anchor=self._anchor(definition.path, definition.line),
                source_sha256=definition.source_sha256,
                aliases=aliases,
                contract=contract,
                native_candidates=candidates,
                owner_phase=phase,
                disposition=disposition,
                domain=domain,
                source_line=definition.line,
            )
            rows.append(row)
            if isinstance(definition.node, (ast.FunctionDef, ast.AsyncFunctionDef)) and not ast.get_docstring(definition.node, clean=False):
                self._issue(
                    f"docstring:{canonical_id}",
                    row["capability_id"],
                    "SOURCE_CONTRACT_DOCSTRING_MISSING",
                    row["source_anchor"],
                    "Source callable has no docstring to provide the behavioral contract text requested by Phase 39.",
                    "Review its implementation, record inputs/shape/errors/missing policy from source, and add an explicit contract note.",
                )
            if phase == "UNRESOLVED":
                self._issue(
                    f"phase:{canonical_id}",
                    row["capability_id"],
                    "OWNER_PHASE_UNRESOLVED",
                    row["source_anchor"],
                    f"No owner-phase hint is registered for {definition.module!r}.",
                    "Assign an owner phase or an explicit dated scope decision after primary review.",
                )
        return rows

    def _class_contract(self, definition: Definition) -> dict[str, Any]:
        assert isinstance(definition.node, ast.ClassDef)
        cls = definition.node
        init = next((node for node in cls.body if isinstance(node, ast.FunctionDef) and node.name == "__init__"), None)
        dataclass_fields: list[dict[str, Any]] = []
        for node in cls.body:
            if isinstance(node, ast.AnnAssign) and isinstance(node.target, ast.Name) and not node.target.id.startswith("_"):
                dataclass_fields.append(
                    {
                        "name": node.target.id,
                        "annotation": ast_text(node.annotation),
                        "default": ast_text(node.value) if node.value is not None else None,
                    }
                )
        return {
            "constructor_signature": source_signature(init) if init is not None else "NOT_DECLARED_IN_SOURCE",
            "dataclass_or_model_fields": dataclass_fields,
            "bases": [ast_text(base) for base in cls.bases],
            "source_docstring": ast.get_docstring(cls, clean=False),
            "shape_order_index_errors_missing_policy": (
                "See source_docstring, field declarations and source_anchor; no semantic claims are inferred by this inventory."
            ),
        }

    def _make_public_value_rows(self) -> list[dict[str, str]]:
        rows: list[dict[str, str]] = []
        for item in self.exported_values:
            route = f"{item['module']}.{item['name']}"
            canonical_id = f"value:{route}@{item['line']}"
            info = self.modules[item["module"]]
            rows.append(self._row(
                record_kind="public_value",
                capability_id=f"python-value:{route}",
                canonical_id=canonical_id,
                python_route=route,
                kind=item["kind"],
                source_file=item["path"],
                source_anchor=self._anchor(item["path"], item["line"]),
                source_sha256=info.source_sha256,
                aliases=[route],
                contract={
                    "source_expression": item["expression"],
                    "source_annotation": item["annotation"],
                    "source_docstring": ast.get_docstring(info.tree, clean=False),
                    "contract_review": "Public value/type alias included from __all__; semantic mapping and native scope require review.",
                },
                native_candidates=[],
                owner_phase=self._phase_for(item["module"]),
                disposition="GAP",
                domain=self._domain(item["module"]),
                source_line=item["line"],
            ))
        return rows

    def _make_unknown_export_rows(self) -> list[dict[str, str]]:
        rows: list[dict[str, str]] = []
        for item in self.unknown_exports:
            route = f"{item['module']}.{item['name']}"
            canonical_id = f"unknown-export:{route}@{item['line']}"
            info = self.modules[item["module"]]
            rows.append(self._row(
                record_kind="unknown_export",
                capability_id=f"python-export:{route}",
                canonical_id=canonical_id,
                python_route=route,
                kind="unknown_export",
                source_file=item["path"],
                source_anchor=self._anchor(item["path"], item["line"]),
                source_sha256=info.source_sha256,
                aliases=[route],
                contract={
                    "resolution_status": "UNRESOLVED",
                    "source_expression": item["detail"],
                    "contract_review": "Resolve this exported object before considering the inventory complete.",
                },
                native_candidates=[],
                owner_phase=self._phase_for(item["module"]),
                disposition="GAP",
                domain=self._domain(item["module"]),
                source_line=item["line"],
            ))
        return rows

    def _method_aliases(self, definition: Definition, aliases: list[str]) -> list[str]:
        # A class import alias also exposes the same declared method/property.
        output = list(aliases)
        class_routes = self.alias_routes.get(self._class_id_for(definition), set())
        for class_route in class_routes:
            suffix = definition.member_name or definition.name
            if definition.member_kind == "property_setter":
                suffix += ".setter"
            elif definition.member_kind == "property_deleter":
                suffix += ".deleter"
            output.append(f"{class_route}.{suffix}")
        return output

    def _class_id_for(self, definition: Definition) -> str:
        assert definition.parent_class
        ids = self.definition_ids_by_route.get(f"{definition.module}.{definition.parent_class}", [])
        return ids[-1] if ids else ""

    def _make_operation_rows(self) -> list[dict[str, str]]:
        rows: list[dict[str, str]] = []
        for binding in self.operation_bindings:
            operation_id = binding["operation_id"]
            canonical_id = binding["canonical_id"]
            route = f"operation:{operation_id}"
            definition = self.definitions.get(canonical_id)
            source_path = binding["source_file"]
            line = int(binding["source_line"])
            domain = binding["provider_module"].split(".")[1] if len(binding["provider_module"].split(".")) > 1 else ""
            # Operation IDs have domain segments; use the linked implementation route when resolved.
            operation_leaf = operation_id.rsplit(".", 1)[-1]
            candidate_route = definition.canonical_route if definition else f"fincore.{domain}.{operation_leaf}"
            candidates, _ = self._native_candidates(candidate_route, "callable")
            disposition = "PLANNED" if candidates else "GAP"
            row = self._row(
                record_kind="operation_binding",
                capability_id=f"operation:{operation_id}",
                canonical_id=canonical_id if canonical_id != "UNKNOWN" else f"unknown-operation:{operation_id}",
                python_route=operation_id,
                kind="operation_binding",
                source_file=source_path,
                source_anchor=self._anchor(source_path, line),
                source_sha256=binding["source_sha256"],
                aliases=[operation_id],
                contract={
                    "operation_id": operation_id,
                    "provider": binding["provider_path"],
                    "callable_expression": binding["callable_expression"],
                    "binding_metadata": binding["binding_metadata"],
                    "canonical_implementation": canonical_id,
                    "resolution_error": binding["resolve_error"],
                    "native_runtime_registration": self._native_runtime_registration(operation_id),
                },
                native_candidates=candidates,
                owner_phase=self._phase_for(f"fincore.{domain}"),
                disposition=disposition,
                domain=domain,
                source_line=line,
            )
            rows.append(row)
            if canonical_id == "UNKNOWN":
                self._issue(
                    f"op-unresolved:{operation_id}",
                    row["capability_id"],
                    "UNRESOLVED_OPERATION_CALLABLE",
                    row["source_anchor"],
                    binding["resolve_error"],
                    "Resolve this exact operation ID to one canonical implementation.",
                )
        return rows

    def _native_runtime_registration(self, operation_id: str) -> dict[str, Any]:
        line = self.native_runtime_operation_lines.get(operation_id)
        return {
            "status": "EXACT_ID_REGISTERED_IN_WORKING_TREE_SOURCE" if line is not None else "NOT_REGISTERED_IN_WORKING_TREE_SOURCE",
            "source_file": self.native_runtime_source,
            "source_sha256": self.native_runtime_source_sha256,
            "line": line,
            "evidence_limit": "Working-tree source inventory only; no test or acceptance is asserted.",
        }

    @staticmethod
    def _operation_scope_rationale(domain: str) -> tuple[str, str]:
        rationales = {
            "metrics": (
                "PREEXISTING_NATIVE_FAMILY_RUNTIME_REGISTRATION_PENDING",
                "Phase 92 acceptance classifies unregistered metrics operations as aligned/pair/typed-keyword variants or multi-output forms; native kernels exist for most, while runtime registration is narrower. This is a mapping rationale, not per-ID parity proof.",
            ),
            "risk": (
                "SELECTED_OR_HISTORICAL_TYPED_RUNTIME_CONTRACT_PENDING",
                "Phase 92 records typed risk/model/report inputs as requiring runtime contracts. Only exact IDs found in the working-tree catalog are registered; remaining IDs need source-to-native contract review.",
            ),
            "factor_analysis": (
                "PREEXISTING_NATIVE_FAMILY_TYPED_INPUT_PENDING",
                "Phase 92 classifies prepared factor-ledger inputs as not expressible in the current runtime_value variant; omission is a runtime input-contract boundary, not proof that the domain lacks native kernels.",
            ),
            "portfolio": (
                "PREEXISTING_NATIVE_FAMILY_TYPED_INPUT_PENDING",
                "Phase 92 classifies portfolio outputs as typed tables/windows and optional cash-column inputs pending runtime contracts.",
            ),
            "performance": (
                "PREEXISTING_NATIVE_FAMILY_TYPED_INPUT_PENDING",
                "Phase 92 classifies cashflow/inference runtime contracts as pending; direct kernel or domain acceptance does not establish operation-catalog registration.",
            ),
            "simulation": (
                "PREEXISTING_NATIVE_FAMILY_MODEL_INPUT_PENDING",
                "Phase 92 classifies composer/parameter objects as outside the current serializable runtime values.",
            ),
            "attribution": (
                "PREEXISTING_NATIVE_FAMILY_TYPED_INPUT_PENDING",
                "Phase 92 classifies typed performance-attribution spans/columns as pending runtime inputs.",
            ),
            "data": (
                "EXCLUDED_DOMAIN_MAPPING_HINT",
                "Phase 92 explicitly places provider/data utilities outside native runtime scope; retain these source IDs as visible excluded-domain rows.",
            ),
            "report": (
                "PREEXISTING_NATIVE_FAMILY_TYPED_INPUT_PENDING",
                "Phase 92 says report documents are supported as a runtime value but builders take domain models. Exact registration must be checked per source ID.",
            ),
            "extensions": (
                "EMPTY_SOURCE_PROVIDER_AT_PIN",
                "The pinned Phase 92 source catalog classifies extensions as an empty provider; no source operation IDs should be missing for this domain.",
            ),
        }
        return rationales.get(
            domain,
            (
                "SCOPE_REVIEW_REQUIRED",
                "No domain-specific operation-catalog rationale is registered; primary review is required.",
            ),
        )

    def _operation_reconciliation_rows(self) -> list[dict[str, str]]:
        rows: list[dict[str, str]] = []
        for binding in sorted(self.operation_bindings, key=lambda item: item["operation_id"]):
            operation_id = binding["operation_id"]
            domain = binding["provider_module"].split(".")[1] if len(binding["provider_module"].split(".")) > 1 else ""
            line = self.native_runtime_operation_lines.get(operation_id)
            if line is not None:
                if operation_id in self.native_committed_operation_ids:
                    classification = "COMMITTED_CATALOG_ID_ACCEPTANCE_STATUS_UNVERIFIED"
                    rationale = "Exact source operation ID occurs in committed HEAD and current builtin_catalog source. The Phase 92 record asserts 44 catalog operations but does not provide per-ID acceptance mapping here; no row is accepted by this inventory."
                else:
                    classification = "CURRENT_WORKING_TREE_ADDITION_UNVERIFIED"
                    rationale = "Exact source operation ID occurs only in the current working-tree builtin_catalog source, after committed HEAD. It is not covered by the committed 44-operation baseline and has no row-level acceptance evidence here."
            else:
                classification, rationale = self._operation_scope_rationale(domain)
            rows.append({
                "operation_id": operation_id,
                "provider_module": binding["provider_module"],
                "provider_source_file": binding["source_file"],
                "provider_source_anchor": self._anchor(binding["source_file"], int(binding["source_line"])),
                "provider_source_sha256": binding["source_sha256"],
                "canonical_implementation": binding["canonical_id"],
                "native_registration_status": "EXACT_ID_REGISTERED" if line is not None else "NOT_REGISTERED",
                "native_registration_source": f"{self.native_runtime_source}#{operation_id}" if line is not None else "UNREGISTERED",
                "native_registration_line": str(line) if line is not None else "UNREGISTERED",
                "scope_classification": classification,
                "scope_rationale": rationale,
            })
        return rows

    def _capability_scope_classification(self, row: dict[str, str]) -> tuple[str, str]:
        source_file = row["source_file"]
        if source_file.startswith(("fincore/data/", "fincore/extensions/")):
            return (
                "EXCLUDED_DOMAIN_MAPPING_HINT",
                "The historical parity audit classifies data providers or plugin loading as outside native scope; retain the route visibly and require primary review before assigning a final disposition.",
            )
        selected_files = {
            "fincore/factor_analysis/analysis.py": "Phase 79 selected analysis/model projection scope",
            "fincore/factor_analysis/performance.py": "Phase 80 selected grouped/performance scope",
            "fincore/factor_analysis/research.py": "Phase 83 selected research-evaluation scope",
            "fincore/risk/diagnostics.py": "Phase 92 selected typed risk/runtime integration scope",
            "fincore/risk/evt.py": "Phase 92 selected typed risk-model/runtime integration scope",
            "fincore/risk/garch.py": "Phase 92 selected typed risk-model/runtime integration scope",
            "fincore/risk/models.py": "Phase 92 selected typed risk-model/runtime integration scope",
            "fincore/risk/report.py": "Phase 92 selected source risk-report/runtime integration scope",
            "fincore/risk/specs.py": "Phase 92 selected typed risk-model/runtime integration scope",
            "fincore/report/models.py": "Phase 92 selected owning report-model/runtime integration scope",
            "fincore/report/portfolio.py": "Phase 92 selected portfolio-report/runtime integration scope",
            "fincore/report/risk.py": "Phase 92 selected risk-report/runtime integration scope",
            "fincore/runtime/runtime.py": "Phase 92 selected runtime catalog/engine/session scope",
            "fincore/portfolio/transactions.py": "Phase 92 selected portfolio transaction-input integration scope",
        }
        if source_file in selected_files or source_file.startswith("fincore/runtime/"):
            rationale = selected_files.get(source_file, "Phase 92 selected runtime catalog/engine/session scope")
            return (
                "EXPLICIT_SELECTED_PLAN_SCOPE_UNMATCHED",
                f"{rationale}; module-level scope is broader than a per-route phase selection, and this inventory row remains unverified pending primary review.",
            )
        return (
            "PREEXISTING_HISTORICAL_SCOPE_MAPPING_HINT",
            "The route is outside the explicit revised-plan source slices above; historical acceptance or legacy name mapping is a discovery hint only, not row-level acceptance evidence.",
        )

    def _capability_scope_rows(self, rows: Sequence[dict[str, str]]) -> list[dict[str, str]]:
        output: list[dict[str, str]] = []
        for row in rows:
            if row["record_kind"] != "capability" or row["disposition"] != "GAP":
                continue
            category, rationale = self._capability_scope_classification(row)
            output.append({
                "python_route": row["python_route"],
                "canonical_id": row["canonical_id"],
                "source_file": row["source_file"],
                "scope_classification": category,
                "scope_rationale": rationale,
                "verification_status": "MAPPING_HINT_ONLY_PRIMARY_REVIEW_REQUIRED",
            })
        return sorted(output, key=lambda item: (item["scope_classification"], item["source_file"], item["python_route"]))

    def _make_consumer_rows(self) -> list[dict[str, str]]:
        if not self.plan_file.exists():
            self._issue(
                "plan-missing",
                "revised-plan-consumers",
                "REVISED_PLAN_MISSING",
                "",
                f"Plan file {self.plan_file} is missing.",
                "Provide the revised plan source and regenerate selected consumer scenarios.",
            )
            return []
        raw = self.plan_file.read_text(encoding="utf-8")
        lines = raw.splitlines()
        try:
            start = next(index for index, line in enumerate(lines) if line.strip() == "## Bounded implementation slices and exit criteria") + 1
            end = next(index for index in range(start, len(lines)) if lines[index].startswith("## "))
        except (StopIteration, ValueError):
            self._issue(
                "plan-scenarios",
                "revised-plan-consumers",
                "REVISED_PLAN_SCENARIOS_UNRESOLVED",
                "",
                "Could not locate the bounded implementation slices section.",
                "Review the plan structure and explicitly select scenario rows before claiming completeness.",
            )
            return []
        items: list[tuple[int, str]] = []
        current_line: int | None = None
        current_text: list[str] = []
        for index in range(start, end):
            line = lines[index]
            match = re.match(r"^(\d+)\.\s+(.*)$", line)
            if match:
                if current_line is not None:
                    items.append((current_line, " ".join(part.strip() for part in current_text).strip()))
                current_line = index + 1
                current_text = [match.group(2)]
            elif current_line is not None and line.strip():
                current_text.append(line.strip())
            elif current_line is not None and not line.strip():
                # A blank line terminates an item unless the next nonblank line
                # is an indented continuation; current plan uses blank separators.
                items.append((current_line, " ".join(part.strip() for part in current_text).strip()))
                current_line, current_text = None, []
        if current_line is not None:
            items.append((current_line, " ".join(part.strip() for part in current_text).strip()))
        plan_sha = sha256(raw.encode("utf-8"))
        relative_plan = self.plan_file.relative_to(self.project_root).as_posix() if self.plan_file.is_relative_to(self.project_root) else str(self.plan_file)
        rows: list[dict[str, str]] = []
        for number, (line, scenario) in enumerate(items, start=1):
            phase_match = re.match(r"(Phases?)\s+([0-9a-zA-Z/]+)", scenario)
            owner = f"{phase_match.group(1)} {phase_match.group(2)} (revised-plan scenario)" if phase_match else "Phase 39 selected consumer scenario"
            scenario_id = f"revised-plan-scenario:{number:02d}"
            rows.append(self._row(
                record_kind="consumer_scenario",
                capability_id=scenario_id,
                canonical_id=scenario_id,
                python_route=scenario_id,
                kind="consumer_scenario",
                source_file=relative_plan,
                source_anchor=f"{relative_plan}:L{line}",
                source_sha256=plan_sha,
                aliases=[],
                contract={
                    "scenario_text": scenario,
                    "scenario_phase": owner,
                    "contract_source": "exact revised-plan text; scenario implementation and C++ evidence remain unverified",
                },
                native_candidates=[],
                owner_phase=owner,
                disposition="PLANNED",
                domain="",
                source_line=line,
            ))
            self._issue(
                f"consumer:{scenario_id}",
                scenario_id,
                "CONSUMER_SCENARIO_MAPPING_REVIEW",
                f"{relative_plan}:L{line}",
                "Scenario was inventoried from the revised plan; end-to-end consumer evidence is not attached by this static inventory.",
                "Map this scenario to explicit native consumer/test IDs, the tested candidate commit SHA, and acceptance-document hashes after execution.",
            )
        return rows

    def _row(
        self,
        *,
        record_kind: str,
        capability_id: str,
        canonical_id: str,
        python_route: str,
        kind: str,
        source_file: str,
        source_anchor: str,
        source_sha256: str,
        aliases: Sequence[str],
        contract: dict[str, Any],
        native_candidates: Sequence[str],
        owner_phase: str,
        disposition: str,
        domain: str,
        source_line: int,
    ) -> dict[str, str]:
        hints = self._acceptance_refs(f"fincore.{domain}" if domain else "")
        return {
            "row_id": f"{record_kind}:{canonical_id}:{source_line}",
            "record_kind": record_kind,
            "capability_id": capability_id,
            "canonical_id": canonical_id,
            "python_route": python_route,
            "kind": kind,
            "pin": self.source.commit,
            "source_file": source_file,
            "source_anchor": source_anchor,
            "source_sha256": source_sha256,
            "aliases": compact_json(sorted(set(aliases))),
            "contract_json": compact_json(contract),
            "native_route": compact_json(list(native_candidates)) if native_candidates else "UNMAPPED",
            "native_header": compact_json(sorted({value.split("#", 1)[0] for value in native_candidates})) if native_candidates else "UNMAPPED",
            "owner_phase": owner_phase or "UNRESOLVED",
            "disposition": disposition,
            "evidence_test_ids": "[]",
            "acceptance_sha": "[]",
            "acceptance_doc_sha256": "[]",
            "tested_candidate_dirty_snapshot_id": "NOT_RECORDED",
            "evidence_refs": compact_json(hints),
            "scope_decision": self._scope_decision(disposition),
            "verification_status": "UNVERIFIED_INVENTORY_ONLY",
            "new_pin_survival_status": "NOT_RUN",
        }

    def _add_unresolved_for_rows(self, rows: Sequence[dict[str, str]]) -> None:
        for row in rows:
            if row["disposition"] == "GAP":
                self._issue(
                    f"native-gap:{row['row_id']}",
                    row["capability_id"],
                    "NATIVE_ROUTE_UNMAPPED",
                    row["source_anchor"],
                    "No same-family native header candidate was found by a conservative symbol-name scan.",
                    "Review behavior and scope; map a proven native route or record an explicit dated DEFERRED/NON_PORT decision.",
                )
            if row["verification_status"] == "UNVERIFIED_INVENTORY_ONLY":
                # Keep evidence reconciliation actionable at row granularity.
                self._issue(
                    f"evidence:{row['row_id']}",
                    row["capability_id"],
                    "ROW_LEVEL_EVIDENCE_UNVERIFIED",
                    row["source_anchor"],
                    "No row-level C++ test ID, tested candidate commit SHA, or acceptance-document hash is asserted by this generator.",
                    "Attach exact oracle case IDs, C++ test IDs, tested candidate commit SHA, separate dirty snapshot ID, acceptance-document hashes, and retained evidence after primary review.",
                )

    def _add_source_issues(self) -> None:
        for info in self.modules.values():
            if info.is_public and info.all_is_dynamic:
                continue
            for name, definitions in info.definitions.items():
                if len(definitions) > 1 and (not name.startswith("_") or (info.exports and name in info.exports)):
                    active = info.bindings.get(name)
                    active_id = active.target_name if active and active.kind == "definition" else ""
                    for definition in definitions:
                        if definition.canonical_id != active_id:
                            self._issue(
                                f"shadowed:{definition.canonical_id}",
                                f"python:{definition.canonical_route}",
                                "SHADOWED_PUBLIC_DEFINITION",
                                self._anchor(definition.path, definition.line),
                                "A public-looking definition is shadowed by a later binding in the same module and is not the active runtime route.",
                                "Confirm whether it should remain as source-history inventory or be excluded from the public API inventory.",
                            )
    def _issue(self, key: str, capability_id: str, code: str, anchor: str, detail: str, action: str) -> None:
        issue_id = f"issue:{key}"
        if any(issue["issue_id"] == issue_id for issue in self.issues):
            return
        route = capability_id.removeprefix("python:").removeprefix("operation:")
        self.issues.append(
            {
                "issue_id": issue_id,
                "capability_id": capability_id,
                "python_route": route,
                "issue_code": code,
                "source_anchor": anchor or "UNAVAILABLE",
                "detail": detail,
                "required_action": action,
                "status": "OPEN_PRIMARY_REVIEW",
            }
        )

    def _make_summary(self, rows: Sequence[dict[str, str]], unresolved: Sequence[dict[str, str]]) -> dict[str, Any]:
        public_modules = [module for module in self.modules.values() if module.is_public]
        records_by_kind = Counter(row["record_kind"] for row in rows)
        dispositions = Counter(row["disposition"] for row in rows)
        issues = Counter(row["issue_code"] for row in unresolved)
        canonical_capabilities = {row["canonical_id"] for row in rows if row["record_kind"] == "capability"}
        alias_count = sum(
            len(set(json.loads(row["aliases"])) - {row["python_route"]})
            for row in rows
            if row["record_kind"] == "capability"
        )
        source_tree_hash = hashlib.sha256(
            "".join(f"{path}\0{sha256(self.source.read(path))}\n" for path in self.source.paths).encode("utf-8")
        ).hexdigest()
        cpp_tree_hash = hashlib.sha256(
            "".join(f"{path}\0{sha256(text.encode('utf-8'))}\n" for path, text in sorted(self.cpp_headers.items())).encode("utf-8")
        ).hexdigest()
        kind_counts = Counter(row["kind"] for row in rows if row["record_kind"] == "capability")
        plan_sha = sha256(self.plan_file.read_bytes()) if self.plan_file.exists() else "UNAVAILABLE"
        source_operation_ids = {row["operation_id"] for row in self.operation_bindings}
        native_operation_ids = set(self.native_runtime_operation_lines)
        operation_domains: dict[str, list[str]] = defaultdict(list)
        for operation_id in sorted(source_operation_ids):
            operation_domains[operation_id.split(".", 1)[0]].append(operation_id)
        operation_domain_reconciliation: dict[str, Any] = {}
        for domain, operation_ids in sorted(operation_domains.items()):
            registered = sorted(set(operation_ids) & native_operation_ids)
            missing = sorted(set(operation_ids) - native_operation_ids)
            classification, rationale = self._operation_scope_rationale(domain)
            operation_domain_reconciliation[domain] = {
                "source_count": len(operation_ids),
                "native_exact_registration_count": len(registered),
                "unregistered_source_count": len(missing),
                "registered_ids": registered,
                "unregistered_ids": missing,
                "unregistered_scope_classification": classification,
                "unregistered_scope_rationale": rationale,
            }
        native_added_ids = sorted(native_operation_ids - self.native_committed_operation_ids)
        runtime_acceptance_path = self.project_root / "docs/acceptance/2026-10-23-phase-92-runtime-catalog-acceptance.md"
        runtime_acceptance_sha = sha256(runtime_acceptance_path.read_bytes()) if runtime_acceptance_path.is_file() else "UNAVAILABLE"
        acceptance_domain_counts: dict[str, dict[str, int]] = {}
        acceptance_explicit_ids: set[str] = set()
        if runtime_acceptance_path.is_file():
            acceptance_text = runtime_acceptance_path.read_text(encoding="utf-8")
            for line in acceptance_text.splitlines():
                match = re.match(r"^\|\s*([a-z_]+)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|", line)
                if match:
                    acceptance_domain_counts[match.group(1)] = {
                        "source_count": int(match.group(2)),
                        "registered_count": int(match.group(3)),
                    }
            acceptance_explicit_ids = set(
                re.findall(r"`((?:metrics|risk|report|portfolio|factor_analysis|optimization)\.[A-Za-z0-9_.]+)`", acceptance_text)
            ) & source_operation_ids
        excluded_unmatched_count = len(operation_domains.get("data", [])) - len(set(operation_domains.get("data", [])) & native_operation_ids)
        unclassified_operation_count = len(operation_domains.get("optimization", [])) - len(set(operation_domains.get("optimization", [])) & native_operation_ids)
        contract_pending_operation_count = len(source_operation_ids - native_operation_ids) - excluded_unmatched_count - unclassified_operation_count
        capability_scope_counts = Counter()
        for row in rows:
            if row["record_kind"] == "capability" and row["disposition"] == "GAP":
                classification, _ = self._capability_scope_classification(row)
                capability_scope_counts[classification] += 1
        runtime_source_path = self.project_root / self.native_runtime_source
        runtime_registration_occurrences = 0
        if runtime_source_path.is_file():
            runtime_text = runtime_source_path.read_text(encoding="utf-8")
            runtime_registration_occurrences = len(
                re.findall(r'^\s*add\("[^"\n]+"', runtime_text, flags=re.MULTILINE)
            )
        return {
            "schema_version": SCHEMA_VERSION,
            "pin": self.source.commit,
            "source_tree_sha256": source_tree_hash,
            "source_python_file_count": len(self.source.paths),
            "native_header_file_count": len(self.cpp_headers),
            "native_header_tree_sha256": cpp_tree_hash,
            "legacy_name_mapping_tool_sha256": self.rename_hints_sha256,
            "revised_plan_sha256": plan_sha,
            "public_module_count": len(public_modules),
            "ledger_row_count": len(rows),
            "record_counts": dict(sorted(records_by_kind.items())),
            "canonical_public_implementation_count": len(canonical_capabilities),
            "public_route_alias_count": alias_count,
            "public_capability_kind_counts": dict(sorted(kind_counts.items())),
            "operation_catalog_expected_count": self.expected_operation_count,
            "operation_catalog_observed_binding_count": len(self.operation_bindings),
            "operation_catalog_unique_id_count": len({row["operation_id"] for row in self.operation_bindings}),
            "native_runtime_catalog_source": self.native_runtime_source,
            "native_runtime_catalog_source_sha256": self.native_runtime_source_sha256,
            "native_runtime_catalog_parse_status": self.native_runtime_parse_status,
            "native_runtime_catalog_registration_occurrence_count": runtime_registration_occurrences,
            "native_runtime_catalog_unique_id_count": len(native_operation_ids),
            "native_runtime_catalog_exact_ids": sorted(native_operation_ids),
            "native_runtime_catalog_source_only_ids": sorted(native_operation_ids - source_operation_ids),
            "native_runtime_catalog_committed_head": self.native_committed_head,
            "native_runtime_catalog_committed_source_sha256": self.native_committed_runtime_source_sha256,
            "native_runtime_catalog_committed_unique_id_count": len(self.native_committed_operation_ids),
            "native_runtime_catalog_current_tree_added_ids": native_added_ids,
            "phase92_acceptance_reference": {
                "path": "docs/acceptance/2026-10-23-phase-92-runtime-catalog-acceptance.md",
                "sha256": runtime_acceptance_sha,
                "asserted_catalog_count": 44,
                "domain_table_counts": acceptance_domain_counts,
                "domain_table_source_count_sum": sum(item["source_count"] for item in acceptance_domain_counts.values()),
                "domain_table_registered_count_sum": sum(item["registered_count"] for item in acceptance_domain_counts.values()),
                "explicit_source_operation_ids_in_document": sorted(acceptance_explicit_ids),
                "explicit_source_operation_id_count": len(acceptance_explicit_ids),
                "committed_runtime_ids_not_explicitly_listed_in_document": sorted(self.native_committed_operation_ids - acceptance_explicit_ids),
                "source_domains_missing_from_document_table": sorted(set(operation_domains) - set(acceptance_domain_counts)),
                "role": "count-level historical acceptance reference only; no per-ID acceptance is inferred",
            },
            "source_operation_domain_counts": {
                domain: len(ids) for domain, ids in sorted(operation_domains.items())
            },
            "source_operation_provider_count": len({row["provider_module"] for row in self.operation_bindings}),
            "source_catalog_native_reconciliation": {
                "source_count": len(source_operation_ids),
                "native_exact_registration_count": len(source_operation_ids & native_operation_ids),
                "unregistered_source_count": len(source_operation_ids - native_operation_ids),
                "classification_limit": "Exact ID membership in current working-tree src/runtime/runtime.cpp only; this is not test/acceptance evidence. Revised-plan scenario text does not enumerate a separate ID allowlist.",
                "domain_rollup": operation_domain_reconciliation,
                "unmatched_group_counts": {
                    "current_native_catalog_registrations_unverified": len(source_operation_ids & native_operation_ids),
                    "preexisting_or_typed_runtime_contract_pending": contract_pending_operation_count,
                    "explicitly_excluded_domain_unmatched": excluded_unmatched_count,
                    "unclassified_scope_rationale_review_required": unclassified_operation_count,
                    "unregistered_plan_scope_id_allowlist_status": "NOT_ENUMERATED_IN_REVISED_PLAN_TEXT; these categories are source/acceptance rationale hints, not an independently verified selected-scope partition",
                },
            },
            "unmatched_capability_scope_counts": dict(sorted(capability_scope_counts.items())),
            "unmatched_capability_scope_artifact": "docs/evidence/revised-plan-20261023/capability-inventory/unmatched-capability-scope.tsv",
            "operation_reconciliation_artifact": "docs/evidence/revised-plan-20261023/capability-inventory/operation-catalog-reconciliation.tsv",
            "consumer_scenario_count": records_by_kind.get("consumer_scenario", 0),
            "consumer_scenario_count_policy": "all top-level numbered items in the selected revised-plan section; count is not hard-coded",
            "disposition_row_counts": dict(sorted(dispositions.items())),
            "accepted_existing_row_count": dispositions.get("ACCEPTED_EXISTING", 0),
            "pass_current_native_row_count": sum(row["verification_status"] == "PASS_CURRENT_NATIVE" for row in rows),
            "acceptance_sha_semantics": "one resolvable C++ tested-candidate Git commit SHA; acceptance document hashes are separate",
            "tested_candidate_dirty_snapshot_id_policy": "CLEAN_TREE or DIRTY_SHA256:<64 lowercase hex>; explicit snapshot identity is not inferred from the commit",
            "executable_capability_disposition_counts": dict(
                sorted(Counter(row["disposition"] for row in rows if row["record_kind"] == "capability").items())
            ),
            "non_executable_module_rows_excluded_from_capability_counts": records_by_kind.get("module", 0),
            "new_pin_survival_status_counts": dict(
                sorted(Counter(row["new_pin_survival_status"] for row in rows).items())
            ),
            "unresolved_actionable_row_count": len(unresolved),
            "unresolved_issue_counts": dict(sorted(issues.items())),
            "acceptance": "INCOMPLETE; any applied dispositions must come from an exact-route primary-reviewed input; automated mapping remains non-acceptance evidence",
            "limits": [
                "AST/Git inventory does not prove dynamic runtime exports beyond statically resolvable aliases and catalog bindings.",
                "Native header symbol matches are candidate mappings only; they do not prove behavioral parity.",
                "Raw docstrings and source signatures are attached; shape, ordering, errors, and missing-value policies still require semantic review.",
                "Historical acceptance citations in candidate mappings are group-level hints; only an exact reviewed disposition overlay can populate route-level acceptance fields, and new-pin survival remains separately NOT_RUN unless proven unchanged.",
                "Dynamic catalog providers, inherited external methods, unknown exports, missing docstrings, and unmapped native rows remain in unresolved.tsv.",
            ],
        }


def validate_inventory(
    rows: Sequence[dict[str, str]],
    unresolved: Sequence[dict[str, str]],
    pin: str,
    *,
    allow_reviewed_dispositions: bool = False,
) -> None:
    if len({row.get("row_id") for row in rows}) != len(rows):
        raise ValueError("ledger row IDs must be unique")
    if len({row.get("issue_id") for row in unresolved}) != len(unresolved):
        raise ValueError("unresolved issue IDs must be unique")
    canonical_ids = [row["canonical_id"] for row in rows if row["record_kind"] == "capability"]
    if len(set(canonical_ids)) != len(canonical_ids):
        raise ValueError("canonical implementation rows must be deduplicated")
    operation_ids = [row["python_route"] for row in rows if row["record_kind"] == "operation_binding"]
    if len(set(operation_ids)) != len(operation_ids):
        raise ValueError("operation IDs must be unique in the emitted ledger")
    for row in rows:
        missing = [column for column in LEDGER_COLUMNS if not row.get(column)]
        if missing:
            raise ValueError(f"ledger row {row.get('row_id')} lacks required fields: {missing}")
        if row["pin"] != pin:
            raise ValueError(f"ledger row {row['row_id']} uses a different source pin")
        if row["disposition"] not in ALLOWED_DISPOSITIONS:
            raise ValueError(f"invalid disposition {row['disposition']!r} in {row['row_id']}")
        if row["disposition"] == "ACCEPTED_EXISTING" and not allow_reviewed_dispositions:
            raise ValueError("this discovery tool must never assert ACCEPTED_EXISTING")
        if row["new_pin_survival_status"] not in {"NOT_RUN", "SOURCE_UNCHANGED"}:
            raise ValueError(f"invalid new-pin survival status in {row['row_id']}")
        if row["acceptance_sha"] == "[]":
            if row["tested_candidate_dirty_snapshot_id"] != "NOT_RECORDED":
                raise ValueError(f"tested candidate dirty snapshot ID has no tested candidate commit in {row['row_id']}")
        else:
            tested_commits = json.loads(row["acceptance_sha"])
            if len(tested_commits) != 1 or not re.fullmatch(r"[0-9a-f]{40}", tested_commits[0]):
                raise ValueError(f"ledger tested candidate commit must be one full Git SHA in {row['row_id']}")
            if row["tested_candidate_dirty_snapshot_id"] != "CLEAN_TREE" and not re.fullmatch(
                r"DIRTY_SHA256:[0-9a-f]{64}", row["tested_candidate_dirty_snapshot_id"]
            ):
                raise ValueError(f"ledger tested candidate requires a clean/dirty snapshot ID in {row['row_id']}")
        acceptance_doc_hashes = json.loads(row["acceptance_doc_sha256"])
        if any(not re.fullmatch(r"[0-9a-f]{64}", value) for value in acceptance_doc_hashes):
            raise ValueError(f"invalid acceptance document SHA-256 in {row['row_id']}")
        if row["disposition"] == "ACCEPTED_EXISTING":
            if not allow_reviewed_dispositions or row["verification_status"] == "UNVERIFIED_INVENTORY_ONLY":
                raise ValueError(f"ACCEPTED_EXISTING lacks an explicit reviewed disposition in {row['row_id']}")
            if row["evidence_test_ids"] == "[]" or row["acceptance_sha"] == "[]":
                raise ValueError(f"ACCEPTED_EXISTING requires named test IDs and a tested candidate commit in {row['row_id']}")
            tested_commits = json.loads(row["acceptance_sha"])
            if len(tested_commits) != 1 or not re.fullmatch(r"[0-9a-f]{40}", tested_commits[0]):
                raise ValueError(f"ACCEPTED_EXISTING requires exactly one full tested candidate commit SHA in {row['row_id']}")
            if row["acceptance_doc_sha256"] == "[]":
                raise ValueError(f"ACCEPTED_EXISTING requires a separate acceptance document hash in {row['row_id']}")
            if row["tested_candidate_dirty_snapshot_id"] == "NOT_RECORDED":
                raise ValueError(f"ACCEPTED_EXISTING requires an explicit tested candidate dirty snapshot ID in {row['row_id']}")
        if row["verification_status"] == "PASS_CURRENT_NATIVE":
            if row["disposition"] != "PLANNED":
                raise ValueError(f"PASS_CURRENT_NATIVE must remain PLANNED pending primary acceptance in {row['row_id']}")
            if row["evidence_test_ids"] == "[]" or row["acceptance_sha"] == "[]" or row["acceptance_doc_sha256"] == "[]":
                raise ValueError(f"PASS_CURRENT_NATIVE requires test IDs, tested commit, and current acceptance document in {row['row_id']}")
            if row["tested_candidate_dirty_snapshot_id"] == "NOT_RECORDED":
                raise ValueError(f"PASS_CURRENT_NATIVE requires a dirty snapshot ID in {row['row_id']}")
        if not re.fullmatch(r"[0-9a-f]{64}", row["source_sha256"]):
            raise ValueError(f"invalid source SHA-256 in {row['row_id']}")
        for column in LEDGER_COLUMNS:
            value = row[column]
            if "\t" in value or "\n" in value or "\r" in value:
                raise ValueError(f"raw TSV control character in {column} for {row['row_id']}")
    for row in unresolved:
        missing = [column for column in UNRESOLVED_COLUMNS if not row.get(column)]
        if missing:
            raise ValueError(f"unresolved row {row.get('issue_id')} lacks required fields: {missing}")
        for column in UNRESOLVED_COLUMNS:
            if "\t" in row[column] or "\n" in row[column] or "\r" in row[column]:
                raise ValueError(f"raw TSV control character in {column} for {row['issue_id']}")


def write_tsv(path: Path, columns: Sequence[str], rows: Sequence[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(columns), delimiter="\t", lineterminator="\n", extrasaction="raise")
        writer.writeheader()
        writer.writerows(rows)


def validate_generated_table(rows: Sequence[dict[str, str]], columns: Sequence[str], identity_column: str) -> None:
    identities = [row.get(identity_column, "") for row in rows]
    if any(not identity for identity in identities):
        raise ValueError(f"generated table rows require {identity_column}")
    if len(set(identities)) != len(identities):
        raise ValueError(f"generated table {identity_column} values must be unique")
    for row in rows:
        missing = [column for column in columns if not row.get(column)]
        if missing:
            raise ValueError(f"generated table row {row.get(identity_column)} lacks required fields: {missing}")
        for column in columns:
            if any(character in row[column] for character in "\t\n\r"):
                raise ValueError(f"generated table raw TSV control character in {column} for {row[identity_column]}")


def _validate_evidence_references(
    references: Any,
    *,
    project_root: Path,
    require_acceptance: bool = False,
) -> list[dict[str, str]]:
    if not isinstance(references, list):
        raise ValueError("route disposition acceptance_refs must be a list")
    normalized: list[dict[str, str]] = []
    for reference in references:
        if not isinstance(reference, dict):
            raise ValueError("route disposition evidence references must be objects")
        missing = [key for key in ("path", "sha256", "locator") if not reference.get(key)]
        if missing:
            raise ValueError(f"route disposition evidence reference lacks required fields: {missing}")
        relative = Path(str(reference["path"]))
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"evidence path must be repository-relative: {relative}")
        if not re.fullmatch(r"[0-9a-f]{64}", str(reference["sha256"])):
            raise ValueError(f"invalid evidence SHA-256 for {relative}")
        full_path = project_root / relative
        if not full_path.is_file():
            raise ValueError(f"evidence reference does not exist: {relative}")
        actual_sha = sha256(full_path.read_bytes())
        if actual_sha != reference["sha256"]:
            raise ValueError(f"evidence SHA-256 mismatch for {relative}")
        default_role = "historical_acceptance" if str(relative).startswith("docs/acceptance/") else "scope_evidence"
        role = str(reference.get("role", default_role))
        if role not in {"historical_acceptance", "current_acceptance", "scope_evidence"}:
            raise ValueError(f"unsupported evidence role for {relative}: {role!r}")
        if role == "historical_acceptance" and not str(relative).startswith("docs/acceptance/"):
            raise ValueError(f"historical acceptance references must be under docs/acceptance/: {relative}")
        normalized.append(
            {
                "path": str(relative),
                "sha256": actual_sha,
                "locator": str(reference["locator"]),
                "role": role,
            }
        )
    if require_acceptance and not any(reference["role"] == "historical_acceptance" for reference in normalized):
        raise ValueError("accepted route requires a historical acceptance reference under docs/acceptance/")
    return normalized


def _verify_source_unchanged(
    source: GitSource,
    base_revision: Any,
    source_path: str,
    expected_source_sha256: str,
) -> None:
    if not isinstance(base_revision, str) or not re.fullmatch(r"[0-9a-f]{40}", base_revision):
        raise ValueError("SOURCE_UNCHANGED requires a full historical_source_commit SHA")
    resolved_base = source.git("rev-parse", f"{base_revision}^{{commit}}").strip()
    if resolved_base != base_revision:
        raise ValueError("historical_source_commit must be a resolved full commit SHA")
    try:
        baseline = subprocess.run(
            ["git", "-C", str(source.repository), "show", f"{resolved_base}:{source_path}"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        ).stdout
    except subprocess.CalledProcessError as error:
        raise ValueError(f"SOURCE_UNCHANGED is invalid: source file is absent at historical_source_commit: {source_path}") from error
    target = source.read(source_path)
    if sha256(baseline) != expected_source_sha256:
        raise ValueError("SOURCE_UNCHANGED is invalid: source file hash differs at historical_source_commit")
    if sha256(target) != expected_source_sha256:
        raise ValueError("SOURCE_UNCHANGED is invalid: source file hash differs at the current Python pin")


def _validate_tested_candidate_commit(project_root: Path, value: Any) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("ACCEPTED_EXISTING requires a full tested_candidate_commit SHA")
    try:
        resolved = subprocess.run(
            ["git", "-C", str(project_root), "rev-parse", f"{value}^{{commit}}"],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        ).stdout.strip()
    except subprocess.CalledProcessError as error:
        raise ValueError("tested_candidate_commit must resolve in the C++ project Git repository") from error
    if resolved != value:
        raise ValueError("tested_candidate_commit must be a resolved full commit SHA")
    return resolved


def _validate_dirty_snapshot_id(value: Any) -> str:
    if not isinstance(value, str):
        raise ValueError("ACCEPTED_EXISTING requires tested_candidate_dirty_snapshot_id")
    if value == "CLEAN_TREE" or re.fullmatch(r"DIRTY_SHA256:[0-9a-f]{64}", value):
        return value
    raise ValueError("tested_candidate_dirty_snapshot_id must be CLEAN_TREE or DIRTY_SHA256:<64 lowercase hex>")


def apply_route_dispositions(
    rows: Sequence[dict[str, str]],
    disposition_document: Mapping[str, Any] | None,
    *,
    source: GitSource,
    project_root: Path,
) -> dict[str, Any]:
    if disposition_document is None:
        return {}
    if disposition_document.get("schema_version") != DISPOSITION_SCHEMA_VERSION:
        raise ValueError("unsupported capability disposition schema_version")
    if disposition_document.get("source_pin") != source.commit:
        raise ValueError("capability disposition source_pin does not match the inventory pin")
    entries = disposition_document.get("routes")
    if not isinstance(entries, list):
        raise ValueError("capability disposition routes must be a list")
    if entries:
        if disposition_document.get("primary_reviewed") is not True:
            raise ValueError("non-empty capability dispositions require primary_reviewed=true")
        if not disposition_document.get("reviewed_by"):
            raise ValueError("non-empty capability dispositions require reviewed_by")
        if not re.fullmatch(r"20\d{2}-\d{2}-\d{2}", str(disposition_document.get("review_date", ""))):
            raise ValueError("non-empty capability dispositions require an ISO review_date")

    rows_by_route: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        rows_by_route[row["python_route"]].append(row)
    overlays: dict[str, Mapping[str, Any]] = {}
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("capability disposition route entries must be objects")
        required = [
            "python_route",
            "canonical_id",
            "source_sha256",
            "disposition",
            "native_routes",
            "native_headers",
            "owner_phase",
            "evidence_test_ids",
            "acceptance_refs",
            "scope_decision",
            "verification_status",
            "new_pin_survival_status",
        ]
        missing = [key for key in required if key not in entry or entry[key] is None or entry[key] == ""]
        if missing:
            raise ValueError(f"route disposition lacks required fields: {missing}")
        route = str(entry["python_route"])
        if route in overlays:
            raise ValueError(f"duplicate explicit route disposition for {route}")
        matching = rows_by_route.get(route, [])
        if len(matching) != 1:
            raise ValueError(f"route disposition requires one exact canonical inventory route: {route}")
        row = matching[0]
        if row["record_kind"] == "module":
            raise ValueError(f"module namespace row cannot receive a route disposition: {route}")
        if row["canonical_id"] != entry["canonical_id"]:
            raise ValueError(f"canonical_id mismatch for explicit route {route}")
        if row["source_sha256"] != entry["source_sha256"]:
            raise ValueError(f"source_sha256 mismatch for explicit route {route}")
        if entry["disposition"] not in ALLOWED_DISPOSITIONS:
            raise ValueError(f"invalid explicit disposition for {route}: {entry['disposition']!r}")
        if not isinstance(entry["native_routes"], list) or not entry["native_routes"] or not all(
            isinstance(value, str) and value for value in entry["native_routes"]
        ):
            raise ValueError(f"native_routes must be a non-empty string list for {route}")
        if not isinstance(entry["native_headers"], list) or not entry["native_headers"] or not all(
            isinstance(value, str) and value for value in entry["native_headers"]
        ):
            raise ValueError(f"native_headers must be a non-empty string list for {route}")
        if not isinstance(entry["evidence_test_ids"], list) or not all(
            isinstance(value, str) and value for value in entry["evidence_test_ids"]
        ):
            raise ValueError(f"evidence_test_ids must be a string list for {route}")
        if not isinstance(entry["owner_phase"], str) or not isinstance(entry["scope_decision"], str):
            raise ValueError(f"owner_phase and scope_decision must be strings for {route}")
        if not isinstance(entry["verification_status"], str):
            raise ValueError(f"verification_status must be a string for {route}")
        if not re.search(r"\b20\d{2}-\d{2}-\d{2}\b", str(entry["scope_decision"])):
            raise ValueError(f"scope_decision must include a dated decision for {route}")
        survival = entry["new_pin_survival_status"]
        if survival not in {"NOT_RUN", "SOURCE_UNCHANGED"}:
            raise ValueError(f"unsupported new_pin_survival_status for {route}: {survival!r}")
        if survival == "SOURCE_UNCHANGED":
            _verify_source_unchanged(source, entry.get("historical_source_commit"), row["source_file"], row["source_sha256"])

        disposition = entry["disposition"]
        evidence = _validate_evidence_references(
            entry["acceptance_refs"],
            project_root=project_root,
        )
        has_candidate_commit = entry.get("tested_candidate_commit") not in (None, "")
        has_dirty_snapshot = entry.get("tested_candidate_dirty_snapshot_id") not in (None, "")
        if has_candidate_commit != has_dirty_snapshot:
            raise ValueError(f"tested_candidate_commit and tested_candidate_dirty_snapshot_id must be provided together: {route}")
        tested_candidate_commit: str | None = None
        dirty_snapshot_id: str | None = None
        if has_candidate_commit:
            tested_candidate_commit = _validate_tested_candidate_commit(project_root, entry["tested_candidate_commit"])
            dirty_snapshot_id = _validate_dirty_snapshot_id(entry["tested_candidate_dirty_snapshot_id"])
        if disposition == "ACCEPTED_EXISTING":
            if not entry["evidence_test_ids"]:
                raise ValueError(f"ACCEPTED_EXISTING requires named historical test IDs: {route}")
            if not any(reference["role"] == "historical_acceptance" for reference in evidence):
                raise ValueError(f"ACCEPTED_EXISTING requires a named historical acceptance cite: {route}")
            if tested_candidate_commit is None or dirty_snapshot_id is None:
                raise ValueError(f"ACCEPTED_EXISTING requires a tested candidate commit and dirty snapshot ID: {route}")
            if entry["native_routes"] == ["UNMAPPED"] or entry["native_headers"] == ["UNMAPPED"]:
                raise ValueError(f"ACCEPTED_EXISTING requires an explicit native route and header: {route}")
        if entry["verification_status"] == "PASS_CURRENT_NATIVE":
            if disposition != "PLANNED":
                raise ValueError(f"PASS_CURRENT_NATIVE routes must remain PLANNED pending primary acceptance: {route}")
            if tested_candidate_commit is None or dirty_snapshot_id is None:
                raise ValueError(f"PASS_CURRENT_NATIVE requires a tested candidate commit and dirty snapshot ID: {route}")
            if not entry["evidence_test_ids"]:
                raise ValueError(f"PASS_CURRENT_NATIVE requires named current test IDs: {route}")
            if not any(reference["role"] == "current_acceptance" for reference in evidence):
                raise ValueError(f"PASS_CURRENT_NATIVE requires a current_acceptance reference: {route}")
        if tested_candidate_commit is not None and dirty_snapshot_id is not None:
            entry = dict(entry)
            entry["_tested_candidate_commit"] = tested_candidate_commit
            entry["_tested_candidate_dirty_snapshot_id"] = dirty_snapshot_id
        if disposition in {"DEFERRED", "NON_PORT"} and not evidence:
            raise ValueError(f"{disposition} requires a dated scope decision and document reference: {route}")
        overlays[route] = entry

    # Recheck class completeness after all exact entries have been collected.
    for route, entry in overlays.items():
        if entry["disposition"] != "ACCEPTED_EXISTING":
            continue
        row = rows_by_route[route][0]
        if row["kind"] not in {"class", "dataclass"}:
            continue
        member_prefix = f"{route}."
        public_members = [
            candidate["python_route"]
            for candidate in rows
            if candidate["record_kind"] == "capability"
            and candidate["python_route"].startswith(member_prefix)
            and candidate["kind"] in {"method", "async_method", "classmethod", "staticmethod", "property", "property_setter", "property_deleter"}
        ]
        missing_members = [member for member in public_members if overlays.get(member, {}).get("disposition") != "ACCEPTED_EXISTING"]
        if missing_members:
            raise ValueError(f"ACCEPTED_EXISTING class has unaccepted public members {route}: {missing_members[:5]}")

    for route, entry in overlays.items():
        if entry["verification_status"] != "PASS_CURRENT_NATIVE":
            continue
        row = rows_by_route[route][0]
        if row["kind"] not in {"class", "dataclass"}:
            continue
        member_prefix = f"{route}."
        public_members = [
            candidate["python_route"]
            for candidate in rows
            if candidate["record_kind"] == "capability"
            and candidate["python_route"].startswith(member_prefix)
            and candidate["kind"] in {"method", "async_method", "classmethod", "staticmethod", "property", "property_setter", "property_deleter"}
        ]
        missing_members = [member for member in public_members if overlays.get(member, {}).get("verification_status") != "PASS_CURRENT_NATIVE"]
        if missing_members:
            raise ValueError(f"PASS_CURRENT_NATIVE class has unverified public members {route}: {missing_members[:5]}")

    for route, entry in overlays.items():
        row = rows_by_route[route][0]
        evidence = _validate_evidence_references(entry["acceptance_refs"], project_root=project_root)
        row["native_route"] = compact_json(entry["native_routes"]) if entry["native_routes"] != ["UNMAPPED"] else "UNMAPPED"
        row["native_header"] = compact_json(entry["native_headers"]) if entry["native_headers"] != ["UNMAPPED"] else "UNMAPPED"
        row["owner_phase"] = str(entry["owner_phase"])
        row["disposition"] = str(entry["disposition"])
        row["evidence_test_ids"] = compact_json(sorted(set(entry["evidence_test_ids"])))
        if "_tested_candidate_commit" in entry:
            row["acceptance_sha"] = compact_json([entry["_tested_candidate_commit"]])
            row["tested_candidate_dirty_snapshot_id"] = entry["_tested_candidate_dirty_snapshot_id"]
        acceptance_document_hashes = {
            reference["sha256"]
            for reference in evidence
            if reference["role"] in {"historical_acceptance", "current_acceptance"}
        }
        if acceptance_document_hashes:
            row["acceptance_doc_sha256"] = compact_json(
                sorted(acceptance_document_hashes)
            )
        row["evidence_refs"] = compact_json(evidence)
        row["scope_decision"] = str(entry["scope_decision"])
        row["verification_status"] = str(entry["verification_status"])
        row["new_pin_survival_status"] = str(entry["new_pin_survival_status"])
    return {
        "explicit_route_disposition_count": len(overlays),
        "accepted_existing_count": sum(entry["disposition"] == "ACCEPTED_EXISTING" for entry in overlays.values()),
        "pass_current_native_count": sum(entry["verification_status"] == "PASS_CURRENT_NATIVE" for entry in overlays.values()),
        "verification_status_counts": dict(sorted(Counter(str(entry["verification_status"]) for entry in overlays.values()).items())),
        "new_pin_survival_counts": dict(sorted(Counter(str(entry["new_pin_survival_status"]) for entry in overlays.values()).items())),
        "class_acceptance_requires_all_public_members": True,
        "aliases_are_not_separate_disposition_keys": True,
    }


def _candidate_test_ids(group: Mapping[str, Any]) -> list[str]:
    values: list[str] = []
    for key in ("existing_test_ids", "test_ids", "tests"):
        tests = group.get(key)
        if isinstance(tests, dict):
            for test_file, identifiers in tests.items():
                for identifier in identifiers if isinstance(identifiers, list) else [identifiers]:
                    values.append(f"{test_file}:{identifier}")
        elif isinstance(tests, list):
            values.extend(str(value) for value in tests if value)
        elif isinstance(tests, str) and tests:
            values.append(tests)
    for test in group.get("test_locations", []) if isinstance(group.get("test_locations", []), list) else []:
        if isinstance(test, dict) and test.get("test_id"):
            values.append(f"{test.get('file', 'UNSPECIFIED')}:{test['test_id']}")
    return sorted(set(values))


def _candidate_evidence_refs(group: Mapping[str, Any]) -> tuple[list[str], list[str]]:
    all_refs: list[str] = []
    acceptance_refs: list[str] = []
    for key in ("acceptance_refs", "acceptance_or_plan_refs", "evidence", "native_refs", "source_refs_at_pin"):
        values = group.get(key, [])
        if isinstance(values, str):
            values = [values]
        if not isinstance(values, list):
            continue
        for value in values:
            if isinstance(value, dict):
                rendered = compact_json(value)
                path_value = str(value.get("path", ""))
            else:
                rendered = str(value)
                path_value = rendered
            if not rendered:
                continue
            all_refs.append(rendered)
            if "docs/acceptance/" in path_value:
                acceptance_refs.append(rendered)
    return sorted(set(all_refs)), sorted(set(acceptance_refs))


def _candidate_value_sequence(value: Any) -> list[Any]:
    if isinstance(value, str):
        return [value] if value else []
    if not isinstance(value, list):
        return []
    if len(value) > 1 and all(isinstance(item, str) and len(item) == 1 for item in value):
        joined = "".join(value)
        if len(joined) >= 16 and any(marker in joined for marker in ("/", "::", ".", " ")):
            return [joined]
    return value


def _candidate_native_routes(group: Mapping[str, Any]) -> tuple[list[str], list[str]]:
    routes: list[str] = []
    headers: list[str] = []
    for key in ("candidate_native_routes", "native_routes", "native_route"):
        for value in _candidate_value_sequence(group.get(key, [])):
            if isinstance(value, dict):
                if value.get("route"):
                    routes.append(str(value["route"]))
                if value.get("header"):
                    headers.append(str(value["header"]))
            elif value:
                routes.append(str(value))
    for value in _candidate_value_sequence(group.get("native_headers", [])):
        if value:
            headers.append(str(value))
    return sorted(set(routes)), sorted(set(headers))


def _candidate_group_facts(group: Mapping[str, Any]) -> dict[str, Any]:
    notes: list[str] = []
    for key in ("limitation", "semantic_limits", "semantic_notes", "bounded_primary_follow_up", "finding", "proposed_bounded_scope"):
        value = group.get(key)
        if isinstance(value, list):
            notes.extend(str(item) for item in value if item)
        elif isinstance(value, str) and value:
            notes.append(value)
    classification = str(
        group.get("candidate_classification")
        or group.get("classification")
        or group.get("candidate_status")
        or "mapping classification unavailable"
    )
    routes, headers = _candidate_native_routes(group)
    evidence, acceptance = _candidate_evidence_refs(group)
    return {
        "classification": classification,
        "native_routes": routes,
        "native_headers": headers,
        "owner_phase_candidate": str(group.get("owner_phase") or group.get("phase") or "UNRESOLVED"),
        "test_ids_candidate": _candidate_test_ids(group),
        "evidence_refs_candidate": evidence,
        "acceptance_refs_candidate": acceptance,
        "limitations": sorted(set(notes)),
    }


def build_reviewed_mapping_input(
    candidate_paths: Sequence[Path],
    rows: Sequence[dict[str, str]],
    *,
    pin: str,
) -> dict[str, Any]:
    if not candidate_paths:
        raise ValueError("at least one read-only candidate mapping file is required")
    row_by_route: dict[str, list[dict[str, str]]] = defaultdict(list)
    rows_by_canonical: dict[str, dict[str, str]] = {}
    for row in rows:
        row_by_route[row["python_route"]].append(row)
        rows_by_canonical[row["canonical_id"]] = row
        try:
            aliases = json.loads(row["aliases"])
        except json.JSONDecodeError:
            aliases = []
        for alias in aliases:
            if str(alias) != row["python_route"]:
                row_by_route[str(alias)].append(row)

    route_records: dict[str, dict[str, Any]] = {}
    unmatched_records: dict[tuple[str, str, str], dict[str, Any]] = {}
    expanded_family_records: dict[tuple[str, str, str], dict[str, Any]] = {}
    manifests: list[dict[str, str]] = []

    def add_route_candidate(
        source_name: str,
        source_path: str,
        source_sha: str,
        group_id: str,
        candidate_route: str,
        group: Mapping[str, Any],
    ) -> None:
        facts = _candidate_group_facts(group)
        matches = row_by_route.get(candidate_route, [])
        resolution = "EXACT_ROUTE" if len(matches) == 1 and matches[0]["python_route"] == candidate_route else "ALIAS_ROUTE" if len(matches) == 1 else "UNMATCHED_OR_AMBIGUOUS"
        if candidate_route.endswith(" and its methods"):
            family_route = candidate_route.removesuffix(" and its methods")
            family_matches = [row for row in rows if row["python_route"] == family_route or row["python_route"].startswith(f"{family_route}.")]
            if family_matches:
                matches = family_matches
                resolution = "EXPANDED_EXACT_ROUTE_FAMILY"
        if len(matches) != 1:
            key = (source_name, group_id, candidate_route)
            if resolution == "EXPANDED_EXACT_ROUTE_FAMILY":
                expanded_family_records[key] = {
                    "candidate_source": source_name,
                    "candidate_file": source_path,
                    "candidate_file_sha256": source_sha,
                    "candidate_group_id": group_id,
                    "candidate_route_family": candidate_route,
                    "resolved_python_routes": sorted({row["python_route"] for row in matches}),
                    "route_resolution": resolution,
                    **facts,
                    "review_status": "FAMILY_EXPANDED_TO_EXACT_SOURCE_ROUTES_PRIMARY_REVIEW_REQUIRED",
                }
            else:
                unmatched_records[key] = {
                    "candidate_source": source_name,
                    "candidate_file": source_path,
                    "candidate_file_sha256": source_sha,
                    "candidate_group_id": group_id,
                    "candidate_route": candidate_route,
                    "route_resolution": resolution,
                    **facts,
                    "review_status": "UNRESOLVED_CANDIDATE_ROUTE_PRIMARY_REVIEW_REQUIRED",
                }
                return
        for row in matches:
            canonical_id = row["canonical_id"]
            route_id = row["row_id"]
            record = route_records.setdefault(
                route_id,
                {
                    "route_id": route_id,
                    "canonical_id": canonical_id,
                    "python_route": row["python_route"],
                    "kind": row["kind"],
                    "record_kind": row["record_kind"],
                    "source_file": row["source_file"],
                    "source_anchor": row["source_anchor"],
                    "source_sha256": row["source_sha256"],
                    "pin": row["pin"],
                    "aliases": json.loads(row["aliases"]),
                    "candidate_mappings": [],
                },
            )
            record["candidate_mappings"].append(
                {
                    "candidate_source": source_name,
                    "candidate_file": source_path,
                    "candidate_file_sha256": source_sha,
                    "candidate_group_id": group_id,
                    "candidate_route": candidate_route,
                    "route_resolution": resolution,
                    "route_was_alias": candidate_route != row["python_route"],
                    **facts,
                }
            )

    for candidate_path in candidate_paths:
        path = candidate_path.resolve()
        if not path.is_file():
            raise ValueError(f"candidate mapping file does not exist: {path}")
        content = path.read_bytes()
        document = json.loads(content)
        candidate_pin = (
            document.get("source_pin")
            or document.get("python_source", {}).get("pin")
            or document.get("source", {}).get("pin")
        )
        if candidate_pin and str(candidate_pin) != pin:
            raise ValueError(f"candidate mapping pin mismatch in {path}: {candidate_pin}")
        source_sha = sha256(content)
        source_name = path.name.removeprefix("fincore-").removesuffix("-mapping-candidates.json")
        source_path = str(path)
        manifests.append({"source": source_name, "path": source_path, "sha256": source_sha, "pin": pin})

        if isinstance(document.get("candidate_groups"), list):
            for index, group in enumerate(document["candidate_groups"]):
                group_id = f"foundation-{index:03d}"
                for route in group.get("routes", []):
                    add_route_candidate(source_name, source_path, source_sha, group_id, str(route), group)
        if isinstance(document.get("candidates"), list):
            for group in document["candidates"]:
                group_id = str(group.get("group_id") or "delivery-unknown")
                for row in group.get("ledger_rows", []):
                    route = str(row.get("python_route", ""))
                    add_route_candidate(source_name, source_path, source_sha, group_id, route, group)
        if isinstance(document.get("mappings"), list):
            for group in document["mappings"]:
                group_id = str(group.get("domain") or "factor-unknown")
                for row in group.get("source_routes", []):
                    route = str(row.get("route", "")) if isinstance(row, dict) else str(row)
                    add_route_candidate(source_name, source_path, source_sha, group_id, route, group)
        if isinstance(document.get("unresolved_or_non_1_to_1_routes"), list):
            for index, group in enumerate(document["unresolved_or_non_1_to_1_routes"]):
                group_id = f"factor-unresolved-{index:03d}"
                for value in group.get("source_routes", []):
                    route = re.split(r"\s+\(", str(value), maxsplit=1)[0]
                    add_route_candidate(source_name, source_path, source_sha, group_id, route, group)
        if isinstance(document.get("supported_existing_exclusions"), list):
            for index, group in enumerate(document["supported_existing_exclusions"]):
                group_id = f"factor-exclusion-{index:03d}"
                for route in group.get("source_route_families", []):
                    add_route_candidate(source_name, source_path, source_sha, group_id, str(route), group)
        for field_name, group_prefix, route_field in (
            ("selected_scope_review_items", "foundation-selected", "routes"),
        ):
            for index, group in enumerate(document.get(field_name, [])) if isinstance(document.get(field_name), list) else []:
                for route in group.get(route_field, []):
                    add_route_candidate(source_name, source_path, source_sha, f"{group_prefix}-{index:03d}", str(route), group)
        if isinstance(document.get("candidate_module_gap_routes"), list):
            module_note = {
                "classification": "namespace container candidate; this route does not count as an executable capability",
                "candidate_native_routes": [],
                "acceptance_or_plan_refs": [],
            }
            for route in document["candidate_module_gap_routes"]:
                add_route_candidate(source_name, source_path, source_sha, "delivery-module-container", str(route), module_note)

    accepted_routes: set[str] = set()
    for route_id, record in route_records.items():
        record["candidate_mappings"].sort(
            key=lambda item: (item["candidate_source"], item["candidate_group_id"], item["candidate_route"])
        )
        labels = " ".join(item["classification"] + " " + " ".join(item["limitations"]) for item in record["candidate_mappings"]).lower()
        tests = sorted({test for item in record["candidate_mappings"] for test in item["test_ids_candidate"]})
        acceptance_refs = sorted({ref for item in record["candidate_mappings"] for ref in item["acceptance_refs_candidate"]})
        native_routes = sorted({route for item in record["candidate_mappings"] for route in item["native_routes"]})
        exact_candidate_route = any(
            item["route_resolution"] in {"EXACT_ROUTE", "ALIAS_ROUTE"}
            for item in record["candidate_mappings"]
        )
        exclusion_candidate = any(token in labels for token in ("non-port", "nonport", "out-of-scope", "excluded", "provider scope exclusion"))
        partial_candidate = any(token in labels for token in ("partial", "unmapped", "unmatched", "no exact", "genuinely absent", "remain separate", "separate coverage question"))
        evidence_candidate = bool(tests and acceptance_refs and exact_candidate_route)
        member_prefix = f"{record['python_route']}."
        members = [
            row["python_route"]
            for row in rows
            if row["record_kind"] == "capability"
            and row["python_route"].startswith(member_prefix)
            and row["kind"] in {"method", "async_method", "classmethod", "staticmethod", "property", "property_setter", "property_deleter"}
        ] if record["kind"] in {"class", "dataclass"} else []
        candidate_routes = {
            candidate["candidate_route"]
            for candidate in record["candidate_mappings"]
            if candidate["route_resolution"] != "UNMATCHED_OR_AMBIGUOUS"
        }
        member_candidate_gaps: list[str] = []
        for member in members:
            member_record = next(
                (candidate_record for candidate_record in route_records.values() if candidate_record["python_route"] == member and candidate_record["record_kind"] == "capability"),
                None,
            )
            if not member_record:
                member_candidate_gaps.append(member)
                continue
            member_labels = " ".join(
                mapping["classification"] + " " + " ".join(mapping["limitations"])
                for mapping in member_record["candidate_mappings"]
            ).lower()
            member_native = any(mapping["native_routes"] for mapping in member_record["candidate_mappings"])
            member_partial = any(token in member_labels for token in ("partial", "unmapped", "unmatched", "no exact", "genuinely absent", "remain separate", "separate coverage question"))
            if not member_native or member_partial:
                member_candidate_gaps.append(member)
        unmatched_members = sorted(set(member for member in members if member not in candidate_routes) | set(member_candidate_gaps))
        if record["record_kind"] == "module":
            proposal = "GAP"
            review_status = "NAMESPACE_CONTAINER_NOT_EXECUTABLE"
        elif exclusion_candidate and not partial_candidate:
            proposal = "DEFERRED" if "defer" in labels else "NON_PORT"
            review_status = "DATED_SCOPE_EVIDENCE_CANDIDATE_UNVERIFIED"
        elif evidence_candidate and not partial_candidate and not unmatched_members and native_routes:
            proposal = "ACCEPTED_EXISTING"
            review_status = "HISTORICAL_ACCEPTANCE_CANDIDATE_GROUP_EVIDENCE_UNVERIFIED"
        elif partial_candidate or unmatched_members:
            proposal = "GAP"
            review_status = "PARTIAL_OR_UNMATCHED_PRIMARY_REVIEW_REQUIRED"
        elif native_routes:
            proposal = "PLANNED"
            review_status = "NATIVE_MAPPING_CANDIDATE_UNVERIFIED"
        else:
            proposal = "GAP"
            review_status = "NO_NATIVE_MAPPING_CANDIDATE_PRIMARY_REVIEW_REQUIRED"
        if proposal == "ACCEPTED_EXISTING":
            accepted_routes.add(route_id)
        record.update(
            {
                "candidate_review": {
                    "proposed_disposition": proposal,
                    "proposal_status": review_status,
                    "historical_acceptance_evidence_candidate": evidence_candidate,
                    "historical_evidence_scope": "candidate-group-level; exact route applicability is unverified",
                    "class_unmatched_public_member_routes": sorted(unmatched_members),
                    "class_group_acceptance_blocked": bool(unmatched_members),
                    "primary_review_required": True,
                },
                "aliases_count": len(set(record["aliases"]) - {record["python_route"]}),
            }
        )

    route_records_list = sorted(route_records.values(), key=lambda item: (item["python_route"], item["record_kind"], item["canonical_id"], item["route_id"]))
    unmatched_list = sorted(unmatched_records.values(), key=lambda item: (item["candidate_source"], item["candidate_group_id"], item["candidate_route"]))
    if len({item["route_id"] for item in route_records_list}) != len(route_records_list):
        raise ValueError("reviewed mapping input contains duplicate inventory route records")
    alias_routes = {
        alias
        for record in route_records_list
        if record["record_kind"] == "capability"
        for alias in set(record["aliases"]) - {record["python_route"]}
    }
    return {
        "schema_version": "1",
        "source_pin": pin,
        "purpose": "Normalized route-level mapping candidates only; no disposition or acceptance is applied by import.",
        "candidate_sources": sorted(manifests, key=lambda item: (item["source"], item["path"])),
        "route_count": len(route_records_list),
        "candidate_mapping_count": sum(len(record["candidate_mappings"]) for record in route_records_list),
        "distinct_alias_route_count": len(alias_routes),
        "historical_acceptance_candidate_route_count": sum(
            record["candidate_review"]["historical_acceptance_evidence_candidate"] for record in route_records_list
        ),
        "proposed_acceptance_existing_candidate_count": len(accepted_routes),
        "unmatched_candidate_route_count": len(unmatched_list),
        "routes": route_records_list,
        "unmatched_candidate_routes": unmatched_list,
        "expanded_candidate_route_families": sorted(
            expanded_family_records.values(),
            key=lambda item: (item["candidate_source"], item["candidate_group_id"], item["candidate_route_family"]),
        ),
        "policy": {
            "candidate_data_is_not_acceptance": True,
            "class_rows_blocked_when_public_members_are_missing": True,
            "modules_are_namespace_containers_only": True,
            "aliases_grouped_under_canonical_implementation": True,
            "current_pin_survival_status": "NOT_RUN unless an exact fincore/ diff comparison is explicitly verified",
        },
    }


def validate_reviewed_mapping_input(document: Mapping[str, Any], rows: Sequence[dict[str, str]], pin: str) -> None:
    if document.get("schema_version") != "1" or document.get("source_pin") != pin:
        raise ValueError("reviewed mapping input schema or source pin mismatch")
    route_records = document.get("routes")
    if not isinstance(route_records, list):
        raise ValueError("reviewed mapping input routes must be a list")
    rows_by_id = {row["row_id"]: row for row in rows}
    seen: set[str] = set()
    for record in route_records:
        if not isinstance(record, dict):
            raise ValueError("reviewed mapping input route records must be objects")
        route_id = str(record.get("route_id", ""))
        if route_id in seen:
            raise ValueError(f"duplicate reviewed mapping route {route_id}")
        seen.add(route_id)
        row = rows_by_id.get(route_id)
        if row is None:
            raise ValueError(f"reviewed mapping references unknown inventory route {route_id}")
        for key in ("canonical_id", "python_route", "source_file", "source_sha256", "kind", "record_kind"):
            if record.get(key) != row[key]:
                raise ValueError(f"reviewed mapping {key} mismatch for {route_id}")
        if row["record_kind"] == "module" and record.get("candidate_review", {}).get("proposed_disposition") == "ACCEPTED_EXISTING":
            raise ValueError(f"module namespace rows cannot be acceptance candidates: {route_id}")
    unmatched = document.get("unmatched_candidate_routes", [])
    if not isinstance(unmatched, list):
        raise ValueError("unmatched_candidate_routes must be a list")
    families = document.get("expanded_candidate_route_families", [])
    if not isinstance(families, list):
        raise ValueError("expanded_candidate_route_families must be a list")


def build_primary_review_queue(
    rows: Sequence[dict[str, str]],
    reviewed_mapping: Mapping[str, Any],
    builder: InventoryBuilder,
) -> list[dict[str, str]]:
    mapping_by_route_id = {record["route_id"]: record for record in reviewed_mapping.get("routes", [])}
    scope_rows = builder._capability_scope_rows(rows)
    output: list[dict[str, str]] = []
    added: set[tuple[str, str]] = set()

    def add(queue_kind: str, row: dict[str, str], record: Mapping[str, Any] | None, scope_classification: str, proposal: str, reason: str) -> None:
        key = (queue_kind, row["row_id"])
        if key in added:
            return
        added.add(key)
        mappings = record.get("candidate_mappings", []) if record else []
        tests = sorted({test for item in mappings for test in item.get("test_ids_candidate", [])})
        acceptance_refs = sorted({ref for item in mappings for ref in item.get("acceptance_refs_candidate", [])})
        native_routes = sorted({native for item in mappings for native in item.get("native_routes", [])})
        output.append(
            {
                "queue_kind": queue_kind,
                "python_route": row["python_route"],
                "canonical_id": row["canonical_id"],
                "kind": row["kind"],
                "source_file": row["source_file"],
                "source_sha256": row["source_sha256"],
                "scope_classification": scope_classification,
                "proposed_disposition": proposal,
                "candidate_groups": compact_json(sorted({item["candidate_group_id"] for item in mappings})),
                "native_routes": compact_json(native_routes),
                "evidence_test_ids": compact_json(tests),
                "acceptance_refs": compact_json(acceptance_refs),
                "review_reason": reason,
            }
        )

    for scope in scope_rows:
        row = next((item for item in rows if item["canonical_id"] == scope["canonical_id"]), None)
        if row is None:
            continue
        record = mapping_by_route_id.get(row["row_id"])
        candidate_review = record.get("candidate_review", {}) if record else {}
        proposed = candidate_review.get("proposed_disposition", "GAP")
        is_unmatched = not record or proposed == "GAP"
        if is_unmatched and scope["scope_classification"] == "EXPLICIT_SELECTED_PLAN_SCOPE_UNMATCHED":
            reason = (
                "Selected-scope route has no exact candidate mapping; primary review must assign route-level mapping or keep GAP."
                if not record
                else "Selected-scope candidate is partial, unmatched, or unresolved; keep GAP until exact route coverage is reviewed."
            )
            add("UNMATCHED_SELECTED_SCOPE", row, record, scope["scope_classification"], "GAP", reason)

    for row in rows:
        if row["record_kind"] not in {"capability", "operation_binding", "public_value"}:
            continue
        record = mapping_by_route_id.get(row["row_id"])
        if not record:
            continue
        candidate_review = record.get("candidate_review", {})
        if not candidate_review.get("historical_acceptance_evidence_candidate"):
            continue
        if row["record_kind"] == "module":
            continue
        class_blocked = candidate_review.get("class_group_acceptance_blocked")
        reason = (
            "Candidate mapping provides named tests and acceptance references at group level; verify exact route coverage and source-contract limits before disposition."
        )
        if class_blocked:
            reason += " Class acceptance is blocked until every public member has exact evidence/disposition."
        classification = "HISTORICAL_ACCEPTANCE_CANDIDATE_GROUP_LEVEL"
        proposed = candidate_review.get("proposed_disposition", "GAP")
        add("HISTORICAL_ACCEPTANCE_CANDIDATE", row, record, classification, proposed, reason)
    output.sort(key=lambda item: (item["queue_kind"], item["python_route"], item["canonical_id"]))
    identities = [(row["queue_kind"], row["canonical_id"]) for row in output]
    if len(set(identities)) != len(identities):
        raise ValueError("primary review queue identities must be unique")
    for item in output:
        missing = [column for column in REVIEW_QUEUE_COLUMNS if not item.get(column)]
        if missing:
            raise ValueError(f"primary review queue row lacks required fields: {missing}")
        for key, value in item.items():
            if "\t" in value or "\n" in value or "\r" in value:
                raise ValueError(f"primary review queue contains a raw TSV control character in {key}")
    return output


def generate(
    *,
    repository: Path,
    revision: str,
    cpp_root: Path,
    plan_file: Path,
    ledger_path: Path,
    unresolved_path: Path,
    summary_path: Path,
    expected_operation_count: int = EXPECTED_OPERATION_COUNT,
    dispositions_path: Path | None = None,
    candidate_paths: Sequence[Path] = (),
    reviewed_mapping_path: Path | None = None,
    review_queue_path: Path | None = None,
) -> dict[str, Any]:
    source = GitSource(repository, revision)
    if revision == PIN and source.commit != PIN:
        raise ValueError(f"pinned source resolved to {source.commit}, expected {PIN}")
    builder = InventoryBuilder(source, cpp_root, plan_file, expected_operation_count=expected_operation_count)
    disposition_document: Mapping[str, Any] | None = None
    if dispositions_path is not None:
        disposition_document = json.loads(dispositions_path.read_text(encoding="utf-8"))
    rows, unresolved, summary = builder.build(disposition_document)
    write_tsv(ledger_path, LEDGER_COLUMNS, rows)
    write_tsv(unresolved_path, UNRESOLVED_COLUMNS, unresolved)
    reconciliation_path = summary_path.parent / "operation-catalog-reconciliation.tsv"
    reconciliation_rows = builder._operation_reconciliation_rows()
    validate_generated_table(reconciliation_rows, OPERATION_RECONCILIATION_COLUMNS, "operation_id")
    write_tsv(reconciliation_path, OPERATION_RECONCILIATION_COLUMNS, reconciliation_rows)
    scope_path = summary_path.parent / "unmatched-capability-scope.tsv"
    scope_rows = builder._capability_scope_rows(rows)
    validate_generated_table(scope_rows, ROUTE_SCOPE_COLUMNS, "python_route")
    write_tsv(scope_path, ROUTE_SCOPE_COLUMNS, scope_rows)
    if reviewed_mapping_path is None or review_queue_path is None:
        evidence_root = summary_path.parent
        reviewed_mapping_path = reviewed_mapping_path or evidence_root / "reviewed-mapping-input.json"
        review_queue_path = review_queue_path or evidence_root / "primary-review-queue.tsv"
    reviewed_mapping: Mapping[str, Any] | None = None
    if candidate_paths:
        reviewed_mapping = build_reviewed_mapping_input(candidate_paths, rows, pin=source.commit)
        validate_reviewed_mapping_input(reviewed_mapping, rows, source.commit)
        reviewed_mapping_path.parent.mkdir(parents=True, exist_ok=True)
        reviewed_mapping_path.write_text(json.dumps(reviewed_mapping, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    elif reviewed_mapping_path.is_file():
        reviewed_mapping = json.loads(reviewed_mapping_path.read_text(encoding="utf-8"))
        validate_reviewed_mapping_input(reviewed_mapping, rows, source.commit)
    if reviewed_mapping is not None:
        review_queue = build_primary_review_queue(rows, reviewed_mapping, builder)
        write_tsv(review_queue_path, REVIEW_QUEUE_COLUMNS, review_queue)
        summary["reviewed_mapping_input"] = {
            "path": str(reviewed_mapping_path.relative_to(builder.project_root)) if reviewed_mapping_path.is_relative_to(builder.project_root) else str(reviewed_mapping_path),
            "sha256": sha256(reviewed_mapping_path.read_bytes()),
            "candidate_source_count": len(reviewed_mapping.get("candidate_sources", [])),
            "route_count": reviewed_mapping.get("route_count", 0),
            "candidate_mapping_count": reviewed_mapping.get("candidate_mapping_count", 0),
            "unmatched_candidate_route_count": reviewed_mapping.get("unmatched_candidate_route_count", 0),
            "expanded_candidate_route_family_count": len(reviewed_mapping.get("expanded_candidate_route_families", [])),
            "historical_acceptance_candidate_route_count": reviewed_mapping.get("historical_acceptance_candidate_route_count", 0),
            "proposed_acceptance_existing_candidate_count": reviewed_mapping.get("proposed_acceptance_existing_candidate_count", 0),
            "alias_route_count_separate": reviewed_mapping.get("distinct_alias_route_count", 0),
            "candidate_is_not_acceptance": True,
        }
        summary["primary_review_queue_artifact"] = str(review_queue_path.relative_to(builder.project_root)) if review_queue_path.is_relative_to(builder.project_root) else str(review_queue_path)
        summary["primary_review_queue_row_count"] = len(review_queue)
        summary["primary_review_queue_sha256"] = sha256(review_queue_path.read_bytes())
        mapping_by_canonical = {
            record["canonical_id"]: record
            for record in reviewed_mapping.get("routes", [])
            if record["record_kind"] == "capability"
        }
        scope_counts = Counter(item["scope_classification"] for item in scope_rows)
        unmatched_scope_counts = Counter()
        for item in scope_rows:
            candidate = mapping_by_canonical.get(item["canonical_id"])
            proposal = candidate.get("candidate_review", {}).get("proposed_disposition") if candidate else "GAP"
            if candidate is None or proposal == "GAP":
                unmatched_scope_counts[item["scope_classification"]] += 1
        summary["unmatched_route_reconciliation_by_scope"] = {
            "gap_capability_routes_before_candidate_mapping": dict(sorted(scope_counts.items())),
            "unmatched_or_partial_after_candidate_mapping": dict(sorted(unmatched_scope_counts.items())),
            "selected_scope_unmatched_routes_after_candidate_mapping": unmatched_scope_counts.get("EXPLICIT_SELECTED_PLAN_SCOPE_UNMATCHED", 0),
            "module_rows_excluded": True,
            "alias_rows_excluded": True,
        }
        queue_counts = Counter(item["queue_kind"] for item in review_queue)
        summary["primary_review_queue_counts"] = dict(sorted(queue_counts.items()))
    if dispositions_path is not None:
        summary["disposition_input"] = {
            "path": str(dispositions_path),
            "sha256": sha256(dispositions_path.read_bytes()),
            "explicit_route_count": len(disposition_document.get("routes", [])) if disposition_document else 0,
            "primary_reviewed": disposition_document.get("primary_reviewed", False) if disposition_document else False,
            "reviewed_by": disposition_document.get("reviewed_by", "") if disposition_document else "",
            "review_date": disposition_document.get("review_date", "") if disposition_document else "",
        }
    summary["generator_sha256"] = sha256(Path(__file__).read_bytes())
    summary["ledger_sha256"] = sha256(ledger_path.read_bytes())
    summary["unresolved_sha256"] = sha256(unresolved_path.read_bytes())
    summary["operation_reconciliation_sha256"] = sha256(reconciliation_path.read_bytes())
    summary["unmatched_capability_scope_sha256"] = sha256(scope_path.read_bytes())
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return summary


def default_paths() -> tuple[Path, Path, Path, Path, Path]:
    repository_root = Path(__file__).resolve().parents[1]
    python_repository = repository_root.parent / "fincore"
    evidence_root = repository_root / "docs/evidence/revised-plan-20261023/capability-inventory"
    return (
        python_repository,
        repository_root / "docs/iterations/capability-ledger.tsv",
        evidence_root / "unresolved.tsv",
        evidence_root / "summary.json",
        repository_root / "docs/plans/2026-10-23-revised-plan-completion.md",
    )


def default_evidence_paths() -> tuple[Path, Path, Path]:
    repository_root = Path(__file__).resolve().parents[1]
    evidence_root = repository_root / "docs/evidence/revised-plan-20261023/capability-inventory"
    return (
        evidence_root / "reviewed-mapping-input.json",
        evidence_root / "primary-review-queue.tsv",
        repository_root / "docs/iterations/capability-dispositions.json",
    )


def main(argv: Sequence[str] | None = None) -> int:
    defaults = default_paths()
    repository_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--python-repository", type=Path, default=defaults[0], help="Git repository containing the pinned fincore source")
    parser.add_argument("--pin", default=PIN, help="Git commit containing the Python source inventory")
    parser.add_argument("--cpp-root", type=Path, default=repository_root / "include/fincore", help="Native public header root used only for mapping hints")
    parser.add_argument("--plan-file", type=Path, default=defaults[4], help="Revised plan from which selected consumer scenarios are inventoried")
    parser.add_argument("--output", type=Path, default=defaults[1], help="Master capability TSV path")
    parser.add_argument("--unresolved-output", type=Path, default=defaults[2], help="Actionable unresolved TSV path")
    parser.add_argument("--summary-output", type=Path, default=defaults[3], help="Deterministic JSON summary path")
    parser.add_argument("--expected-operations", type=int, default=EXPECTED_OPERATION_COUNT)
    reviewed_mapping_default, review_queue_default, _ = default_evidence_paths()
    parser.add_argument(
        "--import-candidate",
        type=Path,
        action="append",
        default=[],
        help="Read-only candidate JSON to normalize into the durable route-level review input (repeatable)",
    )
    parser.add_argument(
        "--reviewed-mapping-input",
        type=Path,
        default=reviewed_mapping_default,
        help="Normalized route-level candidate mapping input used for the primary review queue",
    )
    parser.add_argument("--review-queue-output", type=Path, default=review_queue_default)
    parser.add_argument(
        "--dispositions-file",
        type=Path,
        help="Optional exact-route disposition/evidence overlay; omitted by default so inventory remains unaccepted",
    )
    args = parser.parse_args(argv)
    summary = generate(
        repository=args.python_repository,
        revision=args.pin,
        cpp_root=args.cpp_root,
        plan_file=args.plan_file,
        ledger_path=args.output,
        unresolved_path=args.unresolved_output,
        summary_path=args.summary_output,
        expected_operation_count=args.expected_operations,
        dispositions_path=args.dispositions_file,
        candidate_paths=args.import_candidate,
        reviewed_mapping_path=args.reviewed_mapping_input,
        review_queue_path=args.review_queue_output,
    )
    print(json.dumps(summary, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
