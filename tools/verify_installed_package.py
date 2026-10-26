#!/usr/bin/env python3
"""Build and verify fincore's relocatable installed CMake package."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from uuid import uuid4


DEFAULT_DEPENDENCIES = {
    "dataframe": "90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230",
    "imgui": "46d39d56febc2a00bdd2270dc88c8a13f2a0441a",
    "implot": "7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4",
}


class VerificationError(RuntimeError):
    pass


class CommandRunner:
    def __init__(self, evidence_dir: Path) -> None:
        self.logs_dir = evidence_dir / "commands"
        self.logs_dir.mkdir(parents=True, exist_ok=True)
        self.commands: list[dict[str, object]] = []

    def run(
        self,
        label: str,
        command: list[str],
        *,
        cwd: Path | None = None,
        env: dict[str, str] | None = None,
        expected_success: bool = True,
        expected_output: str | None = None,
    ) -> subprocess.CompletedProcess[str]:
        log_path = self.logs_dir / f"{len(self.commands) + 1:03d}-{label}.txt"
        with log_path.open("w", encoding="utf-8") as log_file:
            log_file.write("$ " + " ".join(_quote_arg(part) for part in command) + "\n")
            process = subprocess.run(
                command,
                cwd=str(cwd) if cwd else None,
                env=env,
                text=True,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                check=False,
            )

        output = log_path.read_text(encoding="utf-8", errors="replace")
        result = {
            "label": label,
            "command": command,
            "cwd": str(cwd) if cwd else None,
            "exit_code": process.returncode,
            "expected_success": expected_success,
            "log": str(log_path),
        }
        self.commands.append(result)

        if expected_success and process.returncode != 0:
            raise VerificationError(
                f"{label} failed with exit code {process.returncode}; see {log_path}"
            )
        if not expected_success and process.returncode == 0:
            raise VerificationError(f"{label} unexpectedly succeeded; see {log_path}")
        if expected_output and expected_output not in output:
            raise VerificationError(
                f"{label} did not report expected text {expected_output!r}; see {log_path}"
            )
        return subprocess.CompletedProcess(command, process.returncode, output, None)


def _quote_arg(value: str) -> str:
    if not value or any(character.isspace() for character in value):
        return '"' + value.replace('"', '\\"') + '"'
    return value


def _parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    default_evidence = Path(tempfile.gettempdir()) / (
        "fincore-phase93-evidence-"
        + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        + "-"
        + uuid4().hex[:8]
    )
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, default=repo_root)
    parser.add_argument("--candidate-build", type=Path)
    parser.add_argument(
        "--dataframe-source", type=Path, default=repo_root.parent / "DataFrame"
    )
    parser.add_argument(
        "--imgui-source", type=Path, default=repo_root.parent / "imgui"
    )
    parser.add_argument(
        "--implot-source", type=Path, default=repo_root.parent / "implot"
    )
    parser.add_argument(
        "--dataframe-head", default=DEFAULT_DEPENDENCIES["dataframe"],
        help="expected full git HEAD for the pinned DataFrame source",
    )
    parser.add_argument(
        "--imgui-head", default=DEFAULT_DEPENDENCIES["imgui"],
        help="expected full git HEAD for the pinned ImGui source",
    )
    parser.add_argument(
        "--implot-head", default=DEFAULT_DEPENDENCIES["implot"],
        help="expected full git HEAD for the pinned ImPlot source",
    )
    parser.add_argument("--evidence-dir", type=Path, default=default_evidence)
    parser.add_argument("--jobs", type=int, choices=(1, 2), default=2)
    return parser.parse_args()


def _git_identity(path: Path, expected_head: str, name: str) -> dict[str, str]:
    try:
        head = subprocess.run(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            check=True,
            text=True,
            capture_output=True,
        ).stdout.strip()
        status = subprocess.run(
            ["git", "-C", str(path), "status", "--porcelain", "--untracked-files=all"],
            check=True,
            text=True,
            capture_output=True,
        ).stdout
    except (OSError, subprocess.CalledProcessError) as error:
        raise VerificationError(f"{name} source must be a readable git checkout: {path}") from error

    if head != expected_head:
        raise VerificationError(
            f"{name} HEAD mismatch: expected {expected_head}, found {head} at {path}"
        )
    if status.strip():
        raise VerificationError(
            f"{name} checkout is dirty; clean pinned dependency sources are required: {path}"
        )
    return {"path": str(path), "head": head, "working_tree": "clean"}


def _copy_ignore(_: str, names: list[str]) -> set[str]:
    ignored = {
        ".git",
        ".DS_Store",
        "CMakeFiles",
        "Testing",
        "__pycache__",
        ".worktrees",
        "build",
        "Build",
    }
    return {name for name in names if name in ignored or name.startswith("build-")}


def _copy_tree(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise VerificationError(f"source directory does not exist: {source}")
    shutil.copytree(source, destination, ignore=_copy_ignore, symlinks=True)


def _tree_digest(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        if not path.is_file() or ".git" in path.parts:
            continue
        relative = path.relative_to(root).as_posix()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        with path.open("rb") as file_obj:
            for chunk in iter(lambda: file_obj.read(1024 * 1024), b""):
                digest.update(chunk)
        digest.update(b"\0")
    return digest.hexdigest()


def _git_head_or_unavailable(path: Path) -> str:
    try:
        return subprocess.run(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            check=True,
            text=True,
            capture_output=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unavailable"


def _read_cmake_cache(cache_path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        left, value = line.split("=", 1)
        key = left.split(":", 1)[0]
        values[key] = value
    return values


def _cmake_env() -> dict[str, str]:
    env = os.environ.copy()
    for name in ("CMAKE_PREFIX_PATH", "CMAKE_MODULE_PATH"):
        env.pop(name, None)
    return env


def _cmake_isolated_flags(prefix: Path) -> list[str]:
    return [
        f"-DCMAKE_PREFIX_PATH:PATH={prefix}",
        "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF",
        "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF",
        "-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY:BOOL=ON",
    ]


def _configure_fresh_build(
    runner: CommandRunner,
    source: Path,
    build: Path,
    stage_prefix: Path,
    dataframe_source: Path,
    imgui_source: Path,
    implot_source: Path,
    jobs: int,
    *,
    build_fincore: bool = True,
) -> None:
    env = _cmake_env()
    runner.run(
        "configure-dataframe",
        [
            "cmake", "-S", str(dataframe_source), "-B", str(build / "dataframe"),
            "-DCMAKE_BUILD_TYPE:STRING=Release",
            f"-DCMAKE_INSTALL_PREFIX:PATH={stage_prefix}",
            "-DHMDF_TESTING:BOOL=OFF",
            "-DHMDF_EXAMPLES:BOOL=OFF",
            "-DHMDF_BENCHMARKS:BOOL=OFF",
            "-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY:BOOL=ON",
            "-DFETCHCONTENT_FULLY_DISCONNECTED:BOOL=ON",
        ],
        env=env,
    )
    runner.run(
        "build-dataframe",
        ["cmake", "--build", str(build / "dataframe"), "--parallel", str(jobs)],
        env=env,
    )
    for component in (None, "lib", "dev"):
        command = ["cmake", "--install", str(build / "dataframe"), "--prefix", str(stage_prefix)]
        if component:
            command.extend(["--component", component])
        runner.run(f"install-dataframe-{component or 'metadata'}", command, env=env)

    if not build_fincore:
        return

    runner.run(
        "configure-fincore",
        [
            "cmake", "-S", str(source), "-B", str(build / "fincore"),
            "-DCMAKE_BUILD_TYPE:STRING=Release",
            f"-DCMAKE_INSTALL_PREFIX:PATH={stage_prefix}",
            "-DFINCORE_BUILD_TESTING:BOOL=OFF",
            "-DFINCORE_BUILD_IMPLOT:BOOL=ON",
            "-DFINCORE_BUILD_IMPLOT_DEMO:BOOL=OFF",
            "-DFINCORE_DATAFRAME_SOURCE_DIR:PATH=",
            f"-DFINCORE_IMGUI_SOURCE_DIR:PATH={imgui_source}",
            f"-DFINCORE_IMPLOT_SOURCE_DIR:PATH={implot_source}",
            "-DFETCHCONTENT_FULLY_DISCONNECTED:BOOL=ON",
            "-DFETCHCONTENT_UPDATES_DISCONNECTED:BOOL=ON",
            *_cmake_isolated_flags(stage_prefix),
        ],
        env=env,
    )
    runner.run(
        "build-fincore",
        ["cmake", "--build", str(build / "fincore"), "--parallel", str(jobs)],
        env=env,
    )


def _verify_candidate_build(
    source_dir: Path,
    candidate_build: Path,
    expected_data_frame: dict[str, str],
    expected_imgui: dict[str, str],
    expected_implot: dict[str, str],
) -> dict[str, str]:
    cache_path = candidate_build / "CMakeCache.txt"
    if not cache_path.is_file():
        raise VerificationError(f"candidate build has no CMakeCache.txt: {candidate_build}")
    cache = _read_cmake_cache(cache_path)
    home = Path(cache.get("CMAKE_HOME_DIRECTORY", "")).resolve()
    if home != source_dir.resolve():
        raise VerificationError(
            f"candidate build uses a different source tree ({home}); expected {source_dir}"
        )
    if cache.get("FINCORE_BUILD_IMPLOT", "OFF").upper() not in {"ON", "TRUE", "1"}:
        raise VerificationError("candidate build must have FINCORE_BUILD_IMPLOT=ON")

    for cache_key, identity, name in (
        ("FINCORE_DATAFRAME_SOURCE_DIR", expected_data_frame, "DataFrame"),
        ("FINCORE_IMGUI_SOURCE_DIR", expected_imgui, "ImGui"),
        ("FINCORE_IMPLOT_SOURCE_DIR", expected_implot, "ImPlot"),
    ):
        configured_path = Path(cache.get(cache_key, "")).resolve()
        if not configured_path.is_dir() or _git_head_or_unavailable(configured_path) != identity["head"]:
            raise VerificationError(
                f"candidate build's {cache_key} does not match pinned {name} HEAD"
            )

    install_script = candidate_build / "cmake_install.cmake"
    if not install_script.is_file() or "fincoreImPlotTargets.cmake" not in install_script.read_text(
        encoding="utf-8", errors="replace"
    ):
        raise VerificationError(
            "candidate build predates the implot component install rules; reconfigure it first"
        )
    return {
        "build_dir": str(candidate_build),
        "source_dir": str(home),
        "configure_status": "existing candidate; not reconfigured or rebuilt",
    }


def _install_fincore_components(
    runner: CommandRunner,
    install_build: Path,
    stage_prefix: Path,
    headless_stage: Path,
) -> None:
    env = _cmake_env()
    base_command = [
        "cmake", "--install", str(install_build), "--prefix", str(stage_prefix),
        "--component", "fincore",
    ]
    runner.run("install-fincore-headless-component", base_command, env=env)
    shutil.copytree(stage_prefix, headless_stage)
    runner.run(
        "install-fincore-implot-component",
        [
            "cmake", "--install", str(install_build), "--prefix", str(stage_prefix),
            "--component", "implot",
        ],
        env=env,
    )


def _assert_relocatable_metadata(
    prefix: Path,
    forbidden_paths: list[Path],
    *,
    expected_gui: bool,
) -> None:
    metadata = sorted(prefix.rglob("*.cmake"))
    if not metadata:
        raise VerificationError(f"no installed CMake metadata found under {prefix}")

    disallowed = [str(path.resolve()) for path in forbidden_paths if path]
    for metadata_file in metadata:
        content = metadata_file.read_text(encoding="utf-8", errors="replace")
        for forbidden in disallowed:
            if forbidden and forbidden in content:
                raise VerificationError(
                    f"installed metadata {metadata_file} embeds source/build path {forbidden}"
                )

    config_files = list(prefix.rglob("fincoreConfig.cmake"))
    if len(config_files) != 1:
        raise VerificationError(
            f"expected one installed fincoreConfig.cmake, found {len(config_files)} under {prefix}"
        )
    config_dir = config_files[0].parent
    if not (config_dir / "fincoreConfig.cmake").is_file():
        raise VerificationError("installed fincoreConfig.cmake is missing")
    if not (config_dir / "fincoreTargets.cmake").is_file():
        raise VerificationError("installed fincoreTargets.cmake is missing")
    gui_export = config_dir / "fincoreImPlotTargets.cmake"
    if expected_gui != gui_export.is_file():
        raise VerificationError(
            f"unexpected implot export presence under {prefix}: {gui_export.is_file()}"
        )


def _configure_consumer(
    runner: CommandRunner,
    source_dir: Path,
    build_dir: Path,
    prefix: Path,
    *,
    expected_success: bool = True,
    expected_output: str | None = None,
) -> None:
    command = [
        "cmake", "-S", str(source_dir), "-B", str(build_dir),
        f"-DEXPECTED_PACKAGE_PREFIX:PATH={prefix}",
        *_cmake_isolated_flags(prefix),
    ]
    runner.run(
        f"consumer-configure-{source_dir.name}",
        command,
        env=_cmake_env(),
        expected_success=expected_success,
        expected_output=expected_output,
    )


def _build_and_run_consumer(
    runner: CommandRunner,
    source_dir: Path,
    build_dir: Path,
    prefix: Path,
    executable: str,
    jobs: int,
) -> None:
    _configure_consumer(runner, source_dir, build_dir, prefix)
    runner.run(
        f"consumer-build-{source_dir.name}",
        ["cmake", "--build", str(build_dir), "--parallel", str(jobs)],
        env=_cmake_env(),
    )
    executable_name = executable + (".exe" if sys.platform == "win32" else "")
    executable_matches = [
        path for path in build_dir.rglob(executable_name) if path.is_file()
    ]
    if len(executable_matches) != 1:
        raise VerificationError(
            f"expected one consumer executable {executable_name}, found {len(executable_matches)}"
        )
    executable_path = executable_matches[0]
    runner.run(
        f"consumer-run-{source_dir.name}", [str(executable_path)], env=_cmake_env()
    )


def _validate_args(args: argparse.Namespace, source_dir: Path, evidence_dir: Path) -> None:
    if args.jobs < 1:
        raise VerificationError("--jobs must be a positive integer")
    if not (source_dir / "CMakeLists.txt").is_file() or not (
        source_dir / "tests" / "consumer"
    ).is_dir():
        raise VerificationError(
            "--source-dir must contain the fincore CMake project and tests/consumer"
        )
    if evidence_dir.exists() and any(evidence_dir.iterdir()):
        raise VerificationError(f"evidence destination must be empty: {evidence_dir}")
    for root in (source_dir, args.dataframe_source.resolve(), args.imgui_source.resolve(), args.implot_source.resolve()):
        if evidence_dir == root or root in evidence_dir.parents:
            raise VerificationError("evidence destination cannot be inside a source checkout")


def main() -> int:
    args = _parse_args()
    source_dir = args.source_dir.resolve()
    evidence_dir = args.evidence_dir.resolve()
    _validate_args(args, source_dir, evidence_dir)
    evidence_dir.mkdir(parents=True, exist_ok=True)
    runner = CommandRunner(evidence_dir)

    scratch_root = Path(tempfile.mkdtemp(prefix="fincore-phase93-", dir="/tmp"))
    scratch_source = scratch_root / "source" / "fincore"
    scratch_dependencies = scratch_root / "source-dependencies"
    scratch_build = scratch_root / "build"
    stage_prefix = scratch_root / "stage-prefix"
    headless_stage = scratch_root / "headless-stage"
    relocated_gui_prefix = scratch_root / "relocated-gui-prefix"
    relocated_headless_prefix = scratch_root / "relocated-headless-prefix"
    manifest: dict[str, object] = {
        "started_at_utc": datetime.now(timezone.utc).isoformat(),
        "scratch_root": str(scratch_root),
        "offline": True,
        "testing": "OFF (library-only package check; GoogleTest FetchContent is not entered)",
        "commands": runner.commands,
    }
    scratch_removed = False

    try:
        dependencies = {
            "dataframe": _git_identity(args.dataframe_source.resolve(), args.dataframe_head, "DataFrame"),
            "imgui": _git_identity(args.imgui_source.resolve(), args.imgui_head, "ImGui"),
            "implot": _git_identity(args.implot_source.resolve(), args.implot_head, "ImPlot"),
        }
        source_head = _git_head_or_unavailable(source_dir)
        manifest["source"] = {
            "path": str(source_dir),
            "head": source_head,
        }
        manifest["dependencies"] = dependencies

        _copy_tree(source_dir, scratch_source)
        _copy_tree(args.dataframe_source.resolve(), scratch_dependencies / "DataFrame")
        _copy_tree(args.imgui_source.resolve(), scratch_dependencies / "imgui")
        _copy_tree(args.implot_source.resolve(), scratch_dependencies / "implot")
        manifest["source"]["copied_snapshot_sha256"] = _tree_digest(scratch_source)
        manifest["source"]["snapshot_scope"] = (
            "fincore source copy used by this run; excludes .git, .worktrees, build trees, "
            "CMakeFiles, Testing, __pycache__, and .DS_Store"
        )

        consumer_source = evidence_dir / "consumer-source"
        shutil.copytree(source_dir / "tests" / "consumer", consumer_source)
        scratch_build.mkdir(parents=True)
        stage_prefix.mkdir()
        _configure_fresh_build(
            runner,
            scratch_source,
            scratch_build,
            stage_prefix,
            scratch_dependencies / "DataFrame",
            scratch_dependencies / "imgui",
            scratch_dependencies / "implot",
            args.jobs,
            build_fincore=not bool(args.candidate_build),
        )

        install_build = scratch_build / "fincore"
        if args.candidate_build:
            candidate = args.candidate_build.resolve()
            manifest["candidate"] = _verify_candidate_build(
                source_dir,
                candidate,
                dependencies["dataframe"],
                dependencies["imgui"],
                dependencies["implot"],
            )
            install_build = candidate

        manifest["build_snapshot"] = {
            "path": str(install_build),
            "sha256": _tree_digest(install_build),
            "scope": "all regular files in the configured fincore build directory",
        }

        invalid_imgui = scratch_root / "invalid-imgui"
        invalid_imgui.mkdir()
        invalid_build = scratch_build / "invalid-imgui"
        invalid_command = [
            "cmake", "-S", str(scratch_source), "-B", str(invalid_build),
            "-DFINCORE_BUILD_TESTING:BOOL=OFF",
            "-DFINCORE_BUILD_IMPLOT:BOOL=ON",
            f"-DFINCORE_IMGUI_SOURCE_DIR:PATH={invalid_imgui}",
            f"-DFINCORE_IMPLOT_SOURCE_DIR:PATH={scratch_dependencies / 'implot'}",
            "-DFINCORE_DATAFRAME_SOURCE_DIR:PATH=",
            *_cmake_isolated_flags(stage_prefix),
        ]
        runner.run(
            "negative-invalid-imgui-source",
            invalid_command,
            env=_cmake_env(),
            expected_success=False,
            expected_output="FINCORE_IMGUI_SOURCE_DIR",
        )

        _install_fincore_components(runner, install_build, stage_prefix, headless_stage)

        forbidden_paths = [
            source_dir,
            args.dataframe_source.resolve(),
            args.imgui_source.resolve(),
            args.implot_source.resolve(),
            scratch_source,
            scratch_dependencies,
            scratch_build,
            scratch_root / "invalid-imgui",
            stage_prefix,
            args.candidate_build.resolve() if args.candidate_build else None,
        ]
        _assert_relocatable_metadata(headless_stage, forbidden_paths, expected_gui=False)
        _assert_relocatable_metadata(stage_prefix, forbidden_paths, expected_gui=True)

        if relocated_gui_prefix.exists() or relocated_headless_prefix.exists():
            raise VerificationError("relocated evidence prefixes already exist")
        shutil.move(str(stage_prefix), str(relocated_gui_prefix))
        shutil.move(str(headless_stage), str(relocated_headless_prefix))

        # Downstream builds run only after all scratch dependency and fincore source/build trees are gone.
        shutil.rmtree(scratch_source)
        shutil.rmtree(scratch_dependencies)
        shutil.rmtree(scratch_build)
        shutil.rmtree(scratch_root / "invalid-imgui", ignore_errors=True)
        scratch_removed = not scratch_source.exists() and not scratch_build.exists()
        if not scratch_removed:
            raise VerificationError("scratch source/build paths remained available before consumers")
        manifest["scratch_source_and_build_removed_before_consumers"] = True

        downstream_build = scratch_root / "downstream-build"
        _build_and_run_consumer(
            runner,
            consumer_source / "headless",
            downstream_build / "headless",
            relocated_headless_prefix,
            "fincore_headless_consumer",
            args.jobs,
        )
        _build_and_run_consumer(
            runner,
            consumer_source / "implot",
            downstream_build / "implot",
            relocated_gui_prefix,
            "fincore_implot_consumer",
            args.jobs,
        )
        _configure_consumer(
            runner,
            consumer_source / "optional_missing",
            downstream_build / "optional_missing",
            relocated_headless_prefix,
        )
        _configure_consumer(
            runner,
            consumer_source / "required_missing",
            downstream_build / "required_missing",
            relocated_headless_prefix,
            expected_success=False,
            expected_output="Required fincore component 'implot' is not installed",
        )
        _configure_consumer(
            runner,
            consumer_source / "unknown_required",
            downstream_build / "unknown_required",
            relocated_gui_prefix,
            expected_success=False,
            expected_output="Required fincore component 'unsupported_component' is not installed",
        )

        manifest["results"] = {
            "fresh_or_candidate_install": "PASS",
            "relocated_headless_consumer_compile_run": "PASS",
            "relocated_optional_implot_consumer_compile_run": "PASS",
            "optional_missing_implot_component": "PASS",
            "required_missing_implot_component": "PASS (expected configure failure)",
            "unknown_required_component": "PASS (expected configure failure)",
            "invalid_imgui_source": "PASS (expected configure failure)",
            "installed_metadata_has_no_source_or_build_paths": "PASS",
            "scratch_source_and_build_removed_before_consumers": "PASS",
        }
        manifest["status"] = "PASS"
        manifest["finished_at_utc"] = datetime.now(timezone.utc).isoformat()
        manifest["relocated_prefixes"] = (
            "ephemeral scratch prefixes; removed after downstream consumers passed"
        )
        manifest["retained_logs"] = str(runner.logs_dir)
        (evidence_dir / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        shutil.rmtree(scratch_root)
        shutil.rmtree(consumer_source)
        scratch_removed = True
        print(f"PASS: installed-package verification; evidence: {evidence_dir}")
        return 0
    except (OSError, VerificationError, subprocess.SubprocessError) as error:
        manifest["status"] = "FAIL"
        manifest["error"] = str(error)
        manifest["finished_at_utc"] = datetime.now(timezone.utc).isoformat()
        manifest["scratch_source_and_build_removed_before_consumers"] = scratch_removed
        manifest["retained_logs"] = str(runner.logs_dir)
        manifest["scratch_retained_for_diagnosis"] = str(scratch_root)
        (evidence_dir / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        print(f"FAIL: {error}\nEvidence: {evidence_dir}\nScratch: {scratch_root}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
