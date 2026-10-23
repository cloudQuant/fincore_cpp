#include "portfolio_solver.hpp"

#include <fincore/detail/optimization.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <typeinfo>

namespace fincore::detail {
namespace {

double dot(const std::vector<double>& left,
           const std::vector<double>& right) {
    double total = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        total += left[index] * right[index];
    }
    return total;
}

double penalty_value(const portfolio_problem& problem,
                     const solver_constraints& constraints,
                     const std::vector<double>& weights) {
    double total = 0.0;
    if (constraints.has_target_return) {
        const double residual =
            portfolio_return(problem, weights) - constraints.target_return;
        total += residual * residual;
    }
    if (constraints.has_target_volatility) {
        const double excess =
            portfolio_volatility(problem, weights) -
            constraints.target_volatility;
        if (excess > 0.0) {
            total += excess * excess;
        }
    }
    for (const auto& sector : constraints.sectors) {
        double allocation = 0.0;
        for (const std::size_t index : sector.indices) {
            allocation += weights[index];
        }
        const double below = std::max(0.0, sector.lower - allocation);
        const double above = std::max(0.0, allocation - sector.upper);
        total += below * below + above * above;
    }
    return total;
}

std::vector<double> penalty_gradient(
    const portfolio_problem& problem,
    const solver_constraints& constraints,
    const std::vector<double>& weights) {
    const std::size_t count = weights.size();
    std::vector<double> gradient(count, 0.0);
    if (constraints.has_target_return) {
        const double residual =
            portfolio_return(problem, weights) - constraints.target_return;
        for (std::size_t index = 0; index < count; ++index) {
            gradient[index] += 2.0 * residual * problem.mean[index];
        }
    }
    if (constraints.has_target_volatility) {
        const double volatility = portfolio_volatility(problem, weights);
        const double excess = volatility - constraints.target_volatility;
        if (excess > 0.0 && volatility > 1e-15) {
            const auto marginal = covariance_product(problem, weights);
            for (std::size_t index = 0; index < count; ++index) {
                gradient[index] +=
                    2.0 * excess * marginal[index] / volatility;
            }
        }
    }
    for (const auto& sector : constraints.sectors) {
        double allocation = 0.0;
        for (const std::size_t index : sector.indices) {
            allocation += weights[index];
        }
        if (allocation < sector.lower) {
            for (const std::size_t index : sector.indices) {
                gradient[index] -= 2.0 * (sector.lower - allocation);
            }
        } else if (allocation > sector.upper) {
            for (const std::size_t index : sector.indices) {
                gradient[index] += 2.0 * (allocation - sector.upper);
            }
        }
    }
    return gradient;
}

}  // namespace

portfolio_problem load_portfolio_problem(const dataframe& returns,
                                         const double periods_per_year) {
    const auto& index = returns.get_index();
    if (index.size() < 2U) {
        throw input_error{
            "returns must contain at least 2 observations"};
    }
    const auto columns = returns.get_columns_info<double>();
    std::vector<std::string> names;
    std::vector<std::vector<double>> series;
    for (const auto& info : columns) {
        if (std::get<2>(info) != typeid(double)) {
            continue;
        }
        const std::string name{std::get<0>(info).c_str()};
        if (name.empty()) {
            throw input_error{"return column names must not be empty"};
        }
        const auto& values = returns.get_column<double>(name.c_str());
        if (values.size() != index.size()) {
            throw input_error{
                "return columns must match the frame index length"};
        }
        for (const double value : values) {
            if (!std::isfinite(value)) {
                throw input_error{
                    "returns contains NaN or infinite values"};
            }
        }
        names.push_back(name);
        series.emplace_back(values.begin(), values.end());
    }
    if (names.empty()) {
        throw input_error{"returns must contain at least one double column"};
    }

    const std::size_t observations = index.size();
    const std::size_t assets = names.size();
    portfolio_problem problem;
    problem.asset_names = std::move(names);
    problem.observations = observations;
    problem.mean.assign(assets, 0.0);
    problem.covariance.assign(assets * assets, 0.0);
    std::vector<double> raw_mean(assets, 0.0);
    for (std::size_t asset = 0; asset < assets; ++asset) {
        double total = 0.0;
        for (const double value : series[asset]) {
            total += value;
        }
        raw_mean[asset] = total / static_cast<double>(observations);
        problem.mean[asset] = raw_mean[asset] * periods_per_year;
    }
    for (std::size_t row = 0; row < assets; ++row) {
        for (std::size_t column = 0; column < assets; ++column) {
            double total = 0.0;
            for (std::size_t observation = 0; observation < observations;
                 ++observation) {
                const double left =
                    series[row][observation] - raw_mean[row];
                const double right =
                    series[column][observation] - raw_mean[column];
                total += left * right;
            }
            problem.covariance[row * assets + column] =
                total / static_cast<double>(observations - 1U) *
                periods_per_year;
        }
    }
    return problem;
}

double portfolio_return(const portfolio_problem& problem,
                        const std::vector<double>& weights) {
    return dot(problem.mean, weights);
}

double portfolio_variance(const portfolio_problem& problem,
                          const std::vector<double>& weights) {
    return dot(weights, covariance_product(problem, weights));
}

double portfolio_volatility(const portfolio_problem& problem,
                            const std::vector<double>& weights) {
    const double variance = portfolio_variance(problem, weights);
    return variance > 0.0 ? std::sqrt(variance) : 0.0;
}

std::vector<double> covariance_product(
    const portfolio_problem& problem, const std::vector<double>& weights) {
    const std::size_t assets = weights.size();
    std::vector<double> product(assets, 0.0);
    for (std::size_t row = 0; row < assets; ++row) {
        for (std::size_t column = 0; column < assets; ++column) {
            product[row] +=
                problem.covariance[row * assets + column] * weights[column];
        }
    }
    return product;
}

std::vector<double> minimize_projected(
    const portfolio_problem& problem, const solver_constraints& constraints,
    const solver_objective& objective, std::vector<double> start,
    const std::size_t inner_iterations) {
    const std::size_t assets = start.size();
    const auto project = [&](const std::vector<double>& values) {
        return project_onto_simplex(values, constraints.lower,
                                    constraints.upper, 1.0);
    };
    std::vector<double> weights = project(start);
    double penalty_weight = 1e2;
    for (int outer = 0; outer < 10; ++outer) {
        double value = objective.value(weights) +
                       penalty_weight * penalty_value(problem, constraints,
                                                      weights);
        double step = 1.0;
        for (std::size_t iteration = 0; iteration < inner_iterations;
             ++iteration) {
            std::vector<double> gradient = objective.gradient(weights);
            const auto penalty = penalty_gradient(problem, constraints,
                                                  weights);
            for (std::size_t index = 0; index < assets; ++index) {
                gradient[index] += penalty_weight * penalty[index];
            }
            double squared_norm = 0.0;
            for (const double component : gradient) {
                squared_norm += component * component;
            }
            if (!std::isfinite(squared_norm) || squared_norm < 1e-28) {
                break;
            }
            const double gradient_norm = std::sqrt(squared_norm);
            const double scale = std::max(1.0, gradient_norm);
            const double slope =
                -squared_norm / scale;
            std::vector<double> candidate;
            double candidate_value = value;
            bool accepted = false;
            for (int backtrack = 0; backtrack < 60; ++backtrack) {
                std::vector<double> trial(assets, 0.0);
                for (std::size_t index = 0; index < assets; ++index) {
                    trial[index] =
                        weights[index] - step * gradient[index] / scale;
                }
                candidate = project(trial);
                candidate_value =
                    objective.value(candidate) +
                    penalty_weight * penalty_value(problem, constraints,
                                                   candidate);
                if (std::isfinite(candidate_value) &&
                    candidate_value <= value + 1e-4 * step * slope) {
                    accepted = true;
                    break;
                }
                step *= 0.5;
                if (step < 1e-16) {
                    break;
                }
            }
            if (!accepted) {
                break;
            }
            double delta = 0.0;
            for (std::size_t index = 0; index < assets; ++index) {
                const double difference = candidate[index] - weights[index];
                delta += difference * difference;
            }
            weights = std::move(candidate);
            value = candidate_value;
            step = std::min(step * 1.5, 1.0);
            if (std::sqrt(delta) < 1e-13) {
                break;
            }
        }
        if (penalty_value(problem, constraints, weights) <= 1e-18) {
            break;
        }
        penalty_weight *= 10.0;
    }
    return weights;
}

void validate_weights(const portfolio_problem& problem,
                      const solver_constraints& constraints,
                      const std::vector<double>& weights,
                      const std::string_view context) {
    const double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    double lower_residual = 0.0;
    double upper_residual = 0.0;
    for (const double weight : weights) {
        lower_residual =
            std::max(lower_residual, constraints.lower - weight);
        upper_residual =
            std::max(upper_residual, weight - constraints.upper);
    }
    const double sum_residual = std::abs(sum - 1.0);
    double target_residual = 0.0;
    if (constraints.has_target_return) {
        target_residual =
            std::abs(portfolio_return(problem, weights) -
                     constraints.target_return);
    }
    double sector_residual = 0.0;
    for (const auto& sector : constraints.sectors) {
        double allocation = 0.0;
        for (const std::size_t index : sector.indices) {
            allocation += weights[index];
        }
        sector_residual =
            std::max(sector_residual, sector.lower - allocation);
        sector_residual =
            std::max(sector_residual, allocation - sector.upper);
    }
    double volatility_residual = 0.0;
    if (constraints.has_target_volatility) {
        volatility_residual =
            std::abs(portfolio_volatility(problem, weights) -
                     constraints.target_volatility);
    }
    const double target_tolerance =
        1e-8 * std::max(1.0, std::abs(constraints.target_return));
    if (sum_residual > 1e-8 || lower_residual > 1e-8 ||
        upper_residual > 1e-8 || target_residual > target_tolerance ||
        sector_residual > 1e-8 || volatility_residual > 1e-6) {
        throw optimization_error{
            std::string(context) +
            " returned infeasible weights: sum=" +
            std::to_string(sum_residual) + ", lower=" +
            std::to_string(lower_residual) + ", upper=" +
            std::to_string(upper_residual) + ", target=" +
            std::to_string(target_residual) + ", sector=" +
            std::to_string(sector_residual) + ", volatility=" +
            std::to_string(volatility_residual)};
    }
}

}  // namespace fincore::detail
