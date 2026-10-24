#!/usr/bin/env python3
"""Archive and hash the Iteration 8 candidate source tree."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import subprocess
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATHS = [
    "CMakeLists.txt",
    "README.md",
    "include",
    "src",
    "tests",
    "examples",
    "benchmarks",
    "assets",
    "cmake",
    "tools",
    "docs/iterations/iteration-8-full-python-parity",
    "docs/iterations/capability-dispositions.json",
    "docs/acceptance/2026-10-24-iteration-8-native-parity-acceptance.md",
    "docs/evidence/iteration-8",
]
EXCLUDED_NAMES = {".DS_Store", "source.tar.gz", "source-manifest.json", "SHA256SUMS"}


def collect() -> list[Path]:
    files: list[Path] = []
    for entry in PATHS:
        path = ROOT / entry
        if path.is_file():
            files.append(path)
        elif path.is_dir():
            for candidate in path.rglob("*"):
                relative = candidate.relative_to(ROOT)
                if any(part.startswith("build") for part in relative.parts):
                    continue
                if candidate.name in EXCLUDED_NAMES or candidate.suffix == ".ppm":
                    continue
                if candidate.is_file():
                    files.append(candidate)
    return sorted(files)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / "docs/evidence/iteration-8/source.tar.gz")
    args = parser.parse_args()
    files = collect()
    payload = io.BytesIO()
    with tarfile.open(fileobj=payload, mode="w") as archive:
        for path in files:
            info = archive.gettarinfo(str(path), arcname=str(path.relative_to(ROOT)))
            info.mtime = 0
            info.uid = 0
            info.gid = 0
            info.uname = ""
            info.gname = ""
            with path.open("rb") as stream:
                archive.addfile(info, stream)
    compressed = gzip.compress(payload.getvalue(), mtime=0)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(compressed)
    head = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
    status = subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain"], text=True)
    submodules = subprocess.check_output(
        ["git", "-C", str(ROOT), "submodule", "status"], text=True
    ).strip().splitlines()
    manifest = {
        "schema_version": "1",
        "iteration": 8,
        "oracle_pin": "3e9ef205ef2023f92e89fddd1052d46b16429c64",
        "c4_manifest_sha256": "245ae75336912e30bcf6f34016ed4451116ec4ed719771a02d4b6e46ab0516e3",
        "c4_archive_sha256": "168789ea6fabe1ed4baf636bbcda3e7e372dca53e18a5a7a9f08ef9b4906a7da",
        "git_head": head,
        "dirty_path_count": len([line for line in status.splitlines() if line.strip()]),
        "submodules": submodules,
        "archive": {
            "path": str(args.output.relative_to(ROOT)),
            "sha256": hashlib.sha256(compressed).hexdigest(),
            "bytes": len(compressed),
        },
        "file_count": len(files),
        "files": [str(path.relative_to(ROOT)) for path in files],
    }
    manifest_path = args.output.parent / "source-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"sha256": manifest["archive"]["sha256"], "bytes": len(compressed),
                      "files": len(files), "dirty": manifest["dirty_path_count"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
