#!/usr/bin/env python3
"""Build fixed-index bootstrap parity fixtures from the pinned Python kernels."""

from __future__ import annotations

import argparse
import importlib
import json
import math
import pathlib
import subprocess
import sys
from typing import Any

import numpy as np
import pandas as pd
from scipy import stats as scipy_stats


PINNED_COMMIT = "4b8f31b1b59707480d5db9603704d83bbf7e6946"
ABSOLUTE_TOLERANCE = 1.0e-10
RELATIVE_TOLERANCE = 1.0e-9

PERFORMANCE_RETURNS = [
    0.01,
    -0.02,
    0.03,
    -0.01,
    0.02,
    -0.03,
    0.015,
    -0.005,
]
PERFORMANCE_DRAWS = [
    [0, 1, 2, 3, 4, 5, 6, 7],
    [0, 0, 1, 1, 4, 4, 6, 6],
    [1, 2, 3, 4, 5, 6, 7, 0],
    [0, 2, 3, 4, 5, 5, 7, 7],
    [0, 1, 3, 4, 5, 6, 6, 7],
    [0, 1, 2, 4, 5, 5, 7, 0],
    [0, 2, 2, 3, 4, 6, 6, 7],
    [1, 1, 2, 3, 4, 5, 7, 7],
]
PERFORMANCE_NAMES = [
    "Annual return",
    "Cumulative returns",
    "Annual volatility",
    "Sharpe ratio",
    "Calmar ratio",
    "Stability",
    "Max drawdown",
    "Omega ratio",
    "Sortino ratio",
    "Skew",
    "Kurtosis",
    "Tail ratio",
    "Daily value at risk",
]
PERFORMANCE_SUMMARY_FIELDS = [
    "mean",
    "median",
    "std",
    "p5",
    "p25",
    "p75",
    "p95",
    "iqr",
]
PERFORMANCE_SUMMARY_PYTHON_FIELDS = {
    "mean": "mean",
    "median": "median",
    "std": "std",
    "p5": "5%",
    "p25": "25%",
    "p75": "75%",
    "p95": "95%",
    "iqr": "IQR",
}

SIMULATION_RETURNS = [0.01, math.nan, 0.0, -0.02]
SIMULATION_DRAWS = [
    [1, 1, 1],
    [0, 0, 0],
    [2, 2, 2],
    [0, 1, 2],
    [2, 0, 2],
]
SIMULATION_STATISTICS = [
    ("mean", "mean"),
    ("sample_standard_deviation", "std"),
    ("median", "median"),
    ("sharpe_ratio", "sharpe"),
    ("sortino_ratio", "sortino"),
    ("minimum", "min"),
    ("maximum", "max"),
]


def encode_number(value: Any) -> dict[str, Any]:
    number = float(value)
    if math.isnan(number):
        return {"class": "nan"}
    if math.isinf(number):
        return {"class": "positive_infinity" if number > 0 else "negative_infinity"}
    return {"class": "finite", "value": number}


def python_fixture(source_root: pathlib.Path) -> dict[str, Any]:
    git_head = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=source_root,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if git_head != PINNED_COMMIT:
        raise RuntimeError(
            f"expected pinned fincore commit {PINNED_COMMIT}, found {git_head}"
        )
    git_status = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=all"],
        cwd=source_root,
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    if git_status:
        raise RuntimeError("pinned fincore source checkout is not clean")

    sys.path.insert(0, str(source_root))
    perf = importlib.import_module("fincore.metrics.perf_stats")
    simulation = importlib.import_module("fincore.simulation.bootstrap")
    for module in (perf, simulation):
        module_path = pathlib.Path(module.__file__).resolve()
        if not module_path.is_relative_to(source_root):
            raise RuntimeError(f"module imported outside pinned source tree: {module_path}")

    metric_functions = [
        perf.annual_return,
        perf.cum_returns_final,
        perf.annual_volatility,
        perf.sharpe_ratio,
        perf.calmar_ratio,
        perf.stability_of_timeseries,
        perf.max_drawdown,
        perf.omega_ratio,
        perf.sortino_ratio,
        scipy_stats.skew,
        scipy_stats.kurtosis,
        perf.tail_ratio,
        perf.value_at_risk,
    ]
    perf_returns = pd.Series(PERFORMANCE_RETURNS, dtype=float)
    perf_samples: list[list[dict[str, Any]]] = []
    for indices in PERFORMANCE_DRAWS:
        sample = perf_returns.iloc[indices].reset_index(drop=True)
        perf_samples.append(
            [encode_number(function(sample)) for function in metric_functions]
        )

    perf_sample_arrays = [
        np.asarray(
            [
                float("nan")
                if result["class"] == "nan"
                else float("inf")
                if result["class"] == "positive_infinity"
                else float("-inf")
                if result["class"] == "negative_infinity"
                else result["value"]
                for result in (row[column] for row in perf_samples)
            ],
            dtype=float,
        )
        for column in range(len(PERFORMANCE_NAMES))
    ]
    perf_summary_values = [
        perf.calc_distribution_stats(values) for values in perf_sample_arrays
    ]
    perf_summary = [
        [
            encode_number(summary[PERFORMANCE_SUMMARY_PYTHON_FIELDS[field]])
            for field in PERFORMANCE_SUMMARY_FIELDS
        ]
        for summary in perf_summary_values
    ]

    raw_simulation_values = np.asarray(SIMULATION_RETURNS, dtype=float)
    cleaned_simulation_values = raw_simulation_values[~np.isnan(raw_simulation_values)]
    simulation_samples = {
        name: [] for name, _ in SIMULATION_STATISTICS
    }
    statistic_functions = {
        name: simulation._get_statistic_fn(python_name)
        for name, python_name in SIMULATION_STATISTICS
    }
    for indices in SIMULATION_DRAWS:
        sample = cleaned_simulation_values[indices]
        for name, _ in SIMULATION_STATISTICS:
            simulation_samples[name].append(encode_number(statistic_functions[name](sample)))

    return {
        "schema": "fincore.bootstrap-fixed-indices.v1",
        "source": {
            "repository": "fincore",
            "commit": git_head,
            "modules": [
                "fincore.metrics.perf_stats",
                "fincore.simulation.bootstrap",
            ],
        },
        "tolerance": {
            "finite_absolute": ABSOLUTE_TOLERANCE,
            "finite_relative": RELATIVE_TOLERANCE,
            "nonfinite_classes": "exact",
            "metric_names_and_order": "exact",
        },
        "performance": {
            "returns": PERFORMANCE_RETURNS,
            "draw_indices": PERFORMANCE_DRAWS,
            "metric_names": PERFORMANCE_NAMES,
            "samples_by_draw_then_metric": perf_samples,
            "summary_fields": PERFORMANCE_SUMMARY_FIELDS,
            "summary_by_metric_then_field": perf_summary,
        },
        "simulation": {
            "returns": ["nan" if math.isnan(value) else value for value in SIMULATION_RETURNS],
            "draw_indices_after_nan_removal": SIMULATION_DRAWS,
            "statistic_names": [name for name, _ in SIMULATION_STATISTICS],
            "samples_by_statistic": simulation_samples,
        },
    }


def cpp_number(value: dict[str, Any]) -> str:
    number_class = value["class"]
    class_names = {
        "finite": "finite",
        "nan": "nan",
        "positive_infinity": "positive_infinity",
        "negative_infinity": "negative_infinity",
    }
    if number_class == "finite":
        number = format(value["value"], ".17g")
        if "." not in number and "e" not in number.lower():
            number += ".0"
        payload = number
    else:
        payload = "0.0"
    return f"expected_number{{expected_class::{class_names[number_class]}, {payload}}}"


def cpp_vector(values: list[Any]) -> str:
    return "{" + ", ".join(str(value) for value in values) + "}"


def cpp_matrix(values: list[list[int]]) -> str:
    return "{\n" + ",\n".join("    " + cpp_vector(row) for row in values) + "\n}"


def render_header(fixture: dict[str, Any]) -> str:
    performance = fixture["performance"]
    simulation = fixture["simulation"]
    perf_samples = performance["samples_by_draw_then_metric"]
    perf_summary = performance["summary_by_metric_then_field"]
    sim_samples = simulation["samples_by_statistic"]

    raw_values = [
        "std::numeric_limits<double>::quiet_NaN()" if value == "nan" else format(value, ".17g")
        for value in simulation["returns"]
    ]
    header = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <limits>",
        "#include <string_view>",
        "#include <vector>",
        "",
        "namespace bootstrap_fixed_indices_fixture {",
        "",
        "enum class expected_class { finite, nan, positive_infinity, negative_infinity };",
        "struct expected_number { expected_class value_class; double value; };",
        "",
        "inline constexpr double absolute_tolerance = 1.0e-10;",
        "inline constexpr double relative_tolerance = 1.0e-9;",
        "",
        "inline const std::vector<double> performance_returns "
        + cpp_vector(performance["returns"])
        + ";",
        "inline const std::vector<std::vector<std::size_t>> performance_draws "
        + cpp_matrix(performance["draw_indices"])
        + ";",
        "inline constexpr std::array<std::string_view, 13> performance_names{{",
        "    " + ",\n    ".join(json.dumps(name) for name in performance["metric_names"]),
        "}};",
        "inline const std::vector<std::vector<expected_number>> performance_samples_by_draw{",
        ",\n".join(
            "    {" + ", ".join(cpp_number(value) for value in row) + "}"
            for row in perf_samples
        ),
        "};",
        "inline const std::array<std::array<expected_number, 8>, 13> performance_summary{{",
        ",\n".join(
            "    std::array<expected_number, 8>{{"
            + ", ".join(cpp_number(value) for value in summary)
            + "}}"
            for summary in perf_summary
        ),
        "}};",
        "",
        "inline const std::vector<double> simulation_returns "
        + cpp_vector(raw_values)
        + ";",
        "inline const std::vector<std::vector<std::size_t>> simulation_draws "
        + cpp_matrix(simulation["draw_indices_after_nan_removal"])
        + ";",
        "inline constexpr std::array<std::string_view, 7> simulation_names{{",
        "    " + ",\n    ".join(json.dumps(name) for name in simulation["statistic_names"]),
        "}};",
        "inline const std::array<std::vector<expected_number>, 7> simulation_samples{{",
        ",\n".join(
            "    std::vector<expected_number>{"
            + ", ".join(cpp_number(value) for value in sim_samples[name])
            + "}"
            for name in simulation["statistic_names"]
        ),
        "}};",
        "",
        "}  // namespace bootstrap_fixed_indices_fixture",
        "",
    ]
    return "\n".join(header)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        type=pathlib.Path,
        default=pathlib.Path("/Users/yunjinqi/Documents/new_projects/fincore"),
        help="Pinned Python fincore checkout",
    )
    parser.add_argument(
        "--json-output",
        type=pathlib.Path,
        default=pathlib.Path("tests/fixtures/bootstrap_fixed_indices.json"),
    )
    parser.add_argument(
        "--header-output",
        type=pathlib.Path,
        default=pathlib.Path("tests/fixtures/bootstrap_fixed_indices.hpp"),
    )
    arguments = parser.parse_args()

    source_root = arguments.source.resolve()
    fixture = python_fixture(source_root)
    for output_path, contents in (
        (arguments.json_output, json.dumps(fixture, indent=2, allow_nan=False) + "\n"),
        (arguments.header_output, render_header(fixture)),
    ):
        path = output_path.resolve()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
