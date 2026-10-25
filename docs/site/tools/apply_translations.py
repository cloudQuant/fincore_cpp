#!/usr/bin/env python3
"""Apply a JSON msgid-to-msgstr mapping to a gettext catalog."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from babel.messages.pofile import read_po, write_po


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("catalog", type=Path)
    parser.add_argument("mapping", type=Path)
    args = parser.parse_args()
    translations = json.loads(args.mapping.read_text(encoding="utf-8"))
    with args.catalog.open("rb") as handle:
        catalog = read_po(handle)
    applied = 0
    missing = []
    for message in catalog:
        if not message.id:
            continue
        key = message.id if isinstance(message.id, str) else message.id[0]
        if key in translations:
            value = translations[key]
            if isinstance(message.id, tuple):
                message.string = (value,)
            else:
                message.string = value
            message.flags.discard("fuzzy")
            applied += 1
        elif key not in missing:
            missing.append(key)
    with args.catalog.open("wb") as handle:
        write_po(handle, catalog, width=79)
    print(json.dumps({"applied": applied, "catalog_entries":
                      len(list(catalog)), "missing": len(missing)},
                     ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
