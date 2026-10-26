#!/usr/bin/env python3
"""Check a built documentation site: internal links, machine paths, catalogs.

Run from the repository root after building both languages. Reports the counts
the acceptance record quotes, and exits non-zero when an internal link does not
resolve on disk or a built page leaks an absolute machine path.
"""

from __future__ import annotations

import argparse
import glob
import html.parser
import os
import re
import sys
import urllib.parse

from babel.messages.pofile import read_po

MACHINE_PATH = re.compile(
    r"(?<![\w./])[A-Za-z]:[\\/](?!/)"      # drive letter followed by a slash
    r"|[/\\]home[/\\]"                      # POSIX home tree
    r"|[/\\]Users[/\\]"                     # Windows home tree
)


class LinkCollector(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.links: list[str] = []

    def handle_starttag(self, tag: str, attrs) -> None:
        found = dict(attrs)
        for key in ("href", "src"):
            if key in found:
                self.links.append(found[key])


def walk_pages(roots: list[str]) -> tuple[int, int, int]:
    pages = broken = machine = 0
    for root in roots:
        for path in sorted(glob.glob(root + "/**/*.html", recursive=True)):
            text = open(path, encoding="utf-8", errors="replace").read()
            collector = LinkCollector()
            collector.feed(text)
            pages += 1
            for link in collector.links:
                if link.startswith(("http://", "https://", "#", "mailto:")):
                    continue
                target = urllib.parse.unquote(link.split("#")[0].split("?")[0])
                if not target:
                    continue
                resolved = os.path.normpath(os.path.join(os.path.dirname(path), target))
                if not (os.path.exists(resolved) or os.path.exists(resolved + ".html")):
                    broken += 1
                    print(f"broken link: {path} -> {link}", file=sys.stderr)
            if MACHINE_PATH.search(text):
                machine += 1
                print(f"machine path: {path}", file=sys.stderr)
    return pages, broken, machine


def walk_catalogs(locale_dir: str) -> tuple[int, int, int, int, int]:
    catalogs = entries = translated = missing = fuzzy = 0
    for po_path in sorted(glob.glob(locale_dir + "/**/*.po", recursive=True)):
        catalogs += 1
        with open(po_path, "rb") as handle:
            catalog = read_po(handle)
        for message in catalog:
            if not message.id:
                continue
            entries += 1
            if message.fuzzy:
                fuzzy += 1
                print(f"fuzzy entry: {po_path}: {message.id!r:.120}", file=sys.stderr)
            if message.string:
                translated += 1
            else:
                missing += 1
                print(f"untranslated: {po_path}: {message.id!r:.120}", file=sys.stderr)
    return catalogs, entries, translated, missing, fuzzy


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--en", default="docs/_build/html/en")
    parser.add_argument("--zh", default="docs/_build/html/zh-cn")
    parser.add_argument("--locale", default="docs/site/locale/zh_CN/LC_MESSAGES")
    args = parser.parse_args()

    pages, broken, machine = walk_pages([args.en, args.zh])
    catalogs, entries, translated, missing, fuzzy = walk_catalogs(args.locale)
    print(
        f"pages={pages} broken_links={broken} machine_path_pages={machine} "
        f"catalogs={catalogs} entries={entries} translated={translated} "
        f"untranslated={missing} fuzzy={fuzzy}"
    )
    return 0 if broken == 0 and machine == 0 and fuzzy == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
