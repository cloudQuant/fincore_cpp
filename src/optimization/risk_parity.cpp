#include <fincore/optimization/risk_parity.hpp>

#include "portfolio_solver.hpp"

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

}  // namespace

risk_parity_result risk_parity(
    const dataframe& returns,
    std::optional<std::vector<double>> risk_budget,
    const std::size_t max_iter) {
    if (returns.get_index().empty()) {
        throw input_error{"returns must be a non-empty DataFrame"};
    }
    if (returns.get_index().size() < 2U) {
        throw input_error{
            "At least 2 observations are required for risk parity"};
    }
    if (max_iter < 1U) {
        throw input_error{"max_iter must be >= 1"};
    }

    const auto problem = detail::load_portfolio_problem(returns, 252.0);
    const std::size_t assets = problem.mean.size();
    std::vector<double> budget(assets, 1.0 / static_cast<double>(assets));
    if (risk_budget.has_value()) {
        if (risk_budget->size() != assets) {
            throw input_error{
                "risk_budget length must match number of assets"};
        }
        budget = *risk_budget;
        for (const double value : budget) {
            if (!std::isfinite(value)) {
                throw input_error{
                    "risk_budget contains NaN or infinite values"};
            }
            if (value < 0.0) {
                throw input_error{"risk_budget must be non-negative"};
            }
        }
        double total = 0.0;
        for (const double value : budget) {
            total += value;
        }
        if (total <= 0.0) {
            throw input_error{"risk_budget must have a positive sum"};
        }
        for (double& value : budget) {
            value /= total;
        }
    }

    solver_constraints constraints;
    constraints.lower = 1e-6;
    constraints.upper = 1.0;
    std::function<double(const std::vector<double>&)> risk_contribution_error =
        [&problem, &budget](const std::vector<double>& weights) {
            const double variance =
                detail::portfolio_variance(problem, weights);
            if (variance < 1e-16) {
                return 1e6;
            }
            const double volatility = std::sqrt(variance);
            const auto marginal =
                detail::covariance_product(problem, weights);
            double total = 0.0;
            std::vector<double> contributions(weights.size(), 0.0);
            for (std::size_t index = 0; index < weights.size(); ++index) {
                contributions[index] =
                    weights[index] * marginal[index] / volatility;
                total += contributions[index];
            }
            if (total < 1e-16) {
                return 1e6;
            }
            double error = 0.0;
            for (std::size_t index = 0; index < weights.size(); ++index) {
                const double difference =
                    contributions[index] / total - budget[index];
                error += difference * difference;
            }
            return error;
        };
    solver_objective objective;
    objective.value = risk_contribution_error;
    objective.gradient = [risk_contribution_error](
                             const std::vector<double>& weights) {
        constexpr double step = 1e-6;
        std::vector<double> gradient(weights.size(), 0.0);
        for (std::size_t index = 0; index < weights.size(); ++index) {
            auto shifted = weights;
            shifted[index] += step;
            const double upper = risk_contribution_error(shifted);
            shifted[index] -= 2.0 * step;
            const double lower = risk_contribution_error(shifted);
            gradient[index] = (upper - lower) / (2.0 * step);
        }
        return gradient;
    };

    const std::vector<double> start(assets,
                                    1.0 / static_cast<double>(assets));
    auto weights = detail::minimize_projected(
        problem, constraints, objective, start, max_iter);
    detail::validate_weights(problem, constraints, weights, "risk_parity");
    const double total =
        std::accumulate(weights.begin(), weights.end(), 0.0);
    if (std::abs(total) < 1e-12 || total < 0.0) {
        throw optimization_error{
            "Cannot normalize risk-parity weights: sum is too close to zero"};
    }
    for (double& weight : weights) {
        weight /= total;
    }

    risk_parity_result result;
    result.asset_names = problem.asset_names;
    result.volatility = detail::portfolio_volatility(problem, weights);
    result.weights = weights;
    result.risk_contributions.assign(assets, 0.0);
    if (result.volatility > 1e-15) {
        const auto marginal = detail::covariance_product(problem, weights);
        for (std::size_t index = 0; index < assets; ++index) {
            result.risk_contributions[index] =
                weights[index] * marginal[index] / result.volatility;
        }
    }
    return result;
}

}  // namespace fincore
