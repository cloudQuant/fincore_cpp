#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore::detail {

struct portfolio_problem {
    std::vector<double> mean;
    std::vector<double> covariance;
    std::vector<std::string> asset_names;
    std::size_t observations = 0U;
};

portfolio_problem load_portfolio_problem(const dataframe& returns,
                                         double periods_per_year);

double portfolio_return(const portfolio_problem& problem,
                        const std::vector<double>& weights);

double portfolio_variance(const portfolio_problem& problem,
                          const std::vector<double>& weights);

double portfolio_volatility(const portfolio_problem& problem,
                            const std::vector<double>& weights);

std::vector<double> covariance_product(
    const portfolio_problem& problem, const std::vector<double>& weights);

struct sector_bounds {
    std::vector<std::size_t> indices;
    double lower;
    double upper;
};

struct solver_constraints {
    double lower = 0.0;
    double upper = 1.0;
    bool has_target_return = false;
    double target_return = 0.0;
    bool has_target_volatility = false;
    double target_volatility = 0.0;
    std::vector<sector_bounds> sectors;
};

// The search treats value and gradient as one function described two ways: no
// finite-difference check is performed, so a gradient that disagrees with its own
// value steers the projection without ever being detected.
struct solver_objective {
    std::function<double(const std::vector<double>&)> value;
    std::function<std::vector<double>(const std::vector<double>&)> gradient;
};

// The returned weights are feasible only as far as the projection could enforce
// within its inner budget, which is why every caller follows this with
// validate_weights instead of treating a completed search as a satisfied
// constraint set.
std::vector<double> minimize_projected(const portfolio_problem& problem,
                                       const solver_constraints& constraints,
                                       const solver_objective& objective,
                                       std::vector<double> start,
                                       std::size_t inner_iterations = 5000U);

void validate_weights(const portfolio_problem& problem,
                      const solver_constraints& constraints,
                      const std::vector<double>& weights,
                      std::string_view context);

}  // namespace fincore::detail
