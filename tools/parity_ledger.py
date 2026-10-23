#!/usr/bin/env python3
"""Regenerate the native-vs-pinned-Python public-name parity ledger."""

from __future__ import annotations

import argparse
import ast
import re
from pathlib import Path

PAIRS = [
    ("metrics", "metrics"),
    ("performance", "performance"),
    ("portfolio", "portfolio"),
    ("factor_analysis", "factor"),
    ("simulation", "simulation"),
    ("optimization", "optimization"),
    ("data", "data"),
    ("risk", "risk"),
    ("attribution", "attribution"),
    ("report", "report"),
    ("runtime", "runtime"),
    ("extensions", "extensions"),
    ("viz", "viz"),
]

EVIDENCE = {
    "metrics": "README + docs/acceptance (metrics phases 2A-52b)",
    "performance": "README + docs/acceptance (performance 53-57)",
    "portfolio": "README + docs/acceptance (portfolio 58-59, 91)",
    "factor_analysis": "docs/acceptance/2026-10-23-iteration-5-*, phase-79/80/84/92",
    "simulation": "README (historical simulation acceptance)",
    "optimization": "docs/acceptance/2026-10-23-phases-68-71-portfolio-optimization-acceptance.md",
    "data": "NON_PORT (network/data providers)",
    "risk": "docs/acceptance/2026-10-23-phase-6*-*, phases-68-71",
    "attribution": "README (historical attribution acceptance)",
    "report": "docs/acceptance/2026-10-23-phase-84/88a/91, iteration-6 html/xlsx",
    "runtime": "docs/acceptance/2026-10-23-phase-92-runtime-catalog-acceptance.md",
    "extensions": "NON_PORT (plugin loading)",
    "viz": "docs/acceptance/2026-10-23-iteration-4-visualization-acceptance.md",
}

RENAMES = {
    "cum_returns": "cumulative_returns",
    "cum_returns_final": "cumulative_return",
    "value_at_risk": "historical_value_at_risk",
    "get_max_drawdown": "max_drawdown",
    "get_max_drawdown_period": "max_drawdown_period",
    "get_max_drawdown_underwater": "drawdown_series",
    "alpha_aligned": "alpha",
    "beta_aligned": "beta",
    "alpha_beta_aligned": "alpha_beta",
    "beta_fragility_heuristic_aligned": "beta_fragility_heuristic",
    "roll_alpha_aligned": "roll_alpha",
    "roll_beta_aligned": "roll_beta",
    "roll_alpha_beta_aligned": "roll_alpha_beta",
    "gpd_risk_estimates_aligned": "gpd_risk_estimates",
    "treynor_ratio": "cal_treynor_ratio",
    "stock_market_correlation": "safe_correlation",
    "bond_market_correlation": "safe_correlation",
    "futures_market_correlation": "safe_correlation",
    "compute_mean_returns_spread": "mean_return_spread",
    "build_information_table": "build_factor_report",
    "build_quantile_statistics_table": "build_factor_report",
    "build_returns_table": "build_factor_report",
    "build_turnover_tables": "build_factor_report",
    "extract_pos": "stack_positions",
    "positions": "factor_positions",
    "research_capabilities": "research_capabilities_report",
    "optimize": "optimize_portfolio",
    "bootstrap": "bootstrap_distribution",
    "bootstrap_ci": "bootstrap_confidence_interval",
    "compute_statistics": "analyze_simulation_paths",
    "estimate_parameters": "estimate_simulation_parameters",
    "normalize_and_stack_positions": "stack_positions",
    "align_time_series": "align_on_common_index",
    "align_binary_metric_inputs": "align_on_common_index",
    "groupby_consecutive": "groupby_consecutive_transactions",
    "get_backend": "plot_series_chart",
    "ops": "plot_series_chart",
}

NON_PORT = {
    "fingerprint_value",
    "frozen_mapping",
    "snapshot_pandas",
    "serializable_value",
    "normalize_research_evaluation_spec",
    "compute_exposures_internal",
    "plotting_context",
    "show_owned_figures",
    "make_operations_provider",
    "compose_catalog",
    "load_optional_module",
    "discover_extensions",
    "load_extension",
    "operations",
    "validate_returns",
    "validate_finite_array",
    "validate_mapping",
    "normalize_time_series_timezone",
    "validate_time_series_timezones",
    "get_forward_returns_columns",
    "make_transaction_frame",
    "map_transaction",
}

CALENDAR_NAMES = {
    "add_custom_calendar_timedelta",
    "diff_custom_calendar_timedeltas",
    "infer_trading_calendar",
    "backshift_returns_series",
    "timedelta_strings_to_integers",
    "timedelta_to_string",
}

DEFERRED_BAYES = {
    "calc_bootstrap",
    "forecast_cone_bootstrap",
    "simulate_paths",
    "run_model",
    "model_best",
    "model_returns_normal",
    "model_returns_t",
    "model_returns_t_alpha_beta",
    "model_stoch_vol",
    "compute_bayes_cone",
    "summarize_paths",
    "to_pandas",
    "compute_consistency_score",
}

PANDAS_UTILS = {
    "aligned_series",
    "ensure_datetime_index_series",
    "pandas_frequency",
    "flatten",
}

NOTES = {
    "portfolio": "transaction/round-trip helper surface not ported",
    "factor_analysis": "event cumulative returns not ported",
    "attribution": "attribution date-alignment warning wrapper and idiosyncratic-risk helper not ported",
}


def deferred_reason(name: str) -> str:
    if name.startswith(("plot_", "create_")) or name in {
        "axes_style",
        "customize",
        "close_owned_figures",
        "deserialize_serializable_value",
    }:
        return "matplotlib/tear-sheet rendering; native ImPlot chart data instead"
    if name.startswith(("fetch_", "get_provider")):
        return "network/data provider excluded from native runtime scope"
    if name in DEFERRED_BAYES:
        return "Bayesian/bootstrap helper surface deferred"
    if name in CALENDAR_NAMES:
        return "calendar utility not exposed; business-day handling is internal"
    if name in PANDAS_UTILS:
        return "pandas utility helper; no native equivalent"
    return ""


def public_functions(package_root: Path) -> set[str]:
    names: set[str] = set()
    for source in package_root.glob("*.py"):
        if source.name.startswith("_"):
            continue
        try:
            tree = ast.parse(source.read_text())
        except SyntaxError:
            continue
        for node in tree.body:
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                if not node.name.startswith("_"):
                    names.add(node.name)
    return names


def header_family(cpp_root: Path, directory: str) -> str:
    target = cpp_root / directory
    names: list[str] = []
    if target.is_dir():
        names.extend(
            str(path.relative_to(cpp_root)) for path in sorted(target.glob("*.hpp"))
        )
    single = cpp_root / f"{directory}.hpp"
    if single.exists():
        names.append(str(single.relative_to(cpp_root)))
    return ";".join(names)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--python-root", required=True)
    parser.add_argument("--cpp-root", default="include/fincore")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    python_root = Path(args.python_root)
    cpp_root = Path(args.cpp_root)
    texts = {path: path.read_text() for path in cpp_root.rglob("*.hpp")}

    def find(name: str) -> bool:
        pattern = re.compile(r"(?<![\w:])" + re.escape(name) + r"\s*\(")
        return any(pattern.search(text) for text in texts.values())

    rows = []
    totals = [0] * 6
    for python_package, cpp_package in PAIRS:
        names = public_functions(python_root / python_package)
        exact: list[str] = []
        renamed: list[str] = []
        deferred: list[str] = []
        non_port: list[str] = []
        gaps: list[str] = []
        for name in sorted(names):
            if find(name):
                exact.append(name)
            elif name in RENAMES and find(RENAMES[name]):
                renamed.append(name)
            elif name in NON_PORT:
                non_port.append(name)
            elif deferred_reason(name):
                deferred.append(name)
            else:
                gaps.append(name)
        counts = [
            len(names),
            len(exact),
            len(renamed),
            len(deferred),
            len(non_port),
            len(gaps),
        ]
        totals = [left + right for left, right in zip(totals, counts)]
        rows.append((python_package, counts, gaps))

    with Path(args.output).open("w") as stream:
        stream.write(
            "family\tpython_route\tnative_headers\tpython_public\texact_name\t"
            "renamed_equivalent\tdeferred\tnon_port\tunresolved_gap\tevidence\t"
            "gap_names\tgap_notes\n"
        )
        cpp_directories = dict(PAIRS)
        for python_package, counts, gaps in rows:
            stream.write(
                "\t".join(
                    [
                        python_package,
                        f"fincore/{python_package}",
                        header_family(cpp_root, cpp_directories[python_package]),
                        *map(str, counts),
                        EVIDENCE[python_package],
                        ", ".join(gaps),
                        NOTES.get(python_package, ""),
                    ]
                )
                + "\n"
            )
        stream.write(
            "\t".join(
                ["TOTAL", "fincore/*", "include/fincore/**", *map(str, totals), "", "", ""]
            )
            + "\n"
        )
    print("wrote", args.output, "totals", totals)


if __name__ == "__main__":
    main()
