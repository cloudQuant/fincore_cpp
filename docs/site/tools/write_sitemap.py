#!/usr/bin/env python3
"""Write a sitemap.xml for the assembled documentation site.

Run after the Pages artifact is assembled: the script walks the HTML files under
the site root and emits one absolute URL per page, so the sitemap cannot drift
from what was actually deployed.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from xml.sax.saxutils import escape


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("site_root", type=Path)
    parser.add_argument("base_url")
    args = parser.parse_args()

    base = args.base_url.rstrip("/") + "/"
    # 404.html answers an error rather than representing a page, so it stays out.
    pages = sorted(path.relative_to(args.site_root).as_posix()
                   for path in args.site_root.rglob("*.html")
                   if path.name != "404.html")
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">']
    for page in pages:
        lines.append(f"  <url><loc>{escape(base + page)}</loc></url>")
    lines.append("</urlset>")
    (args.site_root / "sitemap.xml").write_text("\n".join(lines) + "\n",
                                                encoding="utf-8")
    print(f"wrote sitemap.xml with {len(pages)} pages")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
