#include <fincore/optimization/frontier.hpp>

#include "portfolio_solver.hpp"

#include <fincore/detail/optimization.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace fincore {
namespace {

using detail::portfolio_problem;
using detail::solver_constraints;
using detail::solver_objective;

// Jacobi rotations drive the off-diagonals away; acceptance is deliberately
// tolerant (relative asymmetry up to 1e-10, diagonals down to -1e-10) because
// the question is whether the quadratic solver downstream can be trusted, not
// whether the matrix is strictly positive definite.
bool positive_semidefinite(std::vector<double> matrix,
                           const std::size_t assets) {
    for (std::size_t row = 0; row < assets; ++row) {
        for (std::size_t column = 0; column < assets; ++column) {
            if (!std::isfinite(matrix[row * assets + column])) {
                return false;
            }
            if (std::abs(matrix[row * assets + column] -
                         matrix[column * assets + row]) >
                1e-10 * std::max(
                             1.0,
                             std::abs(matrix[row * assets + column]))) {
                return false;
            }
        }
    }
    for (std::size_t iteration = 0; iteration < 100U * assets * assets;
         ++iteration) {
        std::size_t pivot_row = 0U;
        std::size_t pivot_column = 1U;
        double maximum = 0.0;
        for (std::size_t row = 0; row < assets; ++row) {
            for (std::size_t column = row + 1U; column < assets; ++column) {
                const double magnitude =
                    std::abs(matrix[row * assets + column]);
                if (magnitude > maximum) {
                    maximum = magnitude;
                    pivot_row = row;
                    pivot_column = column;
                }
            }
        }
        if (maximum < 1e-14) {
            break;
        }
        const double diagonal_difference =
            matrix[pivot_column * assets + pivot_column] -
            matrix[pivot_row * assets + pivot_row];
        const double angle = 0.5 * std::atan2(
                                       2.0 * matrix[pivot_row * assets +
                                                   pivot_column],
                                       diagonal_difference);
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        for (std::size_t index = 0; index < assets; ++index) {
            if (index == pivot_row || index == pivot_column) {
                continue;
            }
            const double left = matrix[index * assets + pivot_row];
            const double right = matrix[index * assets + pivot_column];
            matrix[index * assets + pivot_row] =
                cosine * left - sine * right;
            matrix[pivot_row * assets + index] =
                matrix[index * assets + pivot_row];
            matrix[index * assets + pivot_column] =
                sine * left + cosine * right;
            matrix[pivot_column * assets + index] =
                matrix[index * assets + pivot_column];
        }
        const double pivot_left =
            matrix[pivot_row * assets + pivot_row];
        const double pivot_right =
            matrix[pivot_column * assets + pivot_column];
        const double pivot_off =
            matrix[pivot_row * assets + pivot_column];
        matrix[pivot_row * assets + pivot_row] =
            cosine * cosine * pivot_left -
            2.0 * sine * cosine * pivot_off +
            sine * sine * pivot_right;
        matrix[pivot_column * assets + pivot_column] =
            sine * sine * pivot_left +
            2.0 * sine * cosine * pivot_off +
            cosine * cosine * pivot_right;
        matrix[pivot_row * assets + pivot_column] = 0.0;
        matrix[pivot_column * assets + pivot_row] = 0.0;
    }
    for (std::size_t index = 0; index < assets; ++index) {
        if (matrix[index * assets + index] < -1e-10) {
            return false;
        }
    }
    return true;
}

frontier_point_diagnostics diagnostics_for(
    const portfolio_problem& problem,
    const solver_constraints& constraints,
    const std::vector<double>& weights,
    const frontier_solver solver) {
    frontier_point_diagnostics diagnostics;
    diagnostics.solver = solver;
    diagnostics.sum_residual = std::abs(
        std::accumulate(weights.begin(), weights.end(), 0.0) - 1.0);
    for (const double weight : weights) {
        diagnostics.lower_bound_residual = std::max(
            diagnostics.lower_bound_residual, constraints.lower - weight);
        diagnostics.upper_bound_residual = std::max(
            diagnostics.upper_bound_residual, weight - constraints.upper);
    }
    if (constraints.has_target_return) {
        diagnostics.target_return_residual = std::abs(
            detail::portfolio_return(problem, weights) -
            constraints.target_return);
    }
    return diagnostics;
}

solver_objective variance_objective(const portfolio_problem& problem) {
    solver_objective objective;
    objective.value = [&problem](const std::vector<double>& weights) {
        return detail::portfolio_variance(problem, weights);
    };
    objective.gradient = [&problem](const std::vector<double>& weights) {
        auto gradient = detail::covariance_product(problem, weights);
        for (double& value : gradient) {
            value *= 2.0;
        }
        return gradient;
    };
    return objective;
}

solver_objective sharpe_objective(const portfolio_problem& problem,
                                  const double risk_free_rate) {
    const std::size_t assets = problem.mean.size();
    solver_objective objective;
    objective.value = [&problem, risk_free_rate](
                          const std::vector<double>& weights) {
        // Maximizing Sharpe means minimizing its negation. A vanishing
        // denominator is scored as a large positive penalty so the search
        // retreats from zero-volatility corners, and its gradient offers no
        // direction there rather than an extrapolated one.
        const double volatility =
            detail::portfolio_volatility(problem, weights);
        if (volatility <= 1e-12) {
            return 1e6;
        }
        return -(detail::portfolio_return(problem, weights) -
                 risk_free_rate) /
               volatility;
    };
    objective.gradient = [&problem, risk_free_rate, assets](
                             const std::vector<double>& weights) {
        const double volatility =
            detail::portfolio_volatility(problem, weights);
        if (volatility <= 1e-12) {
            return std::vector<double>(assets, 0.0);
        }
        const double excess =
            detail::portfolio_return(problem, weights) - risk_free_rate;
        const auto marginal = detail::covariance_product(problem, weights);
        std::vector<double> gradient(assets, 0.0);
        for (std::size_t index = 0; index < assets; ++index) {
            gradient[index] =
                -(problem.mean[index] * volatility -
                  excess * marginal[index] / volatility) /
                (volatility * volatility);
        }
        return gradient;
    };
    return objective;
}

}  // namespace

efficient_frontier_result efficient_frontier(
    const dataframe& returns, const frontier_config& config) {
    if (returns.get_index().empty()) {
        throw input_error{"returns must be a non-empty DataFrame"};
    }
    if (returns.get_index().size() < 2U) {
        throw input_error{
            "At least 2 observations are required for frontier computation"};
    }
    if (config.n_points < 2U) {
        throw input_error{"n_points must be >= 2"};
    }
    if (!std::isfinite(config.max_weight) || config.max_weight <= 0.0) {
        throw input_error{"max_weight must be > 0"};
    }
    if (!std::isfinite(config.periods_per_year) ||
        config.periods_per_year <= 0.0) {
        throw input_error{
            "periods_per_year must be a positive finite value"};
    }
    if (!std::isfinite(config.risk_free_rate)) {
        throw input_error{"risk_free_rate must be finite"};
    }

    const auto problem =
        detail::load_portfolio_problem(returns, config.periods_per_year);
    const std::size_t assets = problem.mean.size();
    if (assets < 2U) {
        throw input_error{
            "At least 2 assets required for frontier computation"};
    }
    if (!positive_semidefinite(problem.covariance, assets)) {
        throw optimization_error{
            "covariance matrix is not positive semidefinite"};
    }

    solver_constraints constraints;
    constraints.lower = config.short_allowed ? -config.max_weight : 0.0;
    constraints.upper = config.max_weight;
    if (static_cast<double>(assets) * config.max_weight < 1.0 - 1e-12) {
        throw optimization_error{
            "weight bounds are infeasible: the sum of maximum asset weights "
            "is below 1"};
    }

    efficient_frontier_result result;
    result.asset_names = problem.asset_names;
    result.periods_per_year = config.periods_per_year;

    std::vector<double> hessian(assets * assets, 0.0);
    for (std::size_t index = 0; index < assets * assets; ++index) {
        hessian[index] = 2.0 * problem.covariance[index];
    }
    const std::vector<double> linear(assets, 0.0);
    const std::vector<double> start(assets,
                                    1.0 / static_cast<double>(assets));
    detail::projected_gradient_options qp_options;
    qp_options.gradient_tolerance = 1e-12;
    qp_options.step_tolerance = 1e-16;
    qp_options.max_iterations = 50000U;
    const auto mv = detail::minimize_quadratic_on_simplex(
        hessian, linear, start, qp_options);
    detail::validate_weights(problem, constraints, mv.parameters,
                             "min_variance");
    result.min_variance.weights = mv.parameters;
    result.min_variance.expected_return =
        detail::portfolio_return(problem, mv.parameters);
    result.min_variance.volatility =
        detail::portfolio_volatility(problem, mv.parameters);
    result.min_variance_diagnostics = diagnostics_for(
        problem, constraints, mv.parameters, frontier_solver::min_variance);

    const auto ms = detail::minimize_projected(
        problem, constraints,
        sharpe_objective(problem, config.risk_free_rate), start);
    detail::validate_weights(problem, constraints, ms, "max_sharpe");
    result.max_sharpe.weights = ms;
    result.max_sharpe.expected_return =
        detail::portfolio_return(problem, ms);
    result.max_sharpe.volatility =
        detail::portfolio_volatility(problem, ms);
    result.max_sharpe_ratio =
        result.max_sharpe.volatility > 1e-12
            ? (result.max_sharpe.expected_return - config.risk_free_rate) /
                  result.max_sharpe.volatility
            : 0.0;
    result.max_sharpe_diagnostics = diagnostics_for(
        problem, constraints, ms, frontier_solver::projected_gradient);

    // The maximum-return endpoint is a corner of the weight box rather than a
    // quadratic solve: filling in descending-mean order up to the upper bound
    // is exactly optimal for a linear objective, and equal means keep their
    // original asset order because the sort is stable.
    std::vector<std::size_t> order(assets);
    std::iota(order.begin(), order.end(), 0U);
    std::stable_sort(order.begin(), order.end(),
                     [&problem](const std::size_t left,
                                const std::size_t right) {
                         return problem.mean[left] > problem.mean[right];
                     });
    std::vector<double> maximum_weights(assets, constraints.lower);
    double remaining = 1.0 - static_cast<double>(assets) * constraints.lower;
    const double capacity = constraints.upper - constraints.lower;
    for (const std::size_t asset : order) {
        const double allocation = std::min(capacity, remaining);
        maximum_weights[asset] += allocation;
        remaining -= allocation;
        if (remaining <= 1e-14) {
            break;
        }
    }
    if (remaining > 1e-10) {
        throw optimization_error{
            "weight bounds cannot produce a fully invested portfolio"};
    }
    detail::validate_weights(problem, constraints, maximum_weights,
                             "max_return_endpoint");
    const double minimum_return = result.min_variance.expected_return;
    const double maximum_return =
        detail::portfolio_return(problem, maximum_weights);
    // A span this small means every target asks the solver for the book it
    // already returned, so the whole sweep collapses onto min-variance rather
    // than emitting points that differ only by tolerance noise.
    const bool degenerate =
        std::abs(maximum_return - minimum_return) <= 1e-12;

    result.frontier_returns.resize(config.n_points);
    result.frontier_volatilities.resize(config.n_points);
    result.frontier_sharpe.resize(config.n_points);
    result.frontier_weights.resize(config.n_points);
    result.frontier_diagnostics.reserve(config.n_points);

    std::vector<double> previous = mv.parameters;
    for (std::size_t point = 0; point < config.n_points; ++point) {
        const double fraction =
            static_cast<double>(point) /
            static_cast<double>(config.n_points - 1U);
        const double target =
            minimum_return + fraction * (maximum_return - minimum_return);
        std::vector<double> weights;
        frontier_solver solver = frontier_solver::projected_gradient;
        // Only interior points pay for a solve, and each starts from the
        // previous (lower-target) portfolio rather than equal weights, so the
        // sweep walks the frontier instead of re-converging from scratch.
        if (degenerate) {
            weights = mv.parameters;
            solver = frontier_solver::min_variance_degenerate;
        } else if (point == 0U) {
            weights = mv.parameters;
            solver = frontier_solver::min_variance;
        } else if (point + 1U == config.n_points) {
            weights = maximum_weights;
            solver = frontier_solver::bounded_linear_max_return;
        } else {
            solver_constraints target_constraints = constraints;
            target_constraints.has_target_return = true;
            target_constraints.target_return = target;
            weights = detail::minimize_projected(
                problem, target_constraints,
                variance_objective(problem), previous);
            detail::validate_weights(problem, target_constraints, weights,
                                     "frontier_point");
        }
        // Diagnostics have to describe the constraint set the solver actually
        // faced, so a target-return residual is recorded only for interior
        // points, which are the only ones solved against a target.
        solver_constraints point_constraints = constraints;
        if (!degenerate && point != 0U &&
            point + 1U != config.n_points) {
            point_constraints.has_target_return = true;
            point_constraints.target_return = target;
        }
        result.frontier_weights[point] = weights;
        result.frontier_returns[point] =
            detail::portfolio_return(problem, weights);
        result.frontier_volatilities[point] =
            detail::portfolio_volatility(problem, weights);
        result.frontier_sharpe[point] =
            result.frontier_volatilities[point] > 1e-12
                ? (result.frontier_returns[point] - config.risk_free_rate) /
                      result.frontier_volatilities[point]
                : 0.0;
        result.frontier_diagnostics.push_back(
            diagnostics_for(problem, point_constraints, weights, solver));
        previous = std::move(weights);
    }
    return result;
}

}  // namespace fincore
