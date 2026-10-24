#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace fincore::detail {

struct nelder_mead_options {
    double function_tolerance = 1e-12;
    double simplex_tolerance = 1e-10;
    std::size_t max_iterations = 4000U;
};

struct nelder_mead_result {
    std::vector<double> parameters;
    double value{0.0};
    std::size_t iterations{0U};
    bool converged{false};
};

// Returns converged=false on iteration exhaustion or when no finite best
// objective value was evaluated. Invalid starts/options throw input_error.
nelder_mead_result minimize_nelder_mead(
    const std::function<double(const std::vector<double>&)>& objective,
    std::vector<double> start,
    const nelder_mead_options& options = {});

// Projects onto {x: lower <= x_i <= upper, sum(x) == target_sum}.
// Invalid dimensions, bounds, targets, or non-finite values throw input_error.
std::vector<double> project_onto_simplex(const std::vector<double>& values,
                                         double lower = 0.0,
                                         double upper = 1.0,
                                         double target_sum = 1.0);

struct projected_gradient_options {
    double gradient_tolerance = 1e-10;
    double step_tolerance = 1e-14;
    std::size_t max_iterations = 20000U;
};

struct projected_gradient_result {
    std::vector<double> parameters;
    double value{0.0};
    double projected_gradient_norm{0.0};
    std::size_t iterations{0U};
    bool converged{false};
};

// Solves a symmetric-Hessian quadratic over the unit simplex. A zero iteration
// budget returns the projected start with converged=false; malformed inputs
// throw input_error.
projected_gradient_result minimize_quadratic_on_simplex(
    const std::vector<double>& hessian,
    const std::vector<double>& linear,
    std::vector<double> start,
    const projected_gradient_options& options = {});

double quadratic_objective(const std::vector<double>& hessian,
                           const std::vector<double>& linear,
                           const std::vector<double>& point);

}  // namespace fincore::detail
