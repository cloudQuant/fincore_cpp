#!/usr/bin/env python3
"""Compare the supported numerical solver classes with SciPy references.

Run with the repository's Anaconda environment:
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python \
    tools/oracles/solver_spike.py

The temporary C++ runner compiles the checked-in numerics.cpp directly. Its
runtime and peak RSS are characterization data, not a historical benchmark.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import resource
import shlex
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable

import numpy as np
import scipy
from scipy.optimize import minimize


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "docs/evidence/revised-plan-20261023/solver/fixture_manifest.json"
DEFAULT_OUTPUT = (
    ROOT / "docs/evidence/revised-plan-20261023/solver/comparison.json"
)


CPP_RUNNER = r'''#include <fincore/detail/optimization.hpp>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using fincore::detail::minimize_nelder_mead;
using fincore::detail::minimize_quadratic_on_simplex;
using fincore::detail::nelder_mead_options;
using fincore::detail::project_onto_simplex;
using fincore::detail::projected_gradient_options;

void number(const double value) {
    if (std::isfinite(value)) {
        std::cout << value;
    } else {
        std::cout << "null";
    }
}

void vector(const std::vector<double>& values) {
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0U) std::cout << ',';
        number(values[index]);
    }
    std::cout << ']';
}

template <class Objective>
void nelder_case(const char* name, const std::vector<double>& start,
                 Objective objective, const std::size_t repetitions,
                 const nelder_mead_options& options = {}) {
    using clock = std::chrono::steady_clock;
    auto result = minimize_nelder_mead(objective, start, options);
    const auto before = clock::now();
    for (std::size_t run = 0; run < repetitions; ++run) {
        result = minimize_nelder_mead(objective, start, options);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        clock::now() - before).count();
    std::cout << "\"" << name << "\":{\"parameters\":";
    vector(result.parameters);
    std::cout << ",\"value\":";
    number(result.value);
    std::cout << ",\"converged\":" << (result.converged ? "true" : "false")
              << ",\"iterations\":" << result.iterations
              << ",\"mean_ns\":"
              << static_cast<double>(elapsed) / static_cast<double>(repetitions)
              << '}';
}

void quadratic_case(const char* name, const std::vector<double>& hessian,
                    const std::vector<double>& linear,
                    const std::vector<double>& start,
                    projected_gradient_options options,
                    const std::size_t repetitions) {
    using clock = std::chrono::steady_clock;
    auto result = minimize_quadratic_on_simplex(hessian, linear, start, options);
    const auto before = clock::now();
    for (std::size_t run = 0; run < repetitions; ++run) {
        result = minimize_quadratic_on_simplex(hessian, linear, start, options);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        clock::now() - before).count();
    std::cout << "\"" << name << "\":{\"parameters\":";
    vector(result.parameters);
    std::cout << ",\"value\":";
    number(result.value);
    std::cout << ",\"projected_gradient_norm\":";
    number(result.projected_gradient_norm);
    std::cout << ",\"converged\":" << (result.converged ? "true" : "false")
              << ",\"iterations\":" << result.iterations
              << ",\"mean_ns\":"
              << static_cast<double>(elapsed) / static_cast<double>(repetitions)
              << '}';
}

int main(int argc, char** argv) {
    const std::size_t repetitions =
        argc > 1 ? static_cast<std::size_t>(std::stoul(argv[1])) : 10U;
    std::cout << std::setprecision(17) << '{';
    nelder_case("quadratic_2d", {0.0, 0.0}, [](const auto& point) {
        const double x = point[0] - 1.0;
        const double y = point[1] + 0.5;
        return x * x + 3.0 * y * y;
    }, repetitions);
    std::cout << ',';
    nelder_case("rosenbrock_2d", {-1.2, 1.0}, [](const auto& point) {
        const double x = point[0];
        const double y = point[1];
        return (1.0 - x) * (1.0 - x) + 100.0 * (y - x * x) * (y - x * x);
    }, repetitions);
    std::cout << ',';
    nelder_case("ill_conditioned_quadratic_2d", {0.0, 0.0}, [](const auto& point) {
        const double x = point[0] - 2.0;
        const double y = point[1] + 3.0;
        return x * x + 1e4 * y * y;
    }, repetitions);
    std::cout << ',';
    nelder_mead_options short_run;
    short_run.function_tolerance = 0.0;
    short_run.simplex_tolerance = 0.0;
    short_run.max_iterations = 1U;
    nelder_case("finite_exhaustion", {1.0}, [](const auto& point) {
        return point[0] * point[0];
    }, repetitions, short_run);
    std::cout << ',';
    nelder_mead_options nm_no_iterations;
    nm_no_iterations.max_iterations = 0U;
    nelder_case("zero_iteration_nm", {1.0}, [](const auto& point) {
        return point[0] * point[0];
    }, repetitions, nm_no_iterations);
    std::cout << ',';
    nelder_mead_options nonfinite_options;
    nonfinite_options.max_iterations = 25U;
    nelder_case("nan_objective", {1.0}, [](const auto&) {
        return std::numeric_limits<double>::quiet_NaN();
    }, repetitions, nonfinite_options);
    std::cout << ',';
    nelder_case("infinite_objective", {1.0}, [](const auto&) {
        return std::numeric_limits<double>::infinity();
    }, repetitions, nonfinite_options);
    std::cout << ',';
    using clock = std::chrono::steady_clock;
    auto projection = project_onto_simplex({-4.0, 2.0, 10.0}, 0.1, 0.6, 1.0);
    const auto projection_before = clock::now();
    for (std::size_t run = 0; run < repetitions; ++run) {
        projection = project_onto_simplex({-4.0, 2.0, 10.0}, 0.1, 0.6, 1.0);
    }
    const auto projection_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        clock::now() - projection_before).count();
    std::cout << "\"bounded_simplex_boundary\":{\"parameters\":";
    vector(projection);
    std::cout << ",\"mean_ns\":"
              << static_cast<double>(projection_elapsed) / static_cast<double>(repetitions)
              << '}';
    std::cout << ',';
    projected_gradient_options ill_conditioned_options;
    ill_conditioned_options.gradient_tolerance = 1e-8;
    ill_conditioned_options.max_iterations = 50000U;
    quadratic_case("ill_conditioned_diagonal_qp_cond100", {1.0, 0.0, 0.0, 100.0},
                   {0.0, 0.0}, {0.5, 0.5}, ill_conditioned_options,
                   repetitions);
    std::cout << ',';
    quadratic_case("singular_diagonal_qp", {0.0, 0.0, 0.0, 1.0},
                   {0.0, 0.0}, {0.2, 0.8}, {}, repetitions);
    std::cout << ',';
    projected_gradient_options no_iterations;
    no_iterations.max_iterations = 0U;
    quadratic_case("zero_iteration_qp", {2.0, 0.0, 0.0, 4.0},
                   {0.0, 0.0}, {0.5, 0.5}, no_iterations, repetitions);
    std::cout << '}';
    return 0;
}
'''


def within(actual: float, reference: float, tolerance: dict[str, float]) -> bool:
    return abs(actual - reference) <= tolerance["atol"] + tolerance["rtol"] * abs(reference)


def vector_comparison(
    actual: list[float], reference: list[float], tolerance: dict[str, float]
) -> dict[str, Any]:
    if len(actual) != len(reference):
        return {"passed": False, "reason": "dimension mismatch"}
    errors = [abs(left - right) for left, right in zip(actual, reference)]
    passed = all(within(left, right, tolerance) for left, right in zip(actual, reference))
    return {"passed": passed, "max_abs_error": max(errors, default=0.0)}


def quadratic_objective(
    hessian: np.ndarray, linear: np.ndarray, point: np.ndarray
) -> float:
    return float(0.5 * point @ hessian @ point + linear @ point)


def scipy_nelder_reference(case: dict[str, Any]) -> dict[str, Any]:
    name = case["name"]
    if name == "quadratic_2d":
        objective = lambda point: (point[0] - 1.0) ** 2 + 3.0 * (point[1] + 0.5) ** 2
        gradient = lambda point: np.array(
            [2.0 * (point[0] - 1.0), 6.0 * (point[1] + 0.5)]
        )
    elif name == "rosenbrock_2d":
        objective = lambda point: (1.0 - point[0]) ** 2 + 100.0 * (point[1] - point[0] ** 2) ** 2
        gradient = lambda point: np.array(
            [
                -2.0 * (1.0 - point[0]) - 400.0 * point[0] * (point[1] - point[0] ** 2),
                200.0 * (point[1] - point[0] ** 2),
            ]
        )
    elif name == "ill_conditioned_quadratic_2d":
        objective = lambda point: (point[0] - 2.0) ** 2 + 1e4 * (point[1] + 3.0) ** 2
        gradient = lambda point: np.array(
            [2.0 * (point[0] - 2.0), 2e4 * (point[1] + 3.0)]
        )
    else:
        raise ValueError(f"no finite SciPy reference for {name}")
    return _timed_minimize(
        objective,
        jac=gradient,
        x0=np.asarray(case["start"], dtype=float),
        method="BFGS",
        options={"gtol": 1e-10, "maxiter": 10000},
    )


def _timed_minimize(objective: Callable[..., float], **kwargs: Any) -> dict[str, Any]:
    result = minimize(objective, **kwargs)
    return {
        "parameters": np.asarray(result.x, dtype=float).tolist(),
        "value": float(result.fun),
        "success": bool(result.success),
        "message": str(result.message),
        "iterations": int(result.nit),
    }


def measure_scipy_references(
    manifest: dict[str, Any], repetitions: int
) -> tuple[dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    nm_results: dict[str, dict[str, Any]] = {}
    for case in manifest["nelder_mead"]:
        if case["name"] in {
            "nan_objective",
            "infinite_objective",
            "finite_exhaustion",
            "zero_iteration_nm",
        }:
            continue
        reference = scipy_nelder_reference(case)
        samples = []
        for _ in range(repetitions):
            started = time.perf_counter_ns()
            reference = scipy_nelder_reference(case)
            samples.append(time.perf_counter_ns() - started)
        reference["mean_ns"] = sum(samples) / len(samples)
        nm_results[case["name"]] = reference

    projection = manifest["projection"]
    source = np.asarray(projection["values"], dtype=float)
    bounds = [(projection["lower"], projection["upper"])] * source.size
    projection_objective = lambda point: 0.5 * float(np.sum((point - source) ** 2))
    projection_gradient = lambda point: point - source
    projection_constraint = {
        "type": "eq",
        "fun": lambda point: float(np.sum(point) - projection["target_sum"]),
        "jac": lambda point: np.ones_like(point),
    }
    projection_reference = minimize(
        projection_objective,
        np.asarray(projection["reference_parameters"], dtype=float),
        jac=projection_gradient,
        method="SLSQP",
        bounds=bounds,
        constraints=[projection_constraint],
        options={"ftol": 1e-14, "maxiter": 1000},
    )
    projection_samples = []
    for _ in range(repetitions):
        started = time.perf_counter_ns()
        projection_reference = minimize(
            projection_objective,
            np.asarray(projection["reference_parameters"], dtype=float),
            jac=projection_gradient,
            method="SLSQP",
            bounds=bounds,
            constraints=[projection_constraint],
            options={"ftol": 1e-14, "maxiter": 1000},
        )
        projection_samples.append(time.perf_counter_ns() - started)
    assert projection_reference is not None
    projection_ns = sum(projection_samples) / len(projection_samples)
    projection_result = {
        "parameters": np.asarray(projection_reference.x, dtype=float).tolist(),
        "value": float(projection_reference.fun),
        "success": bool(projection_reference.success),
        "message": str(projection_reference.message),
        "iterations": int(projection_reference.nit),
        "mean_ns": projection_ns,
    }

    qp_results: dict[str, dict[str, Any]] = {}
    for case in manifest["quadratic_programs"]:
        hessian = np.asarray(case["hessian"], dtype=float).reshape(
            len(case["linear"]), len(case["linear"])
        )
        linear = np.asarray(case["linear"], dtype=float)
        start = np.asarray(case["start"], dtype=float)
        equality = {
            "type": "eq",
            "fun": lambda point: float(np.sum(point) - 1.0),
            "jac": lambda point: np.ones_like(point),
        }
        objective = lambda point: quadratic_objective(hessian, linear, point)
        gradient = lambda point: hessian @ point + linear
        options = {"ftol": 1e-14, "maxiter": 10000}
        if case["name"] == "zero_iteration_qp":
            reference = {
                "parameters": start.tolist(),
                "value": quadratic_objective(hessian, linear, start),
                "success": False,
                "message": "zero-iteration status fixture",
                "iterations": 0,
                "mean_ns": 0.0,
            }
        else:
            result = minimize(
                objective,
                start,
                jac=gradient,
                method="SLSQP",
                bounds=[(0.0, 1.0)] * start.size,
                constraints=[equality],
                options=options,
            )
            samples = []
            for _ in range(repetitions):
                started = time.perf_counter_ns()
                result = minimize(
                    objective,
                    start,
                    jac=gradient,
                    method="SLSQP",
                    bounds=[(0.0, 1.0)] * start.size,
                    constraints=[equality],
                    options=options,
                )
                samples.append(time.perf_counter_ns() - started)
            elapsed = sum(samples) / len(samples)
            reference = {
                "parameters": np.asarray(result.x, dtype=float).tolist(),
                "value": float(result.fun),
                "success": bool(result.success),
                "message": str(result.message),
                "iterations": int(result.nit),
                "mean_ns": elapsed,
            }
        qp_results[case["name"]] = reference
    return nm_results, {"projection": projection_result, **qp_results}


def compile_and_run_cpp(
    repetitions: int,
) -> tuple[dict[str, Any], str, str, int | None]:
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    with tempfile.TemporaryDirectory(prefix="fincore-phase60-solver-") as raw_dir:
        directory = Path(raw_dir)
        source = directory / "solver_spike.cpp"
        binary = directory / "solver_spike"
        source.write_text(CPP_RUNNER, encoding="utf-8")
        compile_command = compiler + [
            "-std=c++20",
            "-O2",
            "-DNDEBUG",
            "-I",
            str(ROOT / "include"),
            str(ROOT / "src/optimization/numerics.cpp"),
            str(source),
            "-o",
            str(binary),
        ]
        build = subprocess.run(
            compile_command,
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        if build.returncode != 0:
            raise RuntimeError(
                "C++ characterization runner failed to compile:\n"
                + build.stdout
                + build.stderr
            )
        time_command = ["/usr/bin/time", "-l"]
        run = subprocess.run(
            time_command + [str(binary), str(repetitions)],
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        if run.returncode != 0:
            raise RuntimeError(
                "C++ characterization runner failed:\n" + run.stdout + run.stderr
            )
        match = re.search(r"([0-9]+)\s+maximum resident set size", run.stderr)
        peak_rss = int(match.group(1)) if match else None
        return (
            json.loads(run.stdout),
            " ".join(shlex.quote(part) for part in compile_command),
            " ".join(shlex.quote(part) for part in time_command + [str(binary), str(repetitions)]),
            peak_rss,
        )


def validate_comparisons(
    manifest: dict[str, Any],
    cpp: dict[str, Any],
    scipy_nm: dict[str, dict[str, Any]],
    scipy_other: dict[str, dict[str, Any]],
) -> list[dict[str, Any]]:
    tolerances = manifest["tolerance_policy"]
    outcomes: list[dict[str, Any]] = []
    for case in manifest["nelder_mead"]:
        name = case["name"]
        actual = cpp[name]
        outcome: dict[str, Any] = {
            "name": name,
            "solver": "Nelder-Mead",
            "converged": actual["converged"],
            "iterations": actual["iterations"],
            "cpp_mean_ns": actual["mean_ns"],
            "expected_converged": case["expected_converged"],
        }
        passed = actual["converged"] is case["expected_converged"]
        if name in scipy_nm:
            reference = case["reference_parameters"]
            params = vector_comparison(
                actual["parameters"], reference, tolerances["nelder_mead_parameters"]
            )
            scipy_params = vector_comparison(
                scipy_nm[name]["parameters"],
                reference,
                tolerances["nelder_mead_parameters"],
            )
            value = actual["value"]
            value_passed = value is not None and within(
                value, case["reference_value"], tolerances["objective"]
            )
            scipy_value_passed = within(
                scipy_nm[name]["value"],
                case["reference_value"],
                tolerances["objective"],
            )
            outcome.update(
                {
                    "reference": scipy_nm[name],
                    "parameter_residual": params["max_abs_error"],
                    "scipy_parameter_residual": scipy_params["max_abs_error"],
                    "objective_abs_residual": (
                        abs(value - case["reference_value"]) if value is not None else None
                    ),
                    "scipy_objective_abs_residual": abs(
                        scipy_nm[name]["value"] - case["reference_value"]
                    ),
                    "parameter_passed": params["passed"],
                    "objective_passed": value_passed,
                    "scipy_reference_matches_fixture": (
                        scipy_params["passed"] and scipy_value_passed
                    ),
                }
            )
            passed = (
                passed
                and params["passed"]
                and value_passed
                and scipy_params["passed"]
                and scipy_value_passed
            )
        else:
            outcome["value"] = actual["value"]
            if case.get("expected_value") == "nonfinite":
                passed = passed and actual["value"] is None
                outcome["nonfinite_value_rejected"] = actual["value"] is None
            else:
                passed = passed and actual["value"] is not None and actual["value"] <= case["reference_value"]
                outcome["value_not_worse_than_start"] = actual["value"] is not None and actual["value"] <= case["reference_value"]
        outcome["passed"] = passed
        outcomes.append(outcome)

    projection = manifest["projection"]
    actual_projection = cpp[projection["name"]]["parameters"]
    projection_reference = scipy_other["projection"]
    projection_params = vector_comparison(
        actual_projection,
        projection["reference_parameters"],
        tolerances["projection_components"],
    )
    projection_sum = abs(sum(actual_projection) - projection["target_sum"])
    projection_lower = max(0.0, projection["lower"] - min(actual_projection))
    projection_upper = max(0.0, max(actual_projection) - projection["upper"])
    projection_objective = 0.5 * sum(
        (value - original) ** 2
        for value, original in zip(actual_projection, projection["values"])
    )
    projection_objective_residual = abs(
        projection_objective - projection_reference["value"]
    )
    scipy_projection_params = vector_comparison(
        projection_reference["parameters"],
        projection["reference_parameters"],
        tolerances["projection_components"],
    )
    projection_passed = (
        projection_params["passed"]
        and scipy_projection_params["passed"]
        and projection_objective_residual <= tolerances["objective"]["atol"]
        and projection_sum <= tolerances["feasibility"]["atol"]
        and projection_lower <= tolerances["feasibility"]["atol"]
        and projection_upper <= tolerances["feasibility"]["atol"]
    )
    outcomes.append(
        {
            "name": projection["name"],
            "solver": "bounded simplex projection",
            "reference": projection_reference,
            "parameter_residual": projection_params["max_abs_error"],
            "scipy_parameter_residual": scipy_projection_params["max_abs_error"],
            "scipy_reference_matches_fixture": scipy_projection_params["passed"],
            "objective_abs_residual": projection_objective_residual,
            "cpp_mean_ns": cpp[projection["name"]]["mean_ns"],
            "sum_residual": projection_sum,
            "lower_bound_violation": projection_lower,
            "upper_bound_violation": projection_upper,
            "passed": projection_passed,
        }
    )

    for case in manifest["quadratic_programs"]:
        name = case["name"]
        actual = cpp[name]
        reference = scipy_other[name]
        weights = actual["parameters"]
        sum_residual = abs(sum(weights) - 1.0)
        lower_violation = max(0.0, -min(weights))
        upper_violation = max(0.0, max(weights) - 1.0)
        objective_residual = abs(actual["value"] - case["reference_value"])
        params = vector_comparison(
            weights,
            case.get("reference_parameters", reference["parameters"]),
            tolerances["qp_parameters"],
        )
        gradient_passed = (
            case["name"] == "zero_iteration_qp"
            or actual["projected_gradient_norm"]
            <= tolerances["qp_projected_gradient_max"]
        )
        scipy_params = vector_comparison(
            reference["parameters"],
            case.get("reference_parameters", reference["parameters"]),
            tolerances["qp_parameters"],
        )
        scipy_objective_passed = within(
            reference["value"],
            case["reference_value"],
            tolerances["objective"],
        )
        passed = (
            actual["converged"] is case["expected_converged"]
            and objective_residual
            <= tolerances["objective"]["atol"]
            + tolerances["objective"]["rtol"] * abs(case["reference_value"])
            and sum_residual <= tolerances["feasibility"]["atol"]
            and lower_violation <= tolerances["feasibility"]["atol"]
            and upper_violation <= tolerances["feasibility"]["atol"]
            and gradient_passed
        )
        if case["name"] != "zero_iteration_qp":
            passed = (
                passed
                and params["passed"]
                and scipy_params["passed"]
                and scipy_objective_passed
                and reference["success"]
            )
        else:
            passed = passed and actual["iterations"] == 0 and actual["projected_gradient_norm"] > 0.0
        outcomes.append(
            {
                "name": name,
                "solver": "projected-gradient QP",
                "reference": reference,
                "converged": actual["converged"],
                "iterations": actual["iterations"],
                "parameter_residual": params["max_abs_error"],
                "scipy_parameter_residual": scipy_params["max_abs_error"],
                "objective_abs_residual": objective_residual,
                "scipy_objective_abs_residual": abs(
                    reference["value"] - case["reference_value"]
                ),
                "scipy_reference_matches_fixture": (
                    scipy_params["passed"] and scipy_objective_passed
                ),
                "projected_gradient_norm": actual["projected_gradient_norm"],
                "sum_residual": sum_residual,
                "lower_bound_violation": lower_violation,
                "upper_bound_violation": upper_violation,
                "cpp_mean_ns": actual["mean_ns"],
                "passed": passed,
            }
        )
    return outcomes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--repetitions", type=int, default=10)
    args = parser.parse_args()
    if args.repetitions < 1:
        parser.error("--repetitions must be at least 1")

    manifest_bytes = args.manifest.read_bytes()
    manifest = json.loads(manifest_bytes)
    cpp, compile_command, run_command, cpp_peak_rss = compile_and_run_cpp(
        args.repetitions
    )
    scipy_nm, scipy_other = measure_scipy_references(manifest, args.repetitions)
    outcomes = validate_comparisons(manifest, cpp, scipy_nm, scipy_other)
    python_peak_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if sys.platform != "darwin":
        python_peak_rss *= 1024
    status = "PASS" if all(item["passed"] for item in outcomes) else "GAP"
    source_commit = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True, capture_output=True
    ).stdout.strip()
    source_status = subprocess.run(
        ["git", "status", "--short"], cwd=ROOT, text=True, capture_output=True
    ).stdout.splitlines()
    report = {
        "schema_version": 1,
        "status": status,
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "classification": "fresh solver characterization; not a historical regression or release-performance claim",
        "source_commit": source_commit,
        "working_tree_dirty": bool(source_status),
        "manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
        "numerics_source_sha256": hashlib.sha256(
            (ROOT / "src/optimization/numerics.cpp").read_bytes()
        ).hexdigest(),
        "platform": platform.platform(),
        "python": sys.version,
        "numpy": np.__version__,
        "scipy": scipy.__version__,
        "repetitions": args.repetitions,
        "warmups": 1,
        "compile_command": compile_command,
        "run_command": run_command,
        "peak_rss_bytes": {
            "cpp_runner": cpp_peak_rss,
            "python_scipy_process": python_peak_rss,
            "cpp_measurement": "macOS /usr/bin/time -l, maximum resident set size; null if parser unavailable",
            "python_measurement": "resource.getrusage(RUSAGE_SELF).ru_maxrss; includes interpreter and NumPy/SciPy imports",
        },
        "cases": outcomes,
        "solver_observations": cpp,
        "scope": {
            "nelder_mead": ["smooth finite quadratic", "Rosenbrock", "finite ill-conditioned quadratic", "NaN", "infinity", "iteration exhaustion"],
            "simplex_projection": ["bounded feasible projection with active lower and upper bounds"],
            "quadratic_program": ["finite ill-conditioned diagonal Hessian", "singular positive-semidefinite Hessian", "zero iterations"],
            "not_characterized": ["arbitrary nonlinear constraints", "non-convex global optimization", "large-scale QP performance"],
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "output": str(args.output), "cases": len(outcomes)}, sort_keys=True))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
