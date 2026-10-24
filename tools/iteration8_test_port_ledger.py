#!/usr/bin/env python3
"""Build the Iteration 8 Python test-file port ledger.

Every one of the 228 pinned Python test files is accounted for. The status is
deliberately conservative: NATIVE_MODULE_COVERAGE means native tests exist for
the same module surface, not that every Python test case was copied 1:1.
DECLARED_VARIANCE marks Python-identity/optional-runtime behaviour that R3
explicitly does not claim. META_GATE marks repository tooling whose native
equivalent is CTest/build/ledger evidence.
"""

from __future__ import annotations

import argparse
import ast
import csv
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PYTHON_ROOT = Path("/Users/yunjinqi/Documents/new_projects/fincore")
DEFAULT_OUT_DIR = ROOT / "docs/iterations/iteration-8-full-python-parity"

NATIVE_EVIDENCE = {
    "metrics": ["tests/unit/basic_test.cpp", "tests/unit/returns_test.cpp", "tests/unit/drawdown_test.cpp", "tests/unit/ratios_test.cpp", "tests/unit/risk_test.cpp", "tests/unit/stats_test.cpp", "tests/unit/rolling_metrics_test.cpp", "tests/unit/metrics_helpers_test.cpp", "tests/unit/metrics_frequencies_test.cpp", "tests/unit/metrics_bayesian_pure_test.cpp"],
    "attribution": ["tests/unit/brinson_test.cpp", "tests/unit/attribution_fama_french_test.cpp", "tests/unit/style_analysis_test.cpp", "tests/unit/attribution_performance_test.cpp", "tests/unit/attribution_facades_test.cpp"],
    "optimization": ["tests/unit/optimization_portfolio_test.cpp", "tests/unit/optimization_numerics_test.cpp"],
    "risk": ["tests/unit/risk_garch_test.cpp", "tests/unit/risk_evt_test.cpp", "tests/unit/risk_backtesting_test.cpp", "tests/unit/risk_models_test.cpp", "tests/unit/risk_report_test.cpp"],
    "data": ["tests/unit/data_models_test.cpp", "tests/unit/runtime_serialization_test.cpp"],
    "runtime": ["tests/unit/runtime_test.cpp", "tests/unit/runtime_lifecycle_test.cpp", "tests/unit/runtime_utilities_test.cpp", "tests/unit/runtime_serialization_test.cpp"],
    "factor": ["tests/unit/factor_data_test.cpp", "tests/unit/factor_performance_test.cpp", "tests/unit/factor_analysis_test.cpp", "tests/unit/factor_research_test.cpp", "tests/unit/factor_tears_test.cpp", "tests/unit/factor_calendar_test.cpp", "tests/unit/factor_artifacts_test.cpp", "tests/unit/factor_serialization_test.cpp", "tests/unit/factor_timedelta_test.cpp", "tests/unit/factor_workflows_test.cpp"],
    "viz": ["tests/unit/chart_data_test.cpp", "tests/unit/viz_render_model_test.cpp", "tests/unit/viz_documents_test.cpp", "tests/unit/viz_html_builder_test.cpp"],
    "simulation": ["tests/unit/simulation_parameters_test.cpp", "tests/unit/bootstrap_distribution_test.cpp", "tests/unit/gbm_paths_test.cpp", "tests/unit/monte_carlo_simulation_test.cpp", "tests/unit/stress_scenarios_test.cpp", "tests/unit/simulation_base_test.cpp"],
    "report": ["tests/unit/report_models_test.cpp", "tests/unit/report_html_test.cpp", "tests/unit/report_xlsx_test.cpp", "tests/unit/report_renderers_test.cpp", "tests/unit/factor_report_test.cpp"],
    "exceptions": ["tests/unit/errors_test.cpp"],
    "extensions": ["tests/unit/extensions_snapshot_test.cpp", "tests/unit/extensions_discovery_test.cpp"],
    "portfolio": ["tests/unit/positions_test.cpp", "tests/unit/transactions_test.cpp", "tests/unit/round_trips_test.cpp", "tests/unit/capacity_test.cpp"],
    "performance": ["tests/unit/performance_returns_test.cpp", "tests/unit/performance_cashflows_test.cpp", "tests/unit/performance_inference_test.cpp", "tests/unit/performance_disclosures_test.cpp"],
}

META_FOLDERS = {"quality", "packaging", "docs", "architecture", "benchmarks", "contracts", "coverage_gaps"}
VARIANCE_MARKERS = {
    "pymc": "EXTERNAL_OPTIONAL: PyMC posterior runtime is not native",
    "provider": "EXTERNAL_OPTIONAL: live provider SDK/network route; offline model tests cover the native boundary",
    "wheel": "PYTHON_PACKAGING: wheel/pip behaviour is not a native iteration-8 artifact",
    "entry_point": "PYTHON_PLUGIN_DISCOVERY: importlib entry points are replaced by the native manifest ABI",
    "entry_points": "PYTHON_PLUGIN_DISCOVERY: importlib entry points are replaced by the native manifest ABI",
    "importlib": "PYTHON_IMPORT_RUNTIME: importlib identity is not native",
    "to_pandas": "PYTHON_CONTAINER_IDENTITY: pandas object identity is not native",
    "matplotlib": "PYTHON_PLOTTING_OBJECT: matplotlib object identity is not native; native SVG/ImPlot artifacts cover values",
    "plotly": "PYTHON_PLOTTING_OBJECT: Plotly object identity is not native; native Plotly-compatible documents cover values",
    "bokeh": "PYTHON_PLOTTING_OBJECT: Bokeh object identity is not native; native Bokeh-compatible documents cover values",
}


def imports_of(path: Path) -> list[str]:
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"))
    except SyntaxError:
        return []
    modules: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                if alias.name.startswith("fincore"):
                    modules.add(alias.name)
        elif isinstance(node, ast.ImportFrom) and node.module and node.module.startswith("fincore"):
            modules.add(node.module)
    return sorted(modules)


def test_count(path: Path) -> int:
    try:
        tree = ast.parse(path.read_text(encoding="utf-8"))
    except SyntaxError:
        return 0
    return sum(
        1
        for node in ast.walk(tree)
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name.startswith("test_")
    )


def domain_for(relative: str, modules: list[str]) -> str | None:
    name = relative.lower()
    folder = relative.split("/")[0].lower()
    if "exception" in name:
        return "exceptions"
    if folder in {"test_viz", "parity"} and "viz" in name:
        return "viz"
    if folder == "extensions" or "extension" in name:
        return "extensions"
    if folder in {"test_data"} or "snapshot" in name or "provider" in name:
        return "data"
    if folder in {"runtime"}:
        return "runtime"
    if folder in {"factor_analysis"} or "factor" in name:
        return "factor"
    if folder in {"test_optimization"} or "optimization" in name or "frontier" in name or "risk_parity" in name:
        return "optimization"
    if folder in {"test_risk", "risk"}:
        return "risk"
    if folder in {"test_simulation"} or "simulation" in name or "bootstrap" in name or "monte_carlo" in name:
        return "simulation"
    if folder in {"test_attribution", "attribution"}:
        return "attribution"
    if folder in {"test_metrics", "metrics"}:
        return "metrics"
    if folder in {"portfolio"} or "portfolio" in name:
        return "portfolio"
    if folder in {"performance"}:
        return "performance"
    if folder in {"integration"} or "report" in name:
        return "report"
    for module in modules:
        parts = module.split(".")
        if len(parts) > 1:
            candidate = parts[1]
            if candidate == "factor_analysis":
                return "factor"
            if candidate in NATIVE_EVIDENCE:
                return candidate
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--python-root", type=Path, default=DEFAULT_PYTHON_ROOT)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    args = parser.parse_args()

    files = sorted((args.python_root / "tests").rglob("test_*.py"))
    if len(files) != 228:
        raise SystemExit(f"expected 228 test files, got {len(files)}")
    rows = []
    counts: Counter[str] = Counter()
    for path in files:
        relative = str(path.relative_to(args.python_root / "tests"))
        modules = imports_of(path)
        folder = relative.split("/")[0].lower()
        domain = domain_for(relative, modules)
        text = path.read_text(encoding="utf-8", errors="replace").lower()
        variance = next(
            (reason for marker, reason in VARIANCE_MARKERS.items() if marker in text and marker in path.name.lower() or (marker in text and marker in {"pymc", "to_pandas"})),
            None,
        )
        if folder in META_FOLDERS or relative == "parity/test_ledger.py":
            status = "META_GATE_EQUIVALENT"
            evidence = "CTest/build/route-ledger/benchmark evidence (Python repository gate, not product API)"
        elif "pymc" in relative or "bayesian" in relative and "stub" in relative:
            status = "EXTERNAL_OPTIONAL"
            evidence = "PyMC is EXTERNAL_OPTIONAL; native Bayesian cone tests cover the native path"
        elif "provider" in relative and "offline" not in relative and "interface" not in relative and "contracts" not in relative:
            status = "EXTERNAL_OPTIONAL"
            evidence = "live provider SDK tests are EXTERNAL_OPTIONAL; data_models_test.cpp covers offline models"
        elif variance and domain in {"viz", "extensions", "exceptions", "data"}:
            status = "DECLARED_VARIANCE"
            evidence = variance
        elif domain and domain in NATIVE_EVIDENCE:
            status = "NATIVE_MODULE_COVERAGE"
            evidence = ", ".join(NATIVE_EVIDENCE[domain])
        else:
            status = "UNMAPPED"
            evidence = ""
        counts[status] += 1
        rows.append(
            {
                "python_test_file": "tests/" + relative,
                "test_functions": test_count(path),
                "domain": domain or "",
                "imported_fincore_modules": ";".join(modules),
                "status": status,
                "native_evidence": evidence,
                "port_depth": "MODULE" if status == "NATIVE_MODULE_COVERAGE" else "",
                "declared_variance_or_action": variance or ("case-level port review required" if status == "UNMAPPED" else ""),
            }
        )

    args.out_dir.mkdir(parents=True, exist_ok=True)
    tsv_path = args.out_dir / "test-port-ledger.tsv"
    columns = ["python_test_file", "test_functions", "domain", "imported_fincore_modules", "status", "native_evidence", "port_depth", "declared_variance_or_action"]
    with tsv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    summary = {
        "schema_version": "1",
        "python_test_files": len(rows),
        "python_test_functions": sum(row["test_functions"] for row in rows),
        "status_counts": dict(sorted(counts.items())),
        "notes": [
            "NATIVE_MODULE_COVERAGE means native tests exist for the same module surface; it is not a claim that every Python case was copied one-to-one.",
            "DECLARED_VARIANCE and EXTERNAL_OPTIONAL rows are not PASS claims.",
            "META_GATE_EQUIVALENT rows are repository/packaging/quality gates whose native equivalent is CTest, the route ledger, or benchmark evidence.",
            "UNMAPPED rows are explicit remaining work, not silently skipped.",
        ],
    }
    (args.out_dir / "test-port-summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(summary["status_counts"], sort_keys=True))
    print(f"wrote {tsv_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
