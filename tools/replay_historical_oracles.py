#!/usr/bin/env python3
"""Prepare or run bounded paired replays of archived Python oracle scripts.

The default command is an inspection-only pass. Oracle code runs only when
``--execute`` is supplied. Each distinct script hash is run once per pinned
revision; all archived phase/path mappings are retained in the inventory.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
import signal
import shutil
import subprocess
import tempfile
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Sequence


OLD_COMMIT = "2354bb560788f372502151a8b1b53accf536a6c0"
NEW_COMMIT = "4b8f31b1b59707480d5db9603704d83bbf7e6946"
DEFAULT_CONDA = "/Users/yunjinqi/opt/anaconda3/bin/conda"
WRAPPER = r'''import importlib.abc
import importlib.machinery
import importlib.metadata
import json
import locale
import os
import platform
import random
import runpy
import socket
import sys
import time
from pathlib import Path

source_root = Path(os.environ["FINCORE_SELECTED_SOURCE_ROOT"]).resolve()
case_root = Path(os.environ["FINCORE_CASE_ROOT"]).resolve()
script_path = Path(os.environ["FINCORE_ORACLE_SCRIPT"]).resolve()
environment_keys = json.loads(os.environ["FINCORE_SOURCE_ENV_KEYS_JSON"])

for key in environment_keys:
    os.environ[key] = str(source_root)
os.environ["FINCORE_PHASE38_PYTHON_SOURCE"] = str(source_root)
os.environ["FINCORE_PHASE39_PYTHON_SOURCE"] = str(source_root)

class FincoreSourceGuard(importlib.abc.MetaPathFinder):
    """Resolve every fincore import only from this selected checkout."""
    def find_spec(self, fullname, path=None, target=None):
        if fullname != "fincore" and not fullname.startswith("fincore."):
            return None
        search = [str(source_root)] if path is None else list(path)
        spec = importlib.machinery.PathFinder.find_spec(fullname, search, target)
        if spec is None:
            raise ModuleNotFoundError(
                f"{fullname} is absent from selected checkout {source_root}",
                name=fullname,
            )
        locations = list(spec.submodule_search_locations or ())
        candidates = ([spec.origin] if spec.origin else []) + locations
        for candidate in candidates:
            resolved = Path(candidate).resolve()
            if resolved != source_root and source_root not in resolved.parents:
                raise ImportError(
                    f"selected-source import escaped checkout: {fullname}: {resolved}"
                )
        return spec

sys.meta_path.insert(0, FincoreSourceGuard())
sys.path.insert(0, str(source_root))
random.seed(int(os.environ["FINCORE_PYTHON_RANDOM_SEED"]))

def block_network(*args, **kwargs):
    raise RuntimeError("network access disabled by paired-oracle replay wrapper")

socket.create_connection = block_network
socket.getaddrinfo = block_network
socket.socket.connect = block_network
socket.socket.connect_ex = block_network
socket.socket.bind = block_network
socket.socket.listen = block_network
socket.socket.sendto = block_network

def inside_case(value):
    try:
        path = Path(os.fsdecode(value)).resolve()
    except (TypeError, ValueError, OSError):
        return False
    return path == case_root or case_root in path.parents

def deny_mutation(event, args):
    if event in {"subprocess.Popen", "os.system", "os.exec", "os.posix_spawn"}:
        raise PermissionError(f"process execution disabled by paired-oracle replay wrapper: {event}")
    paths = {
        "os.remove": (0,), "os.rename": (0, 1), "os.replace": (0, 1),
        "os.mkdir": (0,), "os.rmdir": (0,), "os.chmod": (0,),
        "os.chown": (0,), "os.truncate": (0,), "os.link": (0, 1),
        "os.symlink": (0, 1), "os.utime": (0,), "os.mknod": (0,),
    }
    for index in paths.get(event, ()):
        if index < len(args) and not inside_case(args[index]):
            raise PermissionError(
                f"oracle mutation outside case scratch blocked: {event} {args[index]!r}"
            )
    if event == "open" and args:
        path = args[0]
        mode = args[1] if len(args) > 1 else "r"
        flags = args[2] if len(args) > 2 and isinstance(args[2], int) else 0
        writes = (
            isinstance(mode, str) and any(char in mode for char in "wax+")
        ) or bool(flags & (os.O_WRONLY | os.O_RDWR | os.O_CREAT | os.O_TRUNC | os.O_APPEND))
        if writes and not inside_case(path):
            raise PermissionError(
                f"oracle write outside case scratch blocked: {path!r}"
            )

sys.addaudithook(deny_mutation)

def installed_version(distribution):
    try:
        return importlib.metadata.version(distribution)
    except importlib.metadata.PackageNotFoundError:
        return None

if hasattr(time, "tzset"):
    time.tzset()
try:
    active_locale = locale.setlocale(locale.LC_ALL, "")
except locale.Error as exc:
    active_locale = f"ERROR:{exc}"

metadata = {
    "python": {
        "version": sys.version,
        "implementation": platform.python_implementation(),
        "executable": str(Path(sys.executable).resolve()),
        "platform": platform.platform(),
    },
    "libraries": {
        name: installed_version(name)
        for name in (
            "numpy", "pandas", "scipy", "backtrader", "pytest",
            "matplotlib", "statsmodels", "numba", "bottleneck",
            "scikit-learn", "polars", "pyarrow", "empyrical",
            "empyrical-reloaded", "pyfolio", "pyfolio-reloaded",
            "alphalens", "alphalens-reloaded",
        )
    },
    "determinism": {
        "LC_ALL": os.environ.get("LC_ALL"),
        "LANG": os.environ.get("LANG"),
        "locale": active_locale,
        "TZ": os.environ.get("TZ"),
        "timezone": list(time.tzname),
        "PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
        "python_random_seed": int(os.environ["FINCORE_PYTHON_RANDOM_SEED"]),
        "numpy_random_seed": int(os.environ["FINCORE_NUMPY_RANDOM_SEED"]),
        "thread_limits": {
            name: os.environ.get(name)
            for name in (
                "OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS",
                "NUMEXPR_NUM_THREADS", "VECLIB_MAXIMUM_THREADS",
            )
        },
    },
}
try:
    import numpy as np
    metadata["libraries"]["numpy_runtime"] = np.__version__
    np.random.seed(int(os.environ["FINCORE_NUMPY_RANDOM_SEED"]))
except Exception as exc:
    metadata["numpy_import_error"] = f"{type(exc).__name__}: {exc}"

def emit_import_state():
    modules = {}
    for name, module in sorted(sys.modules.items()):
        if name == "fincore" or name.startswith("fincore."):
            module_file = getattr(module, "__file__", None)
            module_path = getattr(module, "__path__", None)
            modules[name] = {
                "file": str(Path(module_file).resolve()) if module_file else None,
                "path": [str(Path(item).resolve()) for item in module_path]
                    if module_path else [],
            }
    imported_paths = []
    for module in modules.values():
        imported_paths.extend(item for item in [module["file"], *module["path"]] if item)
    def within_selected_root(path):
        try:
            return Path(path).resolve().is_relative_to(source_root)
        except (OSError, TypeError, ValueError):
            return False
    selected_ok = bool(modules) and all(within_selected_root(path) for path in imported_paths)
    payload = {
        "source_root": str(source_root),
        "script_path": str(script_path),
        "metadata": metadata,
        "fincore_modules": modules,
        "fincore_module_imported": bool(modules),
        "selected_imports_only": selected_ok,
    }
    state_path = case_root / "_wrapper-import-state.json"
    state_path.write_text(json.dumps(payload, sort_keys=True), encoding="utf-8")
    if not selected_ok:
        raise RuntimeError("one or more fincore modules resolved outside selected checkout")

try:
    runpy.run_path(str(script_path), run_name="__main__")
finally:
    emit_import_state()
'''


@dataclass(frozen=True)
class Revision:
    name: str
    commit: str


REVISIONS = (Revision("old", OLD_COMMIT), Revision("new", NEW_COMMIT))


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_binary(
    argv: Sequence[str], *, cwd: Path, env: dict[str, str] | None = None,
    timeout: float | None = None,
) -> subprocess.CompletedProcess[bytes]:
    process = subprocess.Popen(
        list(argv), cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            stdout, stderr = process.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            stdout, stderr = process.communicate()
        raise subprocess.TimeoutExpired(
            argv, timeout, output=stdout or exc.output, stderr=stderr or exc.stderr
        )
    return subprocess.CompletedProcess(list(argv), process.returncode, stdout, stderr)


def git_text(repo: Path, *args: str) -> str:
    result = run_binary(("git", "-C", str(repo), *args), cwd=repo)
    if result.returncode != 0:
        raise RuntimeError(
            f"git {' '.join(args)} failed in {repo}: "
            f"{result.stderr.decode('utf-8', 'replace')}"
        )
    return result.stdout.decode("utf-8", "replace").strip()


def verify_archive(historical_root: Path) -> None:
    sums = historical_root / "SHA256SUMS.txt"
    if not sums.is_file():
        raise RuntimeError(f"historical checksum manifest missing: {sums}")
    for line in sums.read_text(encoding="utf-8").splitlines():
        expected, relative = line.split("  ", 1)
        path = historical_root / relative
        if not path.is_file() or sha256_file(path) != expected:
            raise RuntimeError(f"historical artifact checksum mismatch: {relative}")


def source_env_keys(script_bytes: bytes) -> list[str]:
    """Return literal FINCORE_*_PYTHON_SOURCE environment keys read by a script."""
    try:
        tree = ast.parse(script_bytes.decode("utf-8"))
    except (SyntaxError, UnicodeDecodeError):
        return []
    found: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Subscript):
            base = node.value
            if isinstance(base, ast.Attribute) and base.attr == "environ":
                key_node = node.slice
            else:
                continue
            if isinstance(key_node, ast.Constant) and isinstance(key_node.value, str):
                key = key_node.value
                if key.startswith("FINCORE_") and key.endswith("_PYTHON_SOURCE"):
                    found.add(key)
        elif isinstance(node, ast.Call):
            func = node.func
            is_get = (
                isinstance(func, ast.Attribute)
                and func.attr == "get"
                and isinstance(func.value, ast.Attribute)
                and func.value.attr == "environ"
            ) or (
                isinstance(func, ast.Attribute)
                and func.attr == "getenv"
                and isinstance(func.value, ast.Name)
                and func.value.id == "os"
            )
            if is_get and node.args and isinstance(node.args[0], ast.Constant):
                key = node.args[0].value
                if isinstance(key, str) and key.startswith("FINCORE_") and key.endswith("_PYTHON_SOURCE"):
                    found.add(key)
    return sorted(found)


def static_review(script_bytes: bytes) -> dict[str, Any]:
    """Conservative AST inventory; it does not execute any archived source."""
    text = script_bytes.decode("utf-8", "replace")
    try:
        tree = ast.parse(text)
    except SyntaxError as exc:
        return {
            "parse_status": "SYNTAX_ERROR",
            "syntax_error": f"{exc.msg} at line {exc.lineno}",
            "imports": [], "absolute_path_literals": [], "sensitive_calls": [],
        }
    imports: set[str] = set()
    absolute_paths: set[str] = set()
    sensitive_calls: set[str] = set()
    network_modules = {
        "socket", "urllib", "http", "requests", "aiohttp", "httpx",
        "websocket", "ftplib", "smtplib",
    }
    process_modules = {"subprocess", "multiprocessing", "pty", "pexpect"}
    write_call_names = {
        "open", "write_text", "write_bytes", "to_csv", "to_excel",
        "to_parquet", "to_pickle", "unlink", "mkdir", "rmdir", "remove",
        "rename", "copyfile", "move",
    }
    network_call_names = {
        "connect", "connect_ex", "create_connection", "urlopen", "getaddrinfo",
        "request", "get", "post", "put", "delete",
    }
    network_prefixes = (
        "socket.", "urllib.", "http.", "requests.", "aiohttp.", "httpx.",
        "websocket.", "ftplib.", "smtplib.",
    )
    process_call_names = {"Popen", "run", "call", "check_call", "check_output", "system"}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                root = alias.name.split(".", 1)[0]
                imports.add(alias.name)
                if root in network_modules or root in process_modules:
                    sensitive_calls.add(f"import:{alias.name}")
        elif isinstance(node, ast.ImportFrom):
            module = node.module or ""
            imports.add(module)
            root = module.split(".", 1)[0]
            if root in network_modules or root in process_modules:
                sensitive_calls.add(f"import:{module}")
        elif isinstance(node, ast.Constant) and isinstance(node.value, str):
            literal = node.value
            if literal.startswith("/"):
                absolute_paths.add(literal)
        elif isinstance(node, ast.Call):
            name = node.func.attr if isinstance(node.func, ast.Attribute) else (
                node.func.id if isinstance(node.func, ast.Name) else ""
            )
            qualified = ast.unparse(node.func) if hasattr(ast, "unparse") else name
            if name in write_call_names:
                if name == "open":
                    mode = node.args[1] if len(node.args) > 1 else None
                    if isinstance(mode, ast.Constant) and isinstance(mode.value, str):
                        if any(char in mode.value for char in "wax+"):
                            sensitive_calls.add(f"possible-write:{qualified}")
                    elif mode is not None:
                        sensitive_calls.add(f"mode-dependent-write:{qualified}")
                elif name.startswith("to_"):
                    file_arg = node.args[0] if node.args else None
                    if file_arg is None:
                        file_arg = next((
                            keyword.value for keyword in node.keywords
                            if keyword.arg in {"path_or_buf", "excel_writer", "path"}
                        ), None)
                    if file_arg is None or isinstance(file_arg, ast.Constant) and file_arg.value is None:
                        pass  # In-memory serialization (for example, Series.to_csv()).
                    else:
                        sensitive_calls.add(f"possible-write:{qualified}")
                else:
                    sensitive_calls.add(f"possible-write:{qualified}")
            if name in network_call_names and qualified.startswith(network_prefixes):
                sensitive_calls.add(f"possible-network:{qualified}")
            if name in process_call_names and (
                qualified.startswith("subprocess.") or qualified in {"os.system", "system"}
            ):
                sensitive_calls.add(f"possible-process:{qualified}")
    return {
        "parse_status": "OK",
        "imports": sorted(imports),
        "source_environment_keys": source_env_keys(script_bytes),
        "absolute_path_literals": sorted(absolute_paths),
        "sensitive_calls": sorted(sensitive_calls),
        "source_relative_setup": (
            "Path(__file__).resolve().parent / 'source' plus sys.path insertion"
            if "source" in {node.value for node in ast.walk(tree)
                            if isinstance(node, ast.Constant) and isinstance(node.value, str)}
            and any(
                isinstance(node, ast.Attribute) and node.attr == "__file__"
                for node in ast.walk(tree)
            )
            and any(
                isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
                and node.func.attr == "insert" and ast.unparse(node.func).startswith("sys.path.")
                for node in ast.walk(tree)
            )
            else None
        ),
    }


def archive_records(historical_root: Path) -> dict[str, dict[str, Any]]:
    manifest = json.loads((historical_root / "artifact-manifest.json").read_text(encoding="utf-8"))
    return {
        record["retained_path"]: record
        for record in manifest.get("artifacts", [])
        if record.get("retained_path")
    }


def discover_scripts(historical_root: Path) -> list[dict[str, Any]]:
    run_root = historical_root / "phase-runs"
    records = archive_records(historical_root)
    provenance_doc = json.loads(
        (historical_root / "source-provenance.json").read_text(encoding="utf-8")
    )
    provenance_records = provenance_doc.get("sources", {})
    by_hash: dict[str, dict[str, Any]] = {}
    for path in sorted(run_root.rglob("*.py")):
        if "oracle" not in path.name.lower():
            continue
        data = path.read_bytes()
        digest = sha256_bytes(data)
        relative = path.relative_to(historical_root).as_posix()
        entry = by_hash.setdefault(digest, {
            "script_sha256": digest,
            "script_bytes": len(data),
            "canonical_retained_path": relative,
            "mappings": [],
            "static_review": static_review(data),
        })
        artifact = records.get(relative, {})
        provenance = provenance_records.get(artifact.get("source_provenance_id"), {})
        parent = path.parent
        sibling_records = [
            record for record in records.values()
            if Path(record["retained_path"]).parent == Path(relative).parent
        ]
        roles = {record.get("role"): record for record in sibling_records}
        capture_stem = path.stem[:-len("_oracle")] if path.stem.endswith("_oracle") else path.stem
        capture_paths = {
            record["role"]: record["retained_path"]
            for record in sibling_records
            if record.get("role") in {"stdout_capture", "stderr_capture"}
            and Path(record["retained_path"]).name in {
                f"{capture_stem}.raw.txt", f"{capture_stem}.stderr.txt",
            }
        }
        entry["mappings"].append({
            "phase_or_group": path.relative_to(run_root).parts[0],
            "retained_path": relative,
            "original_source_path": artifact.get("original_source_path"),
            "historical_source_provenance_id": artifact.get("source_provenance_id"),
            "historical_source_git_head": provenance.get("head", "NOT_RECORDED"),
            "historical_source_git_status_state": provenance.get("status_state", "NOT_RECORDED"),
            "historical_source_git_status_porcelain": provenance.get("status_porcelain", "NOT_RECORDED"),
            "historical_capture": "HISTORICAL_CAPTURE"
                if "stdout_capture" in capture_paths and "stderr_capture" in capture_paths
                else "SCRIPT_ONLY_NO_CAPTURE_RECORD",
            "historical_stdout_path": capture_paths.get("stdout_capture"),
            "historical_stderr_path": capture_paths.get("stderr_capture"),
            "historical_status_note": "directory-level capture; association to this exact script is not inferred"
                if "status_capture" in roles else None,
            "historical_status_before_path": roles.get("status_capture", {}).get("retained_path"),
        })
    cases = sorted(by_hash.values(), key=lambda item: item["script_sha256"])
    for case in cases:
        case["mappings"].sort(key=lambda item: (item["phase_or_group"], item["retained_path"]))
    return cases


def summarize_inspection(cases: list[dict[str, Any]]) -> dict[str, Any]:
    phase_rows: dict[str, dict[str, Any]] = {}
    for case in cases:
        for mapping in case["mappings"]:
            phase = mapping["phase_or_group"]
            row = phase_rows.setdefault(phase, {
                "phase_or_group": phase,
                "archive_scripts": 0,
                "distinct_script_hashes": set(),
                "historical_captures": 0,
                "script_only": 0,
            })
            row["archive_scripts"] += 1
            row["distinct_script_hashes"].add(case["script_sha256"])
            if mapping["historical_capture"] == "HISTORICAL_CAPTURE":
                row["historical_captures"] += 1
            else:
                row["script_only"] += 1
    return {
        "distinct_scripts": len(cases),
        "archive_script_mappings": sum(len(case["mappings"]) for case in cases),
        "phases_or_groups": [
            {**row, "distinct_script_hashes": len(row["distinct_script_hashes"])}
            for _, row in sorted(phase_rows.items())
        ],
    }


def prepare_inventory(historical_root: Path, output_dir: Path) -> list[dict[str, Any]]:
    verify_archive(historical_root)
    cases = discover_scripts(historical_root)
    output_dir.mkdir(parents=True, exist_ok=False)
    summary = summarize_inspection(cases)
    write_json(output_dir / "script-inventory.json", {
        "evidence_class": "static_preflight_only; no archived oracle executed",
        "runner_path": str(Path(__file__).resolve()),
        "runner_sha256": sha256_file(Path(__file__).resolve()),
        "wrapper_sha256": sha256_bytes(WRAPPER.encode("utf-8")),
        "historical_archive_root": str(historical_root.resolve()),
        "selected_scope": "historical-oracles/phase-runs/**/*.py with oracle in filename",
        "deduplication_key": "exact SHA-256 of preserved oracle script bytes",
        "execution_targets": {revision.name: revision.commit for revision in REVISIONS},
        "summary": summary,
        "scripts": cases,
    })
    lines = [
        "# Paired replay static preflight",
        "",
        "This is an inspection artifact only. No archived oracle was executed.",
        "The inventory is hash-deduplicated while retaining every phase/path mapping.",
        "Historical output/status files remain in `docs/evidence/revised-plan-20261023/historical-oracles/phase-runs/`.",
        "Historical commands and runtime environments remain unknown unless recorded in the source archive.",
        "",
        f"- Distinct script hashes: {summary['distinct_scripts']}",
        f"- Archived script mappings: {summary['archive_script_mappings']}",
        f"- Phase/group labels: {len(summary['phases_or_groups'])}",
        "- Target commits: old `" + OLD_COMMIT + "`; new `" + NEW_COMMIT + "`.",
        "- Runtime environment: Anaconda base only; deterministic locale/timezone/seeds/threads.",
        "- Execution guard: mandatory `--execute`; default invocation is inspect-only.",
        "- Script runner: isolated scratch clone, strict `fincore` import finder, network block, and write audit hook.",
        "- Comparison: clone-root path replacement only; no numeric normalization and no historical-golden substitution.",
        "",
        "## Phase/group inventory",
        "",
        "| Phase/group | Script mappings | Distinct hashes | Historical captures | Script only |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for row in summary["phases_or_groups"]:
        lines.append(
            f"| `{row['phase_or_group']}` | {row['archive_scripts']} | "
            f"{row['distinct_script_hashes']} | {row['historical_captures']} | {row['script_only']} |"
        )
    lines.extend([
        "",
        "## Static review limits",
        "",
        "The JSON records literal imports, slash-prefixed string literals, selected-source environment keys, source-relative setup, and candidate network/process/write calls from AST inspection. These are leads for manual review, not a proof that dynamic behavior is absent. At replay time the wrapper blocks network sockets and rejects file mutation outside the per-case scratch directory. Scripts that require unsupported external behavior must be recorded as `NOT_RUN` with the exact reason.",
        "",
        "The replay compares only paired outputs from these exact old/new commits. Matching nonzero exits remain failures; matching output is recorded as output equality, never as broad parity. This evidence does not cover all Python source or all capability-map rows.",
    ])
    (output_dir / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    parse_failures = [case for case in cases if case["static_review"]["parse_status"] != "OK"]
    sensitive_imports = [
        (case["script_sha256"], call)
        for case in cases for call in case["static_review"]["sensitive_calls"]
        if call.startswith("import:")
    ]
    candidate_calls = [
        (case["script_sha256"], call)
        for case in cases for call in case["static_review"]["sensitive_calls"]
        if not call.startswith("import:")
    ]
    slash_literals = [
        (mapping["retained_path"], case["static_review"]["absolute_path_literals"])
        for case in cases if case["static_review"]["absolute_path_literals"]
        for mapping in case["mappings"]
    ]
    source_relative = [
        mapping["retained_path"]
        for case in cases if case["static_review"].get("source_relative_setup")
        for mapping in case["mappings"]
    ]
    review = [
        "# Archived oracle script review before replay", "",
        "This review is static and read-only. It does not execute archived Python.",
        "Each retained `phase-runs` script is preserved byte-for-byte and is reviewed by exact SHA-256 in `script-inventory.json`.",
        "",
        f"- Scripts parsed by the Python AST: {len(cases) - len(parse_failures)} / {len(cases)}.",
        f"- AST parse failures: {len(parse_failures)}.",
        f"- Network/process module imports: {len(sensitive_imports)}.",
        f"- Candidate calls requiring manual inspection: {len(candidate_calls)}.",
        f"- Scripts with source-relative `Path(__file__) / 'source'` setup: {len(source_relative)}.",
        "- Historical command and environment: remain `NOT_RECORDED` when absent from archive; no values were inferred.",
        "",
        "## Findings",
        "",
        "No network or subprocess modules were found by AST import scan. No archive script has a literal absolute filesystem path; the slash-prefixed literals listed below are fixture or output-label fragments. No network/process/write candidate call remains after narrowing serialization and dictionary lookup false positives. Runtime guards still block sockets, process creation, and writes outside case scratch.",
        "",
        "The source-relative scripts use `Path(__file__).resolve().parent / 'source'` and insert that directory into `sys.path`. The runner keeps both the staged script path and selected clone path stable between old/new runs, so these original scripts run unchanged and their own selected-source assertion remains meaningful.",
        "",
        "The `to_csv()` calls in Phase 63d and Phase 68 pass no output path and consume the returned CSV string for fixture hashing. Phase 67's `dataclasses.replace` and JSON serialization are in-memory; the Phase 80 event helper named `dump` converts DataFrames into dictionaries. `researchcmp` uses dictionary `.get()` only. These are not file/network side effects.",
        "",
        "Scripts without an adjacent historical stdout and stderr capture remain script-only: `nextopen`, `researchgroups`, and Phase 63b `oracle3.py`. Their current paired execution can be recorded, but no historical expected output is claimed for them.",
        "",
        "## Source-relative script paths",
        "",
    ]
    review.extend(f"- `{path}`" for path in sorted(source_relative))
    review.extend(["", "## Slash-prefixed fixture/output strings", ""])
    review.extend(
        f"- `{path}`: `{', '.join(values)}`"
        for path, values in sorted(slash_literals)
    )
    (output_dir / "manual-script-review.md").write_text("\n".join(review) + "\n", encoding="utf-8")
    write_hash_manifest(output_dir)
    return cases


def write_hash_manifest(root: Path) -> None:
    rows = [
        f"{sha256_file(path)}  {path.relative_to(root).as_posix()}"
        for path in sorted(root.rglob("*"))
        if path.is_file() and path.name != "SHA256SUMS.txt"
    ]
    (root / "SHA256SUMS.txt").write_text("\n".join(rows) + "\n", encoding="utf-8")


def clean_clone(source_repo: Path, scratch: Path, output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    source = scratch / "source"
    clone = run_binary(
        ("git", "clone", "--local", "--no-hardlinks", "--quiet", str(source_repo), str(source)),
        cwd=scratch,
    )
    (output_dir / "clone.stdout.bin").write_bytes(clone.stdout)
    (output_dir / "clone.stderr.bin").write_bytes(clone.stderr)
    if clone.returncode != 0:
        raise RuntimeError(f"clone failed: {clone.stderr.decode('utf-8', 'replace')}")
    return source


def checkout_revision(source: Path, revision: Revision, output_dir: Path) -> dict[str, Any]:
    status_before_checkout = status_snapshot(source)
    head_before = git_text(source, "rev-parse", "HEAD")
    if status_before_checkout:
        raise RuntimeError(
            f"refusing to switch dirty scratch clone for {revision.name}: {status_before_checkout!r}"
        )
    checkout_command = ("git", "-C", str(source), "checkout", "--quiet", "--detach", revision.commit)
    checkout = run_binary(checkout_command, cwd=source.parent)
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "checkout.stdout.bin").write_bytes(checkout.stdout)
    (output_dir / "checkout.stderr.bin").write_bytes(checkout.stderr)
    if checkout.returncode != 0:
        raise RuntimeError(f"checkout failed for {revision.name}: {checkout.stderr.decode('utf-8', 'replace')}")
    head = git_text(source, "rev-parse", "HEAD")
    branch = git_text(source, "branch", "--show-current")
    status = git_text(source, "status", "--porcelain", "--untracked-files=all")
    tree = git_text(source, "rev-parse", f"{head}^{{tree}}")
    if head != revision.commit or branch or status:
        raise RuntimeError(f"scratch checkout validation failed: head={head}, branch={branch!r}, status={status!r}")
    metadata = {
        "revision": revision.name,
        "head_before_checkout": head_before,
        "commit": head,
        "tree": tree,
        "detached": not branch,
        "status_before_checkout": status_before_checkout,
        "status_after_checkout": status,
        "checkout_argv": list(checkout_command),
        "scratch_source": str(source.resolve()),
    }
    write_json(output_dir / "checkout.json", metadata)
    return metadata


def deterministic_environment(source: Path, case_root: Path, source_keys: list[str]) -> dict[str, str]:
    # Pass no ambient application credentials or user Python paths to archived
    # code. Conda activates the requested base environment from its executable.
    env = {
        "PATH": os.environ.get("PATH", "/usr/bin:/bin"),
        "FINCORE_SELECTED_SOURCE_ROOT": str(source.resolve()),
        "FINCORE_CASE_ROOT": str(case_root.resolve()),
        "FINCORE_SOURCE_ENV_KEYS_JSON": json.dumps(source_keys),
        "FINCORE_PHASE38_PYTHON_SOURCE": str(source.resolve()),
        "FINCORE_PHASE39_PYTHON_SOURCE": str(source.resolve()),
        "FINCORE_PYTHON_RANDOM_SEED": "0",
        "FINCORE_NUMPY_RANDOM_SEED": "0",
        "PYTHONDONTWRITEBYTECODE": "1",
        "PYTHONHASHSEED": "0",
        "PYTHONNOUSERSITE": "1",
        "LC_ALL": "C",
        "LANG": "C",
        "TZ": "UTC",
        "OMP_NUM_THREADS": "1",
        "OPENBLAS_NUM_THREADS": "1",
        "MKL_NUM_THREADS": "1",
        "NUMEXPR_NUM_THREADS": "1",
        "VECLIB_MAXIMUM_THREADS": "1",
        "TMPDIR": str(case_root / "tmp"),
        "XDG_CACHE_HOME": str(case_root / "cache"),
        "XDG_CONFIG_HOME": str(case_root / "config"),
        "MPLCONFIGDIR": str(case_root / "matplotlib"),
        "CONDARC": "/dev/null",
        "CONDA_PKGS_DIRS": str(case_root / "conda-pkgs"),
    }
    for key in source_keys:
        env[key] = str(source.resolve())
    return env


def safe_environment_record(env: dict[str, str]) -> dict[str, str]:
    keys = (
        "FINCORE_SELECTED_SOURCE_ROOT", "FINCORE_CASE_ROOT", "FINCORE_SOURCE_ENV_KEYS_JSON",
        "FINCORE_PHASE38_PYTHON_SOURCE", "FINCORE_PHASE39_PYTHON_SOURCE",
        "FINCORE_ORACLE_SCRIPT",
        "FINCORE_PYTHON_RANDOM_SEED", "FINCORE_NUMPY_RANDOM_SEED",
        "PYTHONDONTWRITEBYTECODE", "PYTHONHASHSEED", "PYTHONNOUSERSITE",
        "LC_ALL", "LANG", "TZ", "OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS",
        "MKL_NUM_THREADS", "NUMEXPR_NUM_THREADS", "VECLIB_MAXIMUM_THREADS",
        "TMPDIR", "XDG_CACHE_HOME", "XDG_CONFIG_HOME", "MPLCONFIGDIR",
        "CONDARC", "CONDA_PKGS_DIRS",
    )
    selected = {key: env[key] for key in keys if key in env}
    if "PATH" in env:
        selected["PATH"] = env["PATH"]
    selected.update({
        key: value for key, value in env.items()
        if key.startswith("FINCORE_") and key.endswith("_PYTHON_SOURCE")
    })
    return selected


def status_snapshot(source: Path) -> str:
    return git_text(source, "status", "--porcelain", "--untracked-files=all")


def run_case(
    *, case: dict[str, Any], revision: Revision, source: Path, revision_meta: dict[str, Any],
    historical_root: Path, output_case: Path, staged: Path, case_root: Path,
    conda: str, timeout: float,
) -> dict[str, Any]:
    output_case.mkdir(parents=True, exist_ok=True)
    if any(output_case.glob("*.raw.bin")):
        raise FileExistsError(f"refusing to overwrite case output: {output_case}")
    mappings = case["mappings"]
    source_path = historical_root / case["canonical_retained_path"]
    script_data = source_path.read_bytes()
    if sha256_bytes(script_data) != case["script_sha256"]:
        raise RuntimeError("preserved oracle bytes differ from inventoried hash")
    if sha256_file(staged) != case["script_sha256"]:
        raise RuntimeError("staged oracle hash differs from preserved script")

    case_root.mkdir(parents=True, exist_ok=False)
    (case_root / "tmp").mkdir()
    (case_root / "cache").mkdir()
    (case_root / "config").mkdir()
    (case_root / "matplotlib").mkdir()
    (case_root / "conda-pkgs").mkdir()
    status_before = status_snapshot(source)
    source_keys = case["static_review"].get("source_environment_keys", [])
    env = deterministic_environment(source, case_root, source_keys)
    env["FINCORE_ORACLE_SCRIPT"] = str(staged.resolve())
    command = [conda, "run", "--no-capture-output", "-n", "base", "python", "-B", "-c", WRAPPER]
    command_record = {
        "argv": command,
        "cwd": str(case_root.resolve()),
        "staged_script": str(staged.resolve()),
        "staged_script_sha256": case["script_sha256"],
        "phase_mappings": mappings,
        "environment": safe_environment_record(env),
        "timeout_seconds": timeout,
        "wrapper_sha256": sha256_bytes(WRAPPER.encode("utf-8")),
    }
    write_json(output_case / "command.json", command_record)
    started = time.perf_counter()
    timed_out = False
    try:
        result = run_binary(command, cwd=case_root, env=env, timeout=timeout)
        raw_stdout, raw_stderr = result.stdout, result.stderr
        returncode: int | None = result.returncode
        exit_state = "EXITED"
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        raw_stdout = exc.stdout or b""
        raw_stderr = exc.stderr or b""
        if isinstance(raw_stdout, str):
            raw_stdout = raw_stdout.encode("utf-8", "replace")
        if isinstance(raw_stderr, str):
            raw_stderr = raw_stderr.encode("utf-8", "replace")
        returncode = None
        exit_state = "TIMEOUT"
    elapsed = time.perf_counter() - started
    import_state_path = case_root / "_wrapper-import-state.json"
    if import_state_path.is_file():
        try:
            import_state = json.loads(import_state_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            import_state = {"capture_error": "invalid import-state JSON", "selected_imports_only": False}
        import_state_path.unlink()
    else:
        import_state = None
    # Only exact path bytes are replaced. Output bytes containing numbers,
    # ordering, formatting, and line endings are otherwise preserved.
    source_bytes = str(source.resolve()).encode("utf-8")
    normalized_stdout = raw_stdout.replace(source_bytes, b"<SOURCE_ROOT>")
    normalized_stderr = raw_stderr.replace(source_bytes, b"<SOURCE_ROOT>")
    (output_case / "stdout.raw.bin").write_bytes(raw_stdout)
    (output_case / "stderr.raw.bin").write_bytes(raw_stderr)
    (output_case / "stdout.normalized.bin").write_bytes(normalized_stdout)
    (output_case / "stderr.normalized.bin").write_bytes(normalized_stderr)
    (output_case / "stdout.sha256").write_text(sha256_bytes(raw_stdout) + "\n", encoding="ascii")
    (output_case / "stderr.sha256").write_text(sha256_bytes(raw_stderr) + "\n", encoding="ascii")
    (output_case / "exit-status.txt").write_text(
        f"{exit_state}\n" if returncode is None else f"{returncode}\n", encoding="utf-8"
    )
    write_json(output_case / "import-paths.json", import_state or {
        "capture_error": "wrapper import state was not emitted", "selected_imports_only": False,
    })
    status_after = status_snapshot(source)
    (output_case / "status-before.txt").write_text(status_before, encoding="utf-8")
    (output_case / "status-after.txt").write_text(status_after, encoding="utf-8")
    shutil.copytree(case_root, output_case / "scratch-artifacts")
    result_record = {
        "script_sha256": case["script_sha256"],
        "phase_mappings": [mapping["phase_or_group"] for mapping in mappings],
        "retained_paths": [mapping["retained_path"] for mapping in mappings],
        "revision": revision.name,
        "commit": revision_meta["commit"],
        "tree": revision_meta["tree"],
        "historical_evidence_class": "historical_capture" if any(
            mapping["historical_capture"] == "HISTORICAL_CAPTURE" for mapping in mappings
        ) else "script_only_no_historical_output",
        "historical_outputs_are_not_replaced_or_used_as_current_golden": True,
        "returncode": returncode,
        "exit_state": exit_state,
        "timeout": timed_out,
        "elapsed_seconds": elapsed,
        "stdout_sha256": sha256_bytes(raw_stdout),
        "stderr_sha256": sha256_bytes(raw_stderr),
        "status_before": status_before,
        "status_after": status_after,
        "scratch_source_clean_after": not bool(status_after),
        "import_state_present": import_state is not None,
        "selected_imports_only": bool(import_state and import_state.get("selected_imports_only")),
        "fincore_module_imported": bool(import_state and import_state.get("fincore_module_imported")),
        "outputs": {
            "stdout_raw": str((output_case / "stdout.raw.bin").name),
            "stderr_raw": str((output_case / "stderr.raw.bin").name),
            "stdout_normalized": str((output_case / "stdout.normalized.bin").name),
            "stderr_normalized": str((output_case / "stderr.normalized.bin").name),
        },
        "scratch_artifact_root": "scratch-artifacts",
    }
    write_json(output_case / "result.json", result_record)
    return result_record


def classify_pair(old: dict[str, Any], new: dict[str, Any], old_dir: Path, new_dir: Path) -> dict[str, Any]:
    required_outputs = [
        old_dir / "stdout.normalized.bin", old_dir / "stderr.normalized.bin",
        new_dir / "stdout.normalized.bin", new_dir / "stderr.normalized.bin",
    ]
    if not all(path.is_file() for path in required_outputs):
        return {
            "classification": "NOT_COMPARABLE",
            "reason": "one or both runs did not produce complete normalized output artifacts",
            "full_parity_claim": False,
        }
    old_out = (old_dir / "stdout.normalized.bin").read_bytes()
    new_out = (new_dir / "stdout.normalized.bin").read_bytes()
    old_err = (old_dir / "stderr.normalized.bin").read_bytes()
    new_err = (new_dir / "stderr.normalized.bin").read_bytes()
    gates = {
        "both_exited": old["exit_state"] == new["exit_state"] == "EXITED",
        "both_imports_selected_only": old["selected_imports_only"] and new["selected_imports_only"]
            and old.get("fincore_module_imported", False) and new.get("fincore_module_imported", False),
        "both_checkouts_clean_after": old["scratch_source_clean_after"] and new["scratch_source_clean_after"],
    }
    outputs_equal = old_out == new_out and old_err == new_err
    exits_equal = old["returncode"] == new["returncode"]
    if not all(gates.values()):
        classification = "NOT_COMPARABLE"
        if old["exit_state"] == "TIMEOUT" or new["exit_state"] == "TIMEOUT":
            reason = "TIMEOUT"
        elif not gates["both_imports_selected_only"]:
            reason = "IMPORT_GUARD_OR_CAPTURE_FAILED"
        elif not gates["both_checkouts_clean_after"]:
            reason = "SCRATCH_CHECKOUT_DIRTY"
        else:
            reason = "ONE_OR_BOTH_RUNS_DID_NOT_COMPLETE"
    elif old["returncode"] != 0 or new["returncode"] != 0:
        reason = "NONZERO_EXIT"
        if old["returncode"] == new["returncode"] and outputs_equal:
            classification = "BOTH_FAILED_OUTPUTS_EQUAL_NOT_PARITY"
        else:
            classification = "ORACLE_FAILURE_DIFFERENCE"
    elif not exits_equal:
        classification, reason = "EXIT_CODE_DIFFERENT", "EXIT_CODE_DIFFERENT"
    elif old_out != new_out:
        classification, reason = "STDOUT_DIFFERENT", "STDOUT_DIFFERENT"
    elif old_err != new_err:
        classification, reason = "STDERR_DIFFERENT", "STDERR_DIFFERENT"
    else:
        classification, reason = "OUTPUTS_EQUAL", "OUTPUTS_EQUAL"
    return {
        "classification": classification,
        "reason": reason,
        "classification_scope": "paired stdout/stderr bytes after exact selected-clone-root replacement, exit status, selected imports, and scratch status",
        "full_parity_claim": False,
        "old_returncode": old["returncode"],
        "new_returncode": new["returncode"],
        "normalized_stdout_equal": old_out == new_out,
        "normalized_stderr_equal": old_err == new_err,
        "exit_status_equal": exits_equal,
        "gates": gates,
        "path_normalization_policy": "Replace only the literal selected source clone path with <SOURCE_ROOT>; preserve all other output bytes and numeric text.",
    }


def run_paired(
    *, cases: list[dict[str, Any]], historical_root: Path, source_repo: Path,
    output_root: Path, conda: str, timeout: float,
) -> int:
    if output_root.exists():
        raise SystemExit(f"refusing to overwrite evidence directory: {output_root}")
    output_root.mkdir(parents=True)
    scratch = Path(tempfile.mkdtemp(prefix="fincore-historical-oracle-pair-"))
    (output_root / "scratch-root.txt").write_text(str(scratch.resolve()) + "\n", encoding="utf-8")
    run_started = datetime.now(timezone.utc).isoformat()
    source_observation = {
        "head_before": git_text(source_repo, "rev-parse", "HEAD"),
        "status_before": git_text(source_repo, "status", "--porcelain", "--untracked-files=all"),
    }
    write_json(output_root / "source-repo-before.json", source_observation)
    setup_errors: list[str] = []
    try:
        source = clean_clone(source_repo, scratch, output_root / "clone-setup")
        initial_meta = checkout_revision(source, REVISIONS[0], output_root / "clone-setup")
    except Exception as exc:
        setup_errors.append(f"{type(exc).__name__}: {exc}")
        write_json(output_root / "clone-setup" / "setup-error.json", {
            "error": f"{type(exc).__name__}: {exc}",
            "expected_initial_commit": REVISIONS[0].commit,
        })
        write_json(output_root / "run-manifest.json", {
            "status": "NOT_COMPARABLE_SETUP_FAILURE",
            "setup_errors": setup_errors,
            "historical_archive_root": str(historical_root.resolve()),
            "source_repo": str(source_repo.resolve()),
            "started_utc": run_started,
            "full_parity_claim": False,
        })
        source_observation["head_after"] = git_text(source_repo, "rev-parse", "HEAD")
        source_observation["status_after"] = git_text(source_repo, "status", "--porcelain", "--untracked-files=all")
        write_json(output_root / "source-repo-after.json", source_observation)
        write_hash_manifest(output_root)
        shutil.rmtree(scratch, ignore_errors=True)
        return 2

    revision_observations: dict[str, list[dict[str, Any]]] = {revision.name: [] for revision in REVISIONS}
    revision_observations[REVISIONS[0].name].append(initial_meta)
    unavailable_revisions: dict[str, str] = {}
    pair_rows: list[dict[str, Any]] = []
    for case in cases:
        case_id = case["script_sha256"]
        per_revision: dict[str, dict[str, Any]] = {}
        staged = scratch / "oracle.py"
        source_path = historical_root / case["canonical_retained_path"]
        script_data = source_path.read_bytes()
        if sha256_bytes(script_data) != case_id:
            raise RuntimeError(f"archived script hash changed after inspection: {case['canonical_retained_path']}")
        staged.write_bytes(script_data)
        case_root = scratch / "work" / case_id
        for revision in REVISIONS:
            result_root = output_root / revision.name / case_id
            if revision.name in unavailable_revisions:
                result_root.mkdir(parents=True, exist_ok=True)
                per_revision[revision.name] = {
                    "script_sha256": case_id, "revision": revision.name,
                    "exit_state": "NOT_RUN", "returncode": None,
                    "selected_imports_only": False, "scratch_source_clean_after": False,
                    "reason": unavailable_revisions[revision.name],
                }
                write_json(result_root / "result.json", per_revision[revision.name])
                continue
            try:
                revision_meta = checkout_revision(
                    source, revision, result_root / "checkout"
                )
                revision_observations[revision.name].append(revision_meta)
                per_revision[revision.name] = run_case(
                    case=case, revision=revision, source=source,
                    revision_meta=revision_meta, historical_root=historical_root,
                    output_case=result_root, staged=staged, case_root=case_root,
                    conda=conda, timeout=timeout,
                )
            except Exception as exc:
                result_root.mkdir(parents=True, exist_ok=True)
                per_revision[revision.name] = {
                    "script_sha256": case_id, "revision": revision.name,
                    "exit_state": "NOT_RUN", "returncode": None,
                    "selected_imports_only": False, "scratch_source_clean_after": False,
                    "reason": f"{type(exc).__name__}: {exc}",
                }
                write_json(result_root / "result.json", per_revision[revision.name])
            finally:
                shutil.rmtree(case_root, ignore_errors=True)
            status = status_snapshot(source)
            (result_root / "status-after-check.txt").write_text(status, encoding="utf-8")
            if status:
                unavailable_revisions[revision.name] = (
                    "oracle left the scratch source checkout dirty; further revision changes stopped"
                )
                unavailable_revisions["old"] = unavailable_revisions[revision.name]
                unavailable_revisions["new"] = unavailable_revisions[revision.name]
        if all(name in per_revision for name in ("old", "new")):
            classification = classify_pair(
                per_revision["old"], per_revision["new"],
                output_root / "old" / case_id, output_root / "new" / case_id,
            )
        else:
            classification = {
                "classification": "NOT_COMPARABLE",
                "reason": "one or both revision runs were not recorded",
                "full_parity_claim": False,
            }
        row = {
            "script_sha256": case_id,
            "phase_mappings": [mapping["phase_or_group"] for mapping in case["mappings"]],
            "retained_paths": [mapping["retained_path"] for mapping in case["mappings"]],
            **classification,
        }
        pair_rows.append(row)
        write_json(output_root / "comparisons" / f"{case_id}.json", row)

    counts: dict[str, int] = {}
    for row in pair_rows:
        label = row["classification"]
        counts[label] = counts.get(label, 0) + 1
    run_status = "PAIRED_REPLAY_RECORDED_WITH_FAILURES_VISIBLE"
    final_status = status_snapshot(source)
    final_head = git_text(source, "rev-parse", "HEAD")
    (output_root / "scratch-source-status-after.txt").write_text(final_status, encoding="utf-8")
    write_json(output_root / "revision-observations.json", revision_observations)
    source_observation["head_after"] = git_text(source_repo, "rev-parse", "HEAD")
    source_observation["status_after"] = git_text(source_repo, "status", "--porcelain", "--untracked-files=all")
    write_json(output_root / "source-repo-after.json", source_observation)
    write_json(output_root / "run-manifest.json", {
        "status": run_status,
        "started_utc": run_started,
        "finished_utc": datetime.now(timezone.utc).isoformat(),
        "historical_archive_root": str(historical_root.resolve()),
        "runner_path": str(Path(__file__).resolve()),
        "runner_sha256": sha256_file(Path(__file__).resolve()),
        "wrapper_sha256": sha256_bytes(WRAPPER.encode("utf-8")),
        "source_repo": str(source_repo.resolve()),
        "source_repo_head_before_after": [source_observation["head_before"], source_observation["head_after"]],
        "source_repo_status_before_after": [source_observation["status_before"], source_observation["status_after"]],
        "historical_commands_and_environment": "not inferred; see historical archive metadata",
        "execution_environment": {
            "conda_executable": conda,
            "conda_environment": "base",
            "python_invocation": "python -B -c <strict replay wrapper>",
            "per_script_timeout_seconds": timeout,
        },
        "revision_commits": {revision.name: revision.commit for revision in REVISIONS},
        "scratch_checkout_model": "One disposable clone at a stable path is checked out clean/detached at each pinned revision for each paired case; the same immutable staged oracle path and cwd are used for both runs.",
        "scratch_final_head": final_head,
        "scratch_final_status": final_status,
        "distinct_script_hashes": len(cases),
        "comparison_counts": counts,
        "comparison_policy": "Only exact selected clone-root path bytes are normalized. Numeric values are never normalized. Matching nonzero exits are failures, not parity.",
        "coverage_claim": "Only archived phase-runs scripts selected by filename and hash-deduplicated; not complete Python source or capability-map parity.",
        "full_parity_claim": False,
    })
    table = [
        "# Paired historical-oracle replay results", "",
        "Status: `" + run_status + "` (this label records execution; it is not acceptance).", "",
        "| Script SHA-256 | Phase/group mappings | Classification | Reason |",
        "| --- | --- | --- | --- |",
    ]
    for row in pair_rows:
        table.append(
            f"| `{row['script_sha256']}` | {', '.join(row['phase_mappings'])} | "
            f"`{row['classification']}` | {row.get('reason', '')} |"
        )
    table.extend([
        "", "Matching nonzero exits are recorded as failures, never parity.",
        "Comparison normalizes only the selected clone root string. Numeric output is unchanged.",
        "This is limited to retained scripts under `historical-oracles/phase-runs`; it does not establish full Python/C++ parity.",
    ])
    (output_root / "README.md").write_text("\n".join(table) + "\n", encoding="utf-8")
    write_hash_manifest(output_root)
    # Scratch clones contain only disposable checkouts and are removed after status capture.
    shutil.rmtree(scratch, ignore_errors=True)
    return 0


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    evidence_root = repo_root / "docs/evidence/revised-plan-20261023"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--historical-root", type=Path, default=evidence_root / "historical-oracles")
    parser.add_argument("--source-repo", type=Path, default=repo_root.parent / "fincore")
    parser.add_argument("--evidence-root", type=Path, default=evidence_root / "oracle/broad-replay")
    parser.add_argument("--output-dir", type=Path, help="New output directory; existing paths are never overwritten.")
    parser.add_argument("--conda", default=DEFAULT_CONDA)
    parser.add_argument("--timeout-seconds", type=float, default=60.0)
    parser.add_argument("--inspect-only", action="store_true", help="Write static inventory only; this is the safe default.")
    parser.add_argument("--execute", action="store_true", help="Run archived oracle scripts. Requires prior static review and explicit operator go-ahead.")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    if args.execute and args.inspect_only:
        raise SystemExit("choose either --inspect-only or --execute")
    if args.timeout_seconds <= 0:
        raise SystemExit("--timeout-seconds must be positive")
    if not args.execute:
        # Inspection is intentionally the default and does not call archived scripts.
        args.inspect_only = True
    historical_root = args.historical_root.resolve()
    source_repo = args.source_repo.resolve()
    evidence_root = args.evidence_root.resolve()
    if not historical_root.is_dir():
        raise SystemExit(f"historical archive does not exist: {historical_root}")
    if not source_repo.is_dir():
        raise SystemExit(f"source repository does not exist: {source_repo}")
    if not Path(args.conda).is_file():
        raise SystemExit(f"Anaconda conda executable does not exist: {args.conda}")
    if args.output_dir:
        output_root = args.output_dir.resolve()
    elif args.inspect_only:
        output_root = evidence_root / "preflight"
    else:
        run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        output_root = evidence_root / "runs" / run_id
    if args.inspect_only:
        cases = prepare_inventory(historical_root, output_root)
        summary = summarize_inspection(cases)
        print(json.dumps({"status": "INSPECTION_ONLY", "output_dir": str(output_root), **summary}, sort_keys=True))
        return 0
    verify_archive(historical_root)
    cases = discover_scripts(historical_root)
    return run_paired(
        cases=cases, historical_root=historical_root, source_repo=source_repo,
        output_root=output_root, conda=args.conda, timeout=args.timeout_seconds,
    )


if __name__ == "__main__":
    raise SystemExit(main())
