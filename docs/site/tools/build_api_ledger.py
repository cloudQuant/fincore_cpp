#!/usr/bin/env python3
"""Build the public-API documentation ledger from Doxygen XML.

The ledger is the P126 denominator for the comments/docs iteration. It counts
public class/struct members, free functions, variables, typedefs and enum
values under include/fincore, excluding the internal detail directory (the
Doxyfile already excludes it). It is intentionally a coverage inventory, not
a claim that existing prose is correct.
"""

from __future__ import annotations

import argparse
import json
import re
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_XML = ROOT / "docs" / "_build" / "doxygen" / "xml"
DEFAULT_OUT = ROOT / "docs" / "site" / "api-ledger.tsv"


def text(element: ET.Element | None) -> str:
    if element is None:
        return ""
    return " ".join("".join(element.itertext()).split())


def documented(element: ET.Element) -> bool:
    return bool(text(element.find("briefdescription")) or
                text(element.find("detaileddescription")))


def has_param_docs(element: ET.Element) -> bool:
    detailed = element.find("detaileddescription")
    if detailed is None:
        return False
    return detailed.find(".//parameterlist") is not None


def module_for(path: str, name: str) -> str:
    normalized = path.replace("\\", "/")
    marker = "include/fincore/"
    if marker in normalized:
        relative = normalized.split(marker, 1)[1]
        parts = relative.split("/")
        return parts[0] if len(parts) > 1 else Path(parts[0]).stem
    parts = name.split("::")
    return parts[1] if len(parts) > 1 else "core"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--xml-dir", type=Path, default=DEFAULT_XML)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--summary", type=Path,
                        default=ROOT / "docs" / "site" / "api-ledger-summary.json")
    args = parser.parse_args()
    if not args.xml_dir.is_dir():
        raise SystemExit(f"missing Doxygen XML directory: {args.xml_dir}")

    records: dict[tuple[str, str], dict[str, object]] = {}
    for path in sorted(args.xml_dir.glob("*.xml")):
        try:
            root = ET.parse(path).getroot()
        except ET.ParseError:
            continue
        for compound in root.findall("compounddef"):
            kind = compound.get("kind", "")
            if kind not in {"class", "struct", "union", "namespace"}:
                continue
            compound_name = text(compound.find("compoundname"))
            if not compound_name or "::detail::" in compound_name:
                continue
            location = compound.find("location")
            file_name = location.get("file", "") if location is not None else ""
            line = location.get("line", "") if location is not None else ""
            module = module_for(file_name, compound_name)
            if kind in {"class", "struct", "union"}:
                key = (kind, compound_name)
                records[key] = {
                    "kind": kind,
                    "name": compound_name,
                    "module": module,
                    "file": file_name,
                    "line": line,
                    "documented": int(documented(compound)),
                    "param_docs": "n/a",
                    "signature": "",
                    "template": ("1" if compound.find("templateparamlist")
                                 is not None else "0"),
                }
            for member in compound.findall(".//memberdef"):
                member_kind = member.get("kind", "")
                if member_kind not in {
                        "function", "variable", "typedef", "enum", "enumvalue",
                        "signal", "slot"}:
                    continue
                protection = member.get("prot", "public")
                if protection != "public":
                    continue
                member_name = text(member.find("name"))
                qualified = compound_name + "::" + member_name
                member_location = member.find("location")
                member_file = (member_location.get("file", "")
                               if member_location is not None else file_name)
                member_line = (member_location.get("line", "")
                               if member_location is not None else line)
                member_module = module_for(member_file, qualified)
                has_params = member.find("param") is not None
                signature = ""
                if member_kind == "function":
                    signature = text(member.find("argsstring"))
                key = (member_kind, qualified, signature)
                if key in records:
                    continue
                records[key] = {
                    "kind": member_kind,
                    "name": qualified,
                    "module": member_module,
                    "file": member_file,
                    "line": member_line,
                    "documented": int(documented(member)),
                    "param_docs": ("documented" if has_param_docs(member)
                                   else ("missing" if has_params else "n/a")),
                    "signature": signature,
                    "template": ("1" if member.find("templateparamlist")
                                 is not None else "0"),
                }

    rows = sorted(records.values(), key=lambda row: (
        str(row["module"]), str(row["kind"]), str(row["name"])))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("kind\tqualified_name\tmodule\tfile\tline\t"
                     "documented\tparam_docs\tsignature\ttemplate\n")
        for row in rows:
            handle.write("\t".join(str(row[key]) for key in (
                "kind", "name", "module", "file", "line", "documented",
                "param_docs", "signature", "template")) + "\n")

    by_module: dict[str, Counter] = defaultdict(Counter)
    for row in rows:
        stats = by_module[str(row["module"])]
        stats["total"] += 1
        stats["documented"] += int(row["documented"])
        stats["missing"] += 1 - int(row["documented"])
        if row["param_docs"] == "missing":
            stats["param_missing"] += 1
    summary = {
        "source": str(args.xml_dir),
        "total": len(rows),
        "documented": sum(int(row["documented"]) for row in rows),
        "missing": sum(1 - int(row["documented"]) for row in rows),
        "param_missing": sum(1 for row in rows
                             if row["param_docs"] == "missing"),
        "by_module": {name: dict(stats)
                      for name, stats in sorted(by_module.items())},
    }
    args.summary.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n",
                            encoding="utf-8", newline="\n")
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
