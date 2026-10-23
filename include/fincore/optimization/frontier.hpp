#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace fincore {

enum class frontier_solver {
    min_variance_degenerate,
    min_variance,
    projected_gradient,
    bounded_linear_max_return
};

struct portfolio_point {
    std::vector<double> weights;
    double expected_return = 0.0;
    double volatility = 0.0;
};

struct frontier_point_diagnostics {
    frontier_solver solver = frontier_solver::projected_gradient;
    double sum_residual = 0.0;
    double lower_bound_residual = 0.0;
    double upper_bound_residual = 0.0;
    double target_return_residual = 0.0;
};

struct frontier_config {
    std::size_t n_points = 50U;
    double risk_free_rate = 0.0;
    bool short_allowed = false;
    double max_weight = 1.0;
    double periods_per_year = 252.0;
};

struct efficient_frontier_result {
    std::vector<double> frontier_returns;
    std::vector<double> frontier_volatilities;
    std::vector<double> frontier_sharpe;
    std::vector<std::vector<double>> frontier_weights;
    portfolio_point min_variance;
    portfolio_point max_sharpe;
    double max_sharpe_ratio = 0.0;
    std::vector<std::string> asset_names;
    double periods_per_year = 252.0;
    frontier_point_diagnostics min_variance_diagnostics;
    frontier_point_diagnostics max_sharpe_diagnostics;
    std::vector<frontier_point_diagnostics> frontier_diagnostics;
};

efficient_frontier_result efficient_frontier(
    const dataframe& returns, const frontier_config& config = {});

}  // namespace fincore
