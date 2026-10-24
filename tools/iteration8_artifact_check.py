#!/usr/bin/env python3
"""Value-check and hash the Iteration 8 native report artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIR = ROOT / "docs/evidence/iteration-8/p108/artifacts"


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", type=Path, default=DEFAULT_DIR)
    args = parser.parse_args()
    directory = args.dir
    checks: list[dict] = []

    def record(name: str, passed: bool, detail: str) -> None:
        checks.append({"check": name, "passed": passed, "detail": detail})

    plotly = json.loads((directory / "report-plotly.json").read_text())
    record("plotly_trace_values", plotly["data"][0]["y"] == [1.0, 1.01, 1.03, 1.02, 1.05], "trace y equals report series")
    record("plotly_legend", plotly["data"][0]["name"] == "Cumulative Return", "legend from section map")
    record("plotly_title", plotly["layout"]["title"] == "Iteration 8 Native Artifact Check", "document title")

    bokeh = json.loads((directory / "report-bokeh.json").read_text())
    record("bokeh_glyph_values", bokeh["glyphs"][0]["y"] == [1.0, 1.01, 1.03, 1.02, 1.05], "glyph y equals report series")
    record("bokeh_legend", bokeh["glyphs"][0]["legend_label"] == "Cumulative Return", "glyph legend")

    html = (directory / "report.html").read_text()
    record("html_title", "Iteration 8 Native Artifact Check" in html, "title present")
    record("html_values", all(value in html for value in ("1.25", "2.5", "3.75", "Mean IC")), "metric/table values present")
    record("html_self_contained", "src=\"http" not in html and "href=\"http" not in html, "no external asset URL")
    echarts = (directory / "report-echarts.html").read_text()
    record("echarts_embedded", "echarts" in echarts.lower() and len(echarts) > 500_000, "ECharts payload embedded offline")
    record("echarts_series_values", all(value in echarts for value in ("1.0", "1.01", "1.03", "1.02", "1.05")), "ECharts series values present")

    with zipfile.ZipFile(directory / "report.xlsx") as workbook:
        sheet = workbook.read("xl/worksheets/sheet1.xml").decode("utf-8")
        shared = workbook.read("xl/sharedStrings.xml").decode("utf-8") if "xl/sharedStrings.xml" in workbook.namelist() else ""
        record("xlsx_values", all(value in sheet for value in ("1.25", "2.5", "3.75", "1.05")), "numeric cells present")
        record("xlsx_labels", "Mean Return by Quantile" in sheet or "Cumulative Return" in sheet, "table/series labels present")

    pdf_text = subprocess.check_output(["pdftotext", "-layout", str(directory / "report.pdf"), "-"], text=True)
    record("pdf_text_values", all(value in pdf_text for value in ("Iteration 8 Native Artifact Check", "Mean IC: 0.041", "1.25", "3.75", "1.05")), "PDF text contains metrics/tables/series")
    page = directory / "report-page-1.png"
    subprocess.run(["pdftoppm", "-png", "-r", "110", "-f", "1", "-l", "1", str(directory / "report.pdf"), str(directory / "report-page")], check=True, capture_output=True)
    rendered = directory / "report-page-1.png"
    if rendered.exists() and rendered != page:
        shutil.move(rendered, page)
    record("pdf_page_rendered", page.exists() and page.stat().st_size > 10_000, "PDF page rasterized for visual inspection")

    artifacts = {}
    for path in sorted(directory.iterdir()):
        artifacts[path.name] = {"sha256": sha(path), "size_bytes": path.stat().st_size}
    summary = {
        "schema_version": "1",
        "phase": "P108",
        "artifact_dir": str(directory.relative_to(ROOT)),
        "checks": checks,
        "all_passed": all(item["passed"] for item in checks),
        "artifacts": artifacts,
        "declared_variances": [
            "Native PDF uses base-14 fonts and vector paths, not Chromium PDF bytes.",
            "Plotly/Bokeh documents are native value-equivalent JSON, not Python object identity.",
            "Interactive display is NOT_RUN; only headless artifacts are claimed.",
        ],
    }
    (directory.parent / "artifact-manifest.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"all_passed": summary["all_passed"], "failed": [item for item in checks if not item["passed"]]}, indent=1))
    return 0 if summary["all_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
