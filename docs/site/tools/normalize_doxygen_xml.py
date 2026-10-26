#!/usr/bin/env python3
"""Remove digit separators from generated Doxygen argument strings.

Doxygen copies C++14 digit separators (for example 1'000'000.0) into
<argsstring> elements. The Sphinx C++ domain cannot parse them, so Breathe
fails to resolve otherwise valid functions. The rewrite is limited to
argument strings and does not change any documented value.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

SEPARATOR = re.compile(r"(?<=\d)\s*(?:&apos;|')\s*(?=\d)")


def normalize(text: str) -> str:
    def fix(match: re.Match[str]) -> str:
        return SEPARATOR.sub("", match.group(0))

    return re.sub(r"<argsstring>.*?</argsstring>", fix, text, flags=re.S)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("xml_dir", type=Path)
    args = parser.parse_args()
    changed = 0
    for path in sorted(args.xml_dir.glob("*.xml")):
        original = path.read_text(encoding="utf-8")
        updated = normalize(original)
        if updated != original:
            path.write_text(updated, encoding="utf-8", newline="\n")
            changed += 1
    print(f"normalized {changed} XML files in {args.xml_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
