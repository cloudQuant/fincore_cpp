#!/usr/bin/env python3
"""Generate one Breathe API page per public module from the API ledger."""

from __future__ import annotations

import csv
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LEDGER = ROOT / "docs/site/api-ledger.tsv"
OUTPUT = ROOT / "docs/site/api"

MODULES = {
    "core": (
        "Core types and errors",
        "Frames, timestamps, calendars, operation descriptors, versions, "
        "rolling windows, data-provider contracts and structured errors.",
        ["analysis_context", "calendar", "data", "dataframe", "error", "errors",
         "operations", "rolling_engine", "version"],
        "../quickstart.md",
    ),
    "metrics": (
        "Metrics",
        "Return, risk, drawdown, distribution, rolling, yearly and "
        "performance-attribution statistics.",
        ["metrics"],
        "../guides/metrics.md",
    ),
    "performance": (
        "Performance",
        "Cashflow-adjusted returns, disclosure contexts, returns inference and "
        "performance statistics.",
        ["performance"],
        "../guides/metrics.md",
    ),
    "risk": (
        "Risk",
        "VaR/ES diagnostics, GARCH/EGARCH estimates, extreme-value models, "
        "backtesting, walk-forward validation and risk reports.",
        ["risk"],
        "../guides/portfolio-risk.md",
    ),
    "optimization": (
        "Optimization",
        "Mean-variance objectives, risk parity and efficient-frontier "
        "construction with diagnostics.",
        ["optimization"],
        "../guides/portfolio-risk.md",
    ),
    "portfolio": (
        "Portfolio",
        "Positions, capacity, exposure and portfolio model inputs.",
        ["portfolio"],
        "../guides/portfolio-risk.md",
    ),
    "attribution": (
        "Attribution",
        "Brinson-Fachler, Brinson-Hood-Beebower, Fama-French and style "
        "attribution.",
        ["attribution"],
        "../guides/metrics.md",
    ),
    "factor": (
        "Factor research",
        "Factor data preparation, PIT alignment, forward returns, costs, "
        "research evaluation, detailed research, tears and serialization.",
        ["factor"],
        "../guides/factor-research.md",
    ),
    "simulation": (
        "Simulation",
        "Bootstrap, Monte Carlo and stress scenarios with seeded "
        "reproducibility.",
        ["simulation"],
        "../guides/portfolio-risk.md",
    ),
    "report": (
        "Reports",
        "Report models, HTML/Plotly/Bokeh/XLSX renderers, portfolio and risk "
        "report builders.",
        ["report"],
        "../guides/reports-runtime.md",
    ),
    "runtime": (
        "Runtime",
        "Operation catalogs, sessions, snapshots, artifact bundles and "
        "serialization helpers.",
        ["runtime"],
        "../guides/reports-runtime.md",
    ),
    "viz": (
        "Visualization",
        "Chart data models, native HTML/SVG builders, Plotly/Bokeh documents "
        "and the optional ImPlot backend.",
        ["viz"],
        "../guides/reports-runtime.md",
    ),
    "extensions": (
        "Extensions",
        "Extension discovery and snapshot contracts for optional plugins.",
        ["extensions"],
        "../conventions.md",
    ),
}

KIND_DIRECTIVE = {
    "class": "doxygenclass",
    "struct": "doxygenstruct",
    "union": "doxygenunion",
    "enum": "doxygenenum",
    "function": "doxygenfunction",
    "typedef": "doxygentypedef",
    "variable": "doxygenvariable",
}

KIND_ORDER = {kind: index for index, kind in enumerate(KIND_DIRECTIVE)}

# Multi-line string-literal constants cannot be parsed by the Sphinx C++
# domain; their source comments remain the authoritative contract.
UNRENDERABLE = {
    "fincore::DEFAULT_HTML_CSS",
    "fincore::basel_reference_disclosure",
}


def main() -> int:
    rows = list(csv.DictReader(LEDGER.open(encoding="utf-8"), delimiter="\t"))
    public = [row for row in rows if row["kind"] in KIND_DIRECTIVE
              and row["qualified_name"].startswith("fincore::")]
    top_depth: dict[str, int] = {}
    for row in public:
        depth = len(row["qualified_name"].split("::"))
        module = row["module"]
        top_depth[module] = min(top_depth.get(module, depth), depth)
    by_module: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in public:
        if len(row["qualified_name"].split("::")) == top_depth[row["module"]]:
            by_module[row["module"]].append(row)

    OUTPUT.mkdir(parents=True, exist_ok=True)
    rendered_global: set[str] = set()
    for slug, (title, summary, modules, guide) in MODULES.items():
        symbols = [row for module in modules for row in by_module[module]]
        symbols.sort(key=lambda row: (KIND_ORDER[row["kind"]],
                                      row["qualified_name"]))
        lines = [
            f"# {title}",
            "",
            summary,
            "",
            f"See the [usage guide]({guide}) for task-oriented examples. "
            "Signatures below are generated from the public headers at the "
            "same commit as the rest of this site.",
            "",
        ]
        seen: set[str] = set()
        for row in symbols:
            if row["qualified_name"] in UNRENDERABLE:
                continue
            target = row["qualified_name"]
            if (row["kind"] == "function" and row.get("signature")
                    and row.get("template") != "1"):
                signature = row["signature"]
                signature = signature.replace(" '", "'").replace("' ", "'")
                target = row["qualified_name"] + signature
            if target in seen or target in rendered_global:
                continue
            seen.add(target)
            rendered_global.add(target)
            directive = KIND_DIRECTIVE[row["kind"]]
            lines.append(f"```{{{directive}}} {target}")
            lines.append("```")
            lines.append("")
        (OUTPUT / f"{slug}.md").write_text(
            "\n".join(lines).rstrip() + "\n", encoding="utf-8")
        print(f"{slug}.md: {len(seen)} symbols")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
