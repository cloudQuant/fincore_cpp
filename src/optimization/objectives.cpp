#include <fincore/optimization/objectives.hpp>

#include "portfolio_solver.hpp"

#include <fincore/detail/optimization.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace fincore {
namespace {

using detail::portfolio_problem;
using detail::solver_constraints;
using detail::solver_objective;

solver_constraints build_constraints(
    const portfolio_optimization_config& config,
    const std::vector<std::string>& asset_names) {
    solver_constraints constraints;
    // An explicit min_weight is taken verbatim rather than combined with
    // short_allowed, so it can raise the floor above zero for a short-allowed
    // book or push it below zero for a long-only one.
    constraints.lower =
        config.min_weight.has_value()
            ? *config.min_weight
            : (config.short_allowed ? -config.max_weight : 0.0);
    constraints.upper = config.max_weight;
    if (config.objective == portfolio_objective::target_return) {
        constraints.has_target_return = true;
        constraints.target_return = *config.target_return;
    }
    if (config.objective == portfolio_objective::target_risk) {
        constraints.has_target_volatility = true;
        constraints.target_volatility = *config.target_volatility;
    }
    for (const auto& sector : config.sector_constraints) {
        detail::sector_bounds bounds;
        for (std::size_t index = 0; index < asset_names.size(); ++index) {
            const auto mapped = config.sector_map.find(asset_names[index]);
            if (mapped != config.sector_map.end() &&
                mapped->second == sector.sector) {
                bounds.indices.push_back(index);
            }
        }
        // Assets missing from sector_map stay unconstrained, and a named
        // sector holding none of them is dropped instead of failed: its bound
        // would be vacuous, and rejecting it would tie a valid book to the
        // vocabulary of the constraint list.
        if (bounds.indices.empty()) {
            continue;
        }
        bounds.lower = sector.lower;
        bounds.upper = sector.upper;
        constraints.sectors.push_back(std::move(bounds));
    }
    return constraints;
}

solver_objective make_objective(
    const portfolio_problem& problem,
    const portfolio_optimization_config& config) {
    const std::size_t assets = problem.mean.size();
    solver_objective objective;
    // The enum names what the caller wants to reach, not what gets minimized:
    // target_return sweeps variance with the return carried as a constraint,
    // and target_risk sweeps negative return with the volatility ceiling as
    // one, so the two pairs below repeat their unconstrained siblings.
    switch (config.objective) {
        case portfolio_objective::min_variance:
            objective.value = [&problem](const std::vector<double>& weights) {
                return detail::portfolio_variance(problem, weights);
            };
            objective.gradient = [&problem](
                                     const std::vector<double>& weights) {
                auto gradient = detail::covariance_product(problem, weights);
                for (double& value : gradient) {
                    value *= 2.0;
                }
                return gradient;
            };
            break;
        case portfolio_objective::max_sharpe:
            objective.value = [&problem, &config](
                                  const std::vector<double>& weights) {
                const double volatility =
                    detail::portfolio_volatility(problem, weights);
                if (volatility <= 1e-12) {
                    return 1e6;
                }
                return -(detail::portfolio_return(problem, weights) -
                         config.risk_free_rate) /
                       volatility;
            };
            objective.gradient = [&problem, &config, assets](
                                     const std::vector<double>& weights) {
                const double volatility =
                    detail::portfolio_volatility(problem, weights);
                if (volatility <= 1e-12) {
                    return std::vector<double>(assets, 0.0);
                }
                const double excess =
                    detail::portfolio_return(problem, weights) -
                    config.risk_free_rate;
                const auto marginal =
                    detail::covariance_product(problem, weights);
                std::vector<double> gradient(assets, 0.0);
                for (std::size_t index = 0; index < assets; ++index) {
                    gradient[index] =
                        -(problem.mean[index] * volatility -
                          excess * marginal[index] / volatility) /
                        (volatility * volatility);
                }
                return gradient;
            };
            break;
        case portfolio_objective::target_return:
            objective.value = [&problem](const std::vector<double>& weights) {
                return detail::portfolio_variance(problem, weights);
            };
            objective.gradient = [&problem](
                                     const std::vector<double>& weights) {
                auto gradient = detail::covariance_product(problem, weights);
                for (double& value : gradient) {
                    value *= 2.0;
                }
                return gradient;
            };
            break;
        case portfolio_objective::target_risk:
            objective.value = [&problem](const std::vector<double>& weights) {
                return -detail::portfolio_return(problem, weights);
            };
            objective.gradient = [&problem](
                                     const std::vector<double>&) {
                std::vector<double> gradient(problem.mean.size(), 0.0);
                for (std::size_t index = 0; index < problem.mean.size();
                     ++index) {
                    gradient[index] = -problem.mean[index];
                }
                return gradient;
            };
            break;
    }
    return objective;
}

}  // namespace

portfolio_optimization_result optimize_portfolio(
    const dataframe& returns, const portfolio_optimization_config& config) {
    if (returns.get_index().empty()) {
        throw input_error{"returns must be a non-empty DataFrame"};
    }
    if (returns.get_index().size() < 2U) {
        throw input_error{
            "At least 2 observations are required for optimization"};
    }
    if (!std::isfinite(config.max_weight) || config.max_weight <= 0.0) {
        throw input_error{"max_weight must be > 0"};
    }
    if (config.min_weight.has_value() &&
        (!std::isfinite(*config.min_weight) ||
         *config.min_weight > config.max_weight)) {
        throw input_error{"min_weight must be <= max_weight"};
    }
    if (!std::isfinite(config.risk_free_rate)) {
        throw input_error{"risk_free_rate must be finite"};
    }
    if (!config.sector_constraints.empty() &&
        config.sector_map.empty()) {
        throw input_error{
            "sector_map is required when sector_constraints is provided"};
    }
    switch (config.objective) {
        case portfolio_objective::max_sharpe:
        case portfolio_objective::min_variance:
            break;
        case portfolio_objective::target_return:
            if (!config.target_return.has_value() ||
                !std::isfinite(*config.target_return)) {
                throw input_error{
                    "target_return must be specified for "
                    "objective='target_return'"};
            }
            break;
        case portfolio_objective::target_risk:
            if (!config.target_volatility.has_value() ||
                !std::isfinite(*config.target_volatility)) {
                throw input_error{
                    "target_volatility must be specified for "
                    "objective='target_risk'"};
            }
            break;
        default:
            throw input_error{
                "Unknown objective. Choose from 'max_sharpe', "
                "'min_variance', 'target_return', 'target_risk'"};
    }

    // This entry point annualizes at a fixed 252 periods; frontier_config
    // exposes the same factor as a parameter, so returns series on another
    // cadence reach the two paths with different scales by design.
    const auto problem = detail::load_portfolio_problem(returns, 252.0);
    const auto constraints =
        build_constraints(config, problem.asset_names);
    if (problem.mean.size() * constraints.upper < 1.0 - 1e-12 ||
        problem.mean.size() * constraints.lower > 1.0 + 1e-12) {
        throw optimization_error{
            "weight bounds are infeasible for a fully invested portfolio"};
    }

    if (config.objective == portfolio_objective::target_risk) {
        // Both probes drop the volatility ceiling that is being tested: leaving
        // it applied would cap the highest-volatility portfolio at the target
        // itself, so the reachability range could never contradict the bound it
        // is meant to establish.
        solver_constraints equality = constraints;
        equality.has_target_volatility = false;
        const auto minimum = detail::minimize_projected(
            problem, equality,
            make_objective(problem,
                           {portfolio_objective::min_variance}),
            std::vector<double>(problem.mean.size(),
                                1.0 / static_cast<double>(
                                          problem.mean.size())));
        const double minimum_volatility =
            detail::portfolio_volatility(problem, minimum);
        solver_constraints maximization = constraints;
        maximization.has_target_volatility = false;
        const auto maximum =
            detail::minimize_projected(
                problem, maximization,
                make_objective(problem,
                               {portfolio_objective::target_risk}),
                std::vector<double>(problem.mean.size(),
                                    1.0 / static_cast<double>(
                                              problem.mean.size())));
        const double maximum_volatility =
            detail::portfolio_volatility(problem, maximum);
        const double target = *config.target_volatility;
        if (target < minimum_volatility - 1e-8 ||
            target > maximum_volatility + 1e-8) {
            throw optimization_error{
                "target_volatility is infeasible for the bounded portfolio"};
        }
    }

    std::vector<double> start(problem.mean.size(),
                              1.0 / static_cast<double>(
                                        problem.mean.size()));
    std::vector<double> weights;
    // A sector-free minimum-variance book is a quadratic over the unit simplex
    // and uses the dedicated solver for that set; a single sector row breaks
    // the shape and routes the same objective through the general solver. The
    // trade-off is recorded as F10-D10: weights on this path cannot go below
    // zero, so a lowered bound only reaches the general branch.
    if (config.objective == portfolio_objective::min_variance &&
        constraints.sectors.empty()) {
        std::vector<double> hessian(problem.covariance.size(), 0.0);
        for (std::size_t index = 0; index < hessian.size(); ++index) {
            hessian[index] = 2.0 * problem.covariance[index];
        }
        const std::vector<double> linear(problem.mean.size(), 0.0);
        detail::projected_gradient_options options;
        options.gradient_tolerance = 1e-12;
        options.step_tolerance = 1e-16;
        options.max_iterations = 50000U;
        weights = detail::minimize_quadratic_on_simplex(
                      hessian, linear, start, options)
                      .parameters;
    } else {
        weights = detail::minimize_projected(
            problem, constraints, make_objective(problem, config), start);
    }
    detail::validate_weights(problem, constraints, weights,
                             "optimize");

    portfolio_optimization_result result;
    result.weights = std::move(weights);
    result.expected_return = detail::portfolio_return(problem, result.weights);
    result.volatility =
        detail::portfolio_volatility(problem, result.weights);
    result.sharpe =
        result.volatility > 1e-12
            ? (result.expected_return - config.risk_free_rate) /
                  result.volatility
            : 0.0;
    result.asset_names = problem.asset_names;
    result.objective = config.objective;
    return result;
}

}  // namespace fincore
