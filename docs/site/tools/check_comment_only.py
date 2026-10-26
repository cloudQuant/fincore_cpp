#!/usr/bin/env python3
"""Check that header changes only add/remove C/C++ comments.

Compares the worktree against HEAD after removing comments and normalizing
whitespace outside string/character literals. It also recognizes raw strings so
comment-looking bytes inside literals are preserved.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def strip_comments(source: str) -> str:
    output: list[str] = []
    i = 0
    n = len(source)
    while i < n:
        char = source[i]
        if char == "/" and i + 1 < n and source[i + 1] == "/":
            i += 2
            while i < n and source[i] not in "\r\n":
                i += 1
            continue
        if char == "/" and i + 1 < n and source[i + 1] == "*":
            i += 2
            while i + 1 < n and not (source[i] == "*" and source[i + 1] == "/"):
                i += 1
            i = min(n, i + 2)
            continue
        if char == "R" and i + 1 < n and source[i + 1] == '"':
            delimiter_end = source.find("(", i + 2)
            if delimiter_end != -1:
                delimiter = source[i + 2:delimiter_end]
                terminator = ")" + delimiter + '"'
                literal_end = source.find(terminator, delimiter_end + 1)
                if literal_end != -1:
                    literal_end += len(terminator)
                    output.append(source[i:literal_end])
                    i = literal_end
                    continue
        if (char == "'" and 0 < i < n - 1
                and source[i - 1].isalnum() and source[i + 1].isalnum()):
            # C++14 digit separator, not a character literal.
            output.append(char)
            i += 1
            continue
        if char in {'"', "'"}:
            quote = char
            output.append(char)
            i += 1
            while i < n:
                output.append(source[i])
                if source[i] == "\\" and i + 1 < n:
                    i += 1
                    output.append(source[i])
                elif source[i] == quote:
                    i += 1
                    break
                i += 1
            continue
        if char.isspace():
            i += 1
            continue
        output.append(char)
        i += 1
    return "".join(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("files", nargs="*")
    args = parser.parse_args()
    if args.files:
        paths = [Path(item) for item in args.files]
    else:
        paths = [Path(item) for item in subprocess.check_output(
            ["git", "-C", str(ROOT), "diff", "--name-only", "--",
             "include/fincore"], text=True, encoding="utf-8").splitlines()]
    failures = 0
    for relative in paths:
        relative_text = relative.as_posix()
        try:
            before = subprocess.check_output(
                ["git", "-C", str(ROOT), "show", f"HEAD:{relative_text}"],
                text=True, encoding="utf-8")
        except subprocess.CalledProcessError:
            print(f"{relative_text}: not present at HEAD (new file)")
            failures += 1
            continue
        after = (ROOT / relative).read_text(encoding="utf-8")
        if strip_comments(before) != strip_comments(after):
            print(f"{relative_text}: NON-COMMENT TOKEN CHANGE")
            failures += 1
        else:
            print(f"{relative_text}: comment-only OK")
    print(f"checked={len(paths)} failures={failures}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
