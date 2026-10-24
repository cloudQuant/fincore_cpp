#!/usr/bin/env python3
"""Recheck new-pin survival for Iteration 8 NOT_RUN route rows.

The reviewed ledger was built at 4b8f31b; Iteration 8 pins 3e9ef20. This tool
resolves each fincore.* route to its module/symbol at both commits, compares a
normalized signature and body hash, and writes per-route evidence. It does not
mutate the ledger.
"""

from __future__ import annotations

import argparse
import ast
import csv
import hashlib
import json
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LEDGER = ROOT / "docs/iterations/capability-dispositions.json"
DEFAULT_OUT = ROOT / "docs/evidence/iteration-8/p100/new-pin-survival.json"
OLD_PIN = "4b8f31b1b59707480d5db9603704d83bbf7e6946"
NEW_PIN = "3e9ef205ef2023f92e89fddd1052d46b16429c64"
DEFAULT_ORACLE = Path("/Users/yunjinqi/Documents/new_projects/fincore")


def sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def load_routes(path: Path) -> list[dict]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    return [row for row in payload["routes"] if row["new_pin_survival_status"] == "NOT_RUN"]


def git_show(oracle: Path, commit: str, path: str) -> str | None:
    result = subprocess.run(
        ["git", "-C", str(oracle), "show", f"{commit}:{path}"],
        capture_output=True,
        text=True,
    )
    return result.stdout if result.returncode == 0 else None


def tracked_files(oracle: Path) -> set[str]:
    result = subprocess.check_output(
        ["git", "-C", str(oracle), "ls-tree", "-r", "--name-only", NEW_PIN, "--", "fincore"],
        text=True,
    )
    return {line.strip() for line in result.splitlines() if line.strip().endswith(".py")}


def resolve_route(route: str, files: set[str]) -> tuple[str, list[str]] | None:
    parts = route.split(".")[1:]  # drop fincore
    if not parts:
        return None
    for length in range(len(parts), 0, -1):
        candidate = "fincore/" + "/".join(parts[:length]) + ".py"
        if candidate in files:
            return candidate, parts[length:]
    if route == "fincore.__version__":
        return "fincore/__init__.py", ["__version__"]
    return None


def function_signature(node: ast.FunctionDef | ast.AsyncFunctionDef) -> dict:
    return {
        "kind": "function",
        "name": node.name,
        "async": isinstance(node, ast.AsyncFunctionDef),
        "args": [
            (arg.arg, ast.unparse(default) if default is not None else None)
            for arg, default in zip(
                node.args.posonlyargs + node.args.args + node.args.kwonlyargs,
                [None] * (len(node.args.posonlyargs) + len(node.args.args) - len(node.args.defaults))
                + list(node.args.defaults)
                + [None] * len(node.args.kwonlyargs),
            )
        ],
        "returns": ast.unparse(node.returns) if node.returns is not None else None,
        "decorators": [ast.unparse(item) for item in node.decorator_list],
    }


def find_symbol(tree: ast.Module, parts: list[str]) -> ast.AST | None:
    scope: list[ast.stmt] = tree.body
    node: ast.AST | None = None
    for part in parts:
        found = None
        for statement in scope:
            if isinstance(statement, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)) and statement.name == part:
                found = statement
                break
            if isinstance(statement, (ast.Assign, ast.AnnAssign)):
                targets = statement.targets if isinstance(statement, ast.Assign) else [statement.target]
                if any(isinstance(target, ast.Name) and target.id == part for target in targets):
                    found = statement
                    break
        if found is None:
            return None
        node = found
        scope = found.body if isinstance(found, ast.ClassDef) else []
    return node


def node_identity(node: ast.AST) -> tuple[dict, str]:
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
        return function_signature(node), ast.dump(ast.Module(body=node.body, type_ignores=[]), include_attributes=False)
    if isinstance(node, ast.ClassDef):
        methods = [
            function_signature(item)
            for item in node.body
            if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef)) and not item.name.startswith("_")
        ]
        constants = [
            target.id
            for item in node.body
            if isinstance(item, (ast.Assign, ast.AnnAssign))
            for target in (item.targets if isinstance(item, ast.Assign) else [item.target])
            if isinstance(target, ast.Name) and not target.id.startswith("_")
        ]
        signature = {
            "kind": "class",
            "name": node.name,
            "bases": [ast.unparse(base) for base in node.bases],
            "methods": methods,
            "constants": constants,
        }
        body = ast.dump(ast.Module(body=node.body, type_ignores=[]), include_attributes=False)
        return signature, body
    if isinstance(node, (ast.Assign, ast.AnnAssign)):
        value = node.value
        name = "unknown"
        targets = node.targets if isinstance(node, ast.Assign) else [node.target]
        for target in targets:
            if isinstance(target, ast.Name):
                name = target.id
        signature = {"kind": "value", "name": name, "value": ast.unparse(value) if value else None}
        return signature, ast.dump(node, include_attributes=False)
    return {"kind": type(node).__name__}, ast.dump(node, include_attributes=False)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ledger", type=Path, default=DEFAULT_LEDGER)
    parser.add_argument("--oracle", type=Path, default=DEFAULT_ORACLE)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    rows = load_routes(args.ledger)
    files = tracked_files(args.oracle)
    results = []
    counts: Counter[str] = Counter()
    for row in rows:
        route = row["python_route"]
        if route.startswith("revised-plan-scenario:"):
            results.append(
                {
                    "python_route": route,
                    "verdict": "PLAN_DERIVED_SCENARIO",
                    "evidence": "scenario-text-preservation.json exact_scenario_texts_unchanged=10",
                }
            )
            counts["PLAN_DERIVED_SCENARIO"] += 1
            continue
        resolved = resolve_route(route, files)
        if resolved is None:
            results.append({"python_route": route, "verdict": "UNRESOLVED_MODULE"})
            counts["UNRESOLVED_MODULE"] += 1
            continue
        path, symbol_parts = resolved
        new_text = git_show(args.oracle, NEW_PIN, path)
        old_text = git_show(args.oracle, OLD_PIN, path)
        if new_text is None:
            results.append({"python_route": route, "module": path, "verdict": "MODULE_MISSING"})
            counts["MODULE_MISSING"] += 1
            continue
        if old_text is None:
            results.append({"python_route": route, "module": path, "verdict": "OLD_MODULE_MISSING"})
            counts["OLD_MODULE_MISSING"] += 1
            continue
        if not symbol_parts:
            results.append({"python_route": route, "module": path, "verdict": "MODULE_ROUTE", "evidence": sha(new_text)})
            counts["MODULE_ROUTE"] += 1
            continue
        old_node = find_symbol(ast.parse(old_text), symbol_parts)
        new_node = find_symbol(ast.parse(new_text), symbol_parts)
        if new_node is None:
            results.append({"python_route": route, "module": path, "symbol": ".".join(symbol_parts), "verdict": "MISSING"})
            counts["MISSING"] += 1
            continue
        if old_node is None:
            results.append({"python_route": route, "module": path, "symbol": ".".join(symbol_parts), "verdict": "OLD_MISSING"})
            counts["OLD_MISSING"] += 1
            continue
        old_signature, old_body = node_identity(old_node)
        new_signature, new_body = node_identity(new_node)
        if old_signature != new_signature:
            verdict = "SIGNATURE_CHANGED"
        elif old_body != new_body:
            verdict = "SURVIVED_BODY_CHANGED"
        else:
            verdict = "SURVIVED_IDENTICAL"
        counts[verdict] += 1
        results.append(
            {
                "python_route": route,
                "module": path,
                "symbol": ".".join(symbol_parts),
                "verdict": verdict,
                "old_body_sha256": sha(old_body),
                "new_body_sha256": sha(new_body),
            }
        )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema_version": "1",
        "old_pin": OLD_PIN,
        "new_pin": NEW_PIN,
        "route_rows_checked": len(rows),
        "verdicts": dict(sorted(counts.items())),
        "rows": results,
    }
    args.out.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(payload["verdicts"], sort_keys=True))
    print(f"wrote {args.out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
