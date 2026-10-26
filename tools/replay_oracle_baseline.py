#!/usr/bin/env python3
"""Replay the preserved Phase38/39 adjusted-Sharpe oracle in clean clones."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Sequence


PHASE38_COMMIT = "2354bb560788f372502151a8b1b53accf536a6c0"
PHASE39_COMMIT = "4b8f31b1b59707480d5db9603704d83bbf7e6946"
OPTIONAL_DISTRIBUTIONS = (
    "empyrical",
    "empyrical-reloaded",
    "pyfolio",
    "pyfolio-reloaded",
    "alphalens",
    "alphalens-reloaded",
    "matplotlib",
    "statsmodels",
    "numba",
    "bottleneck",
)
ENVIRONMENT_LINES = (
    "fincore_file=",
    "fincore_source_root=",
    "sys_path_head=",
)

METADATA_PROGRAM = r"""
import importlib.metadata as metadata
import json
import locale
import os
import platform
import sys
import time
from pathlib import Path

import fincore
import numpy as np
import pandas as pd
import scipy

source_root = Path(os.environ["FINCORE_PHASE38_PYTHON_SOURCE"]).resolve()
module_file = Path(fincore.__file__).resolve()
if os.path.commonpath((str(source_root), str(module_file))) != str(source_root):
    raise SystemExit(
        f"IMPORT_ROOT_MISMATCH: {module_file} is outside {source_root}"
    )
if hasattr(time, "tzset"):
    time.tzset()
try:
    active_locale = locale.setlocale(locale.LC_ALL, "")
except locale.Error as exc:
    active_locale = f"ERROR:{exc}"

def installed_version(name):
    try:
        return metadata.version(name)
    except metadata.PackageNotFoundError:
        return None

result = {
    "python": {
        "version": sys.version,
        "implementation": platform.python_implementation(),
        "executable": str(Path(sys.executable).resolve()),
        "platform": platform.platform(),
    },
    "libraries": {
        "numpy": np.__version__,
        "pandas": pd.__version__,
        "scipy": scipy.__version__,
        "optional_distributions": {
            name: installed_version(name)
            for name in (
                "empyrical", "empyrical-reloaded", "pyfolio",
                "pyfolio-reloaded", "alphalens", "alphalens-reloaded",
                "matplotlib", "statsmodels", "numba", "bottleneck",
            )
        },
    },
    "import": {
        "expected_source_root": str(source_root),
        "fincore_file": str(module_file),
        "sys_path_head": sys.path[:3],
    },
    "determinism": {
        "LC_ALL": os.environ.get("LC_ALL"),
        "LANG": os.environ.get("LANG"),
        "locale": active_locale,
        "TZ": os.environ.get("TZ"),
        "timezone": list(time.tzname),
        "PYTHONHASHSEED": os.environ.get("PYTHONHASHSEED"),
        "python_random_seed": os.environ.get("FINCORE_PYTHON_RANDOM_SEED"),
        "numpy_random_seed": os.environ.get("FINCORE_NUMPY_RANDOM_SEED"),
        "thread_limits": {
            key: os.environ.get(key)
            for key in (
                "OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS",
                "MKL_NUM_THREADS", "NUMEXPR_NUM_THREADS",
            )
        },
        "probe_uses_randomness": False,
    },
}
print(json.dumps(result, sort_keys=True, indent=2))
"""

SEED_WRAPPER = r"""
import os
import random
import runpy

import numpy as np

random.seed(int(os.environ["FINCORE_PYTHON_RANDOM_SEED"]))
np.random.seed(int(os.environ["FINCORE_NUMPY_RANDOM_SEED"]))
runpy.run_path(os.environ["FINCORE_ORACLE_SCRIPT"], run_name="__main__")
"""


@dataclass(frozen=True)
class Revision:
    name: str
    commit: str


REVISIONS = (
    Revision("phase38", PHASE38_COMMIT),
    Revision("phase39", PHASE39_COMMIT),
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def run_command(
    command: Sequence[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(command),
        cwd=cwd,
        env=env,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def git_text(source: Path, *args: str) -> str:
    result = run_command(("git", "-C", str(source), *args), cwd=source)
    if result.returncode != 0:
        raise RuntimeError(
            f"git {' '.join(args)} failed in {source}: {result.stderr.strip()}"
        )
    return result.stdout.strip()


def clean_status(source: Path) -> str:
    return git_text(source, "status", "--porcelain", "--untracked-files=all")


def verify_historical_hashes(historical_root: Path) -> None:
    sums_path = historical_root / "SHA256SUMS.txt"
    for line in sums_path.read_text(encoding="utf-8").splitlines():
        expected, relative = line.split("  ", 1)
        path = historical_root / relative
        if path.resolve() == sums_path.resolve():
            raise RuntimeError("historical SHA256SUMS must not hash itself")
        actual = sha256_file(path)
        if actual != expected:
            raise RuntimeError(f"historical artifact hash mismatch: {relative}")


def deterministic_environment(source: Path, scratch: Path) -> dict[str, str]:
    env = os.environ.copy()
    env.pop("PYTHONHOME", None)
    env.pop("PYTHONSTARTUP", None)
    env.update(
        {
            "FINCORE_PHASE38_PYTHON_SOURCE": str(source.resolve()),
            "FINCORE_PYTHON_RANDOM_SEED": "0",
            "FINCORE_NUMPY_RANDOM_SEED": "0",
            "PYTHONPATH": str(source.resolve()),
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
        }
    )
    return env


def conda_python_command(conda: str, *args: str) -> list[str]:
    return [conda, "run", "--no-capture-output", "-n", "base", "python", *args]


def capture_metadata(
    *, conda: str, scratch: Path, env: dict[str, str], output_dir: Path
) -> tuple[dict[str, Any], int]:
    result = run_command(
        conda_python_command(conda, "-B", "-c", METADATA_PROGRAM),
        cwd=scratch,
        env=env,
    )
    write_text(output_dir / "metadata.stdout.txt", result.stdout)
    write_text(output_dir / "metadata.stderr.txt", result.stderr)
    try:
        metadata = json.loads(result.stdout)
    except json.JSONDecodeError:
        metadata = {
            "capture_error": "metadata output was not valid JSON",
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
        }
    write_text(
        output_dir / "metadata.json",
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
    )
    return metadata, result.returncode


def run_revision(
    *,
    revision: Revision,
    source_repo: Path,
    oracle_script: Path,
    output_root: Path,
    conda: str,
) -> dict[str, Any]:
    output_dir = output_root / revision.name
    scratch = Path(tempfile.mkdtemp(prefix=f"fincore-{revision.name}-oracle-"))
    source = scratch / "source"
    write_text(output_dir / "scratch-path.txt", str(scratch.resolve()) + "\n")

    clone_command = (
        "git", "clone", "--local", "--no-hardlinks", "--no-checkout", "--quiet",
        str(source_repo), str(source),
    )
    clone_started = time.perf_counter()
    clone_result = run_command(clone_command, cwd=output_root)
    write_text(output_dir / "clone.stdout.txt", clone_result.stdout)
    write_text(output_dir / "clone.stderr.txt", clone_result.stderr)
    if clone_result.returncode != 0:
        raise RuntimeError(
            f"clone failed for {revision.name}: {clone_result.stderr.strip()}"
        )
    checkout_command = ("git", "-C", str(source), "checkout", "--quiet", "--detach", revision.commit)
    checkout_result = run_command(checkout_command, cwd=output_root)
    write_text(output_dir / "checkout.stdout.txt", checkout_result.stdout)
    write_text(output_dir / "checkout.stderr.txt", checkout_result.stderr)
    if checkout_result.returncode != 0:
        raise RuntimeError(
            f"checkout failed for {revision.name}: {checkout_result.stderr.strip()}"
        )
    clone_elapsed = time.perf_counter() - clone_started

    head = git_text(source, "rev-parse", "HEAD")
    if head != revision.commit:
        raise RuntimeError(f"wrong detached HEAD for {revision.name}: {head}")
    branch = git_text(source, "branch", "--show-current")
    if branch:
        raise RuntimeError(f"{revision.name} is not detached: branch={branch}")
    status_before = clean_status(source)
    write_text(output_dir / "status-before.txt", status_before)
    if status_before:
        raise RuntimeError(f"scratch checkout is dirty before run: {revision.name}")

    tree = git_text(source, "rev-parse", f"{head}^{{tree}}")
    tracked_files = (
        "fincore/metrics/ratios.py",
        "fincore/metrics/stats.py",
    )
    source_files: dict[str, str] = {}
    for relative in tracked_files:
        path = source / relative
        source_files[relative] = sha256_file(path) if path.is_file() else "MISSING"

    env = deterministic_environment(source, scratch)
    metadata, metadata_returncode = capture_metadata(
        conda=conda, scratch=scratch, env=env, output_dir=output_dir
    )

    staged_oracle = scratch / "oracle.py"
    shutil.copyfile(oracle_script, staged_oracle)
    oracle_hash = sha256_file(staged_oracle)
    run_env = env.copy()
    run_env["FINCORE_ORACLE_SCRIPT"] = str(staged_oracle.resolve())
    command = conda_python_command(conda, "-B", "-c", SEED_WRAPPER)
    command_json = {
        "argv": command,
        "cwd": str(scratch.resolve()),
        "oracle_script": str(staged_oracle.resolve()),
        "oracle_sha256": oracle_hash,
        "environment": {
            key: run_env.get(key)
            for key in (
                "FINCORE_PHASE38_PYTHON_SOURCE", "FINCORE_ORACLE_SCRIPT",
                "FINCORE_PYTHON_RANDOM_SEED", "FINCORE_NUMPY_RANDOM_SEED",
                "PYTHONPATH", "PYTHONDONTWRITEBYTECODE", "PYTHONHASHSEED",
                "PYTHONNOUSERSITE", "LC_ALL", "LANG", "TZ",
                "OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS",
                "NUMEXPR_NUM_THREADS",
            )
        },
    }
    write_text(
        output_dir / "command.json",
        json.dumps(command_json, indent=2, sort_keys=True) + "\n",
    )
    oracle_started = time.perf_counter()
    result = run_command(command, cwd=scratch, env=run_env)
    oracle_elapsed = time.perf_counter() - oracle_started
    write_text(output_dir / "stdout.raw.txt", result.stdout)
    write_text(output_dir / "stderr.raw.txt", result.stderr)
    value_lines = [
        line for line in result.stdout.splitlines()
        if not line.startswith(ENVIRONMENT_LINES)
    ]
    write_text(output_dir / "value-output.txt", "\n".join(value_lines) + "\n")
    write_text(output_dir / "returncode.txt", f"{result.returncode}\n")

    status_after = clean_status(source)
    write_text(output_dir / "status-after.txt", status_after)
    if status_after:
        raise RuntimeError(f"scratch checkout became dirty after run: {revision.name}")

    expected_root = metadata.get("import", {}).get("expected_source_root", "")
    module_file = metadata.get("import", {}).get("fincore_file", "")
    source_record = {
        "name": revision.name,
        "commit": head,
        "detached": True,
        "tree": tree,
        "status_before": status_before,
        "status_after": status_after,
        "source_file_sha256": source_files,
        "oracle_sha256": oracle_hash,
        "oracle_script_source": str(oracle_script.resolve()),
        "metadata_returncode": metadata_returncode,
        "import_root_ok": bool(
            expected_root and module_file.startswith(expected_root + os.sep)
        ),
        "clone_elapsed_seconds": clone_elapsed,
        "oracle_elapsed_seconds": oracle_elapsed,
        "oracle_returncode": result.returncode,
    }
    write_text(
        output_dir / "source.json",
        json.dumps(source_record, indent=2, sort_keys=True) + "\n",
    )
    shutil.rmtree(scratch)
    return {
        "name": revision.name,
        "commit": revision.commit,
        "output_dir": output_dir,
        "value_lines": value_lines,
        "stderr": result.stderr,
        "returncode": result.returncode,
        "metadata_returncode": metadata_returncode,
        "metadata": metadata,
        "source_record": source_record,
    }


def historical_value_lines(path: Path) -> list[str]:
    return [
        line for line in path.read_text(encoding="utf-8").splitlines()
        if not line.startswith(ENVIRONMENT_LINES)
    ]


def compare_outputs(
    *,
    results: list[dict[str, Any]],
    historical_root: Path,
) -> dict[str, Any]:
    checks: dict[str, Any] = {}
    all_values_match = True
    all_stderr_match = True
    setup_clean = True
    oracle_failed_unexpectedly = False
    for result in results:
        name = result["name"]
        historical_dir = historical_root / name
        old_values = historical_value_lines(historical_dir / "oracle.raw.txt")
        old_stderr = (historical_dir / "oracle.stderr.txt").read_text(encoding="utf-8")
        value_match = result["value_lines"] == old_values
        stderr_match = result["stderr"] == old_stderr
        phase_classification = (
            "VALUE_DRIFT" if not value_match
            else "STDERR_DRIFT" if not stderr_match
            else "NO_DRIFT"
        )
        checks[f"{name}_vs_historical"] = {
            "classification": phase_classification,
            "value_output_equal": value_match,
            "stderr_equal": stderr_match,
            "historical_value_line_count": len(old_values),
            "replay_value_line_count": len(result["value_lines"]),
        }
        all_values_match = all_values_match and value_match
        all_stderr_match = all_stderr_match and stderr_match
        setup_clean = (
            setup_clean
            and result["metadata_returncode"] == 0
            and result["source_record"]["import_root_ok"]
        )
        if result["returncode"] != 0:
            expected_failure = any(
                line.startswith("FAIL:") or line.startswith("FAILURES=")
                and line != "FAILURES=0"
                for line in result["value_lines"]
            )
            oracle_failed_unexpectedly = oracle_failed_unexpectedly or not expected_failure

    phase38, phase39 = results
    cross_match = phase38["value_lines"] == phase39["value_lines"]
    checks["phase38_vs_phase39"] = {
        "classification": "NO_DRIFT" if cross_match else "VALUE_DRIFT",
        "value_output_equal": cross_match,
    }
    if not setup_clean:
        overall = "METADATA_OR_IMPORT_FAILED"
    elif oracle_failed_unexpectedly:
        overall = "RUN_FAILED"
    elif not all_values_match or not cross_match or any(
        result["returncode"] != 0 for result in results
    ):
        overall = "VALUE_DRIFT"
    elif not all_stderr_match:
        overall = "STDERR_DRIFT"
    else:
        overall = "NO_DRIFT"
    return {
        "classification": overall,
        "classification_scope": "adjusted-sharpe oracle value output and stderr only",
        "checks": checks,
        "value_output_line_policy": (
            "Ignore only fincore_file=, fincore_source_root=, and sys_path_head=; "
            "all other stdout lines and all stderr bytes are compared."
        ),
        "full_parity_claim": False,
    }


def emit_comparison(output_root: Path, comparison: dict[str, Any]) -> None:
    write_text(
        output_root / "comparison.json",
        json.dumps(comparison, indent=2, sort_keys=True) + "\n",
    )
    lines = [
        f"classification: {comparison['classification']}",
        f"scope: {comparison['classification_scope']}",
        "",
    ]
    for name, check in comparison["checks"].items():
        lines.append(f"{name}: {check['classification']}")
        lines.append(f"  value_output_equal: {check['value_output_equal']}")
        if "stderr_equal" in check:
            lines.append(f"  stderr_equal: {check['stderr_equal']}")
    lines.extend(
        [
            "",
            "NO_DRIFT is limited to this adjusted-Sharpe oracle output. It does not "
            "establish broad performance, risk, factor, report, package, or release parity.",
        ]
    )
    write_text(output_root / "classification.txt", "\n".join(lines) + "\n")


def write_run_readme(
    output_root: Path,
    *,
    source_repo: Path,
    oracle_script: Path,
    comparison: dict[str, Any],
) -> None:
    text = f"""# Phase38/39 Python oracle replay

This run replays the preserved adjusted-Sharpe oracle against two fresh detached
clones from `{source_repo}`. The checked commits are:

- Phase38: `{PHASE38_COMMIT}`
- Phase39: `{PHASE39_COMMIT}`

The oracle is `{oracle_script}`. Its expected constants were preserved from the
historical Phase38 script. Full stdout, stderr, command, environment metadata,
pre/post Git status, source hashes, and return codes are stored in each phase
directory. Each scratch clone is made with `git clone --local --no-hardlinks`
under the system temporary directory and deleted after its post-run status is
captured. The source repository and its worktree are read only.

Classification: **{comparison['classification']}**. This classification compares
only the original oracle's value output and stderr. It is not a claim of full
Python/C++ parity or broad factor/report coverage.

The historical captures did not record Python, NumPy, pandas, SciPy, locale,
timezone, or random-seed metadata. The replay records those settings, but cannot
retroactively establish the historical environment. Phase39 also has no
historical post-run Git status artifact.

## Probe map

| Probe group | Cases | Behavior covered |
| --- | --- | --- |
| API and ordinary scalar | `signature`, `period_kw`, `ordinary`, `ordinary_risk_free_0.001` | Signature, rejected `period` keyword, ordinary result, risk-free adjustment |
| Minimum samples and missing data | `raw_n0`–`raw_n4`, `raw4_valid0`–`raw4_valid3`, `raw10_all_nan` | Small input handling and NaN filtering |
| Adjustment boundaries | `upper_n19/20/49/50`, `lower_n19/20/49/50`, control lines | Dampening, skew/kurtosis terms, lower/upper clamps |
| Degenerate and non-finite values | `zero_std_*`, `positive_inf`, `negative_inf`, `both_inf`, `risk_free_*`, `near_zero_variance` | Zero variance, infinities, invalid risk-free values, near-zero variance |
| Sparse series and order sensitivity | `raw20_clean2`, `order_ordered`, `order_reordered`, `order_distinct` | Sparse clean observations and floating-point accumulation order |

The capture contains deterministic scalar and risk-edge probes for adjusted
Sharpe. It does not contain independent performance, factor, or report oracles;
those domains require their own fixtures and historical expected outputs.
"""
    write_text(output_root / "README.md", text)


def write_hash_manifest(output_root: Path) -> None:
    lines = []
    for path in sorted(output_root.rglob("*")):
        if not path.is_file() or path.name == "SHA256SUMS.txt":
            continue
        lines.append(f"{sha256_file(path)}  {path.relative_to(output_root)}")
    write_text(output_root / "SHA256SUMS.txt", "\n".join(lines) + "\n")


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    evidence_root = repo_root / "docs/evidence/revised-plan-20261023/oracle"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-repo",
        type=Path,
        default=repo_root.parent / "fincore",
        help="Python fincore Git repository used as the read-only clone source.",
    )
    parser.add_argument(
        "--evidence-root",
        type=Path,
        default=evidence_root,
        help="Directory containing historical captures and receiving replay runs.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="Optional new output directory. Defaults to a UTC timestamp under runs/.",
    )
    parser.add_argument(
        "--conda",
        default="/Users/yunjinqi/opt/anaconda3/bin/conda",
        help="Conda executable used to run Python in the base environment.",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    repo_root = Path(__file__).resolve().parents[1]
    source_repo = args.source_repo.resolve()
    evidence_root = args.evidence_root.resolve()
    historical_root = evidence_root / "historical"
    oracle_script = repo_root / "tools/oracles/phase38_adjusted_sharpe.py"
    output_root = args.output_dir
    if output_root is None:
        run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        output_root = evidence_root / "runs" / run_id
    output_root = output_root.resolve()

    if not source_repo.is_dir():
        raise SystemExit(f"source repository does not exist: {source_repo}")
    if not oracle_script.is_file():
        raise SystemExit(f"oracle script does not exist: {oracle_script}")
    if output_root.exists():
        raise SystemExit(f"refusing to overwrite existing evidence: {output_root}")
    verify_historical_hashes(historical_root)
    output_root.mkdir(parents=True)
    started_at = datetime.now(timezone.utc).isoformat()

    results = []
    run_errors: list[str] = []
    for revision in REVISIONS:
        try:
            results.append(
                run_revision(
                    revision=revision,
                    source_repo=source_repo,
                    oracle_script=oracle_script,
                    output_root=output_root,
                    conda=args.conda,
                )
            )
        except Exception as exc:  # Preserve a durable per-revision failure record.
            run_errors.append(f"{revision.name}: {type(exc).__name__}: {exc}")
            phase_dir = output_root / revision.name
            phase_dir.mkdir(parents=True, exist_ok=True)
            scratch_path_file = phase_dir / "scratch-path.txt"
            if scratch_path_file.is_file():
                scratch_path = Path(scratch_path_file.read_text(encoding="utf-8").strip())
                shutil.rmtree(scratch_path, ignore_errors=True)
            write_text(phase_dir / "failure.txt", run_errors[-1] + "\n")

    if len(results) == len(REVISIONS):
        comparison = compare_outputs(results=results, historical_root=historical_root)
    else:
        comparison = {
            "classification": "RUN_FAILED",
            "classification_scope": "adjusted-sharpe oracle replay",
            "checks": {},
            "run_errors": run_errors,
            "full_parity_claim": False,
        }
    emit_comparison(output_root, comparison)
    write_run_readme(
        output_root,
        source_repo=source_repo,
        oracle_script=oracle_script,
        comparison=comparison,
    )
    write_text(
        output_root / "run.json",
        json.dumps(
            {
                "started_at_utc": started_at,
                "finished_at_utc": datetime.now(timezone.utc).isoformat(),
                "source_repo": str(source_repo),
                "oracle_script": str(oracle_script),
                "oracle_sha256": sha256_file(oracle_script),
                "historical_root": str(historical_root),
                "phase_commits": {item.name: item.commit for item in REVISIONS},
                "run_errors": run_errors,
                "classification": comparison["classification"],
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
    )
    write_hash_manifest(output_root)
    print(f"output_dir={output_root}")
    print(f"classification={comparison['classification']}")
    return 0 if comparison["classification"] == "NO_DRIFT" else 1


if __name__ == "__main__":
    raise SystemExit(main())
