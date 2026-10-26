#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace fincore {

/// Solver that produced one frontier point.
enum class frontier_solver {
    min_variance_degenerate,   ///< Minimum-variance weights reused for every
                               ///< point because the attainable return range
                               ///< is degenerate.
    min_variance,              ///< First point, solved as a variance
                               ///< minimization.
    projected_gradient,        ///< Interior target-return point or the
                               ///< max-Sharpe portfolio, solved by projected
                               ///< gradient descent.
    bounded_linear_max_return  ///< Last point, filled greedily by descending
                               ///< expected return up to the weight bounds.
};

/// One portfolio on the efficient frontier.
struct portfolio_point {
    /// Asset weights in `asset_names` order; they sum to 1.
    std::vector<double> weights;
    double expected_return = 0.0;  ///< Annualized expected return.
    double volatility = 0.0;       ///< Annualized volatility.
};

/// Constraint residuals measured for one frontier point.
struct frontier_point_diagnostics {
    /// Solver used for the point.
    frontier_solver solver = frontier_solver::projected_gradient;
    /// Absolute deviation of the weight sum from 1.
    double sum_residual = 0.0;
    /// Largest lower-bound violation; 0 when feasible.
    double lower_bound_residual = 0.0;
    /// Largest upper-bound violation; 0 when feasible.
    double upper_bound_residual = 0.0;
    /// Absolute target-return deviation; 0 when no target was imposed.
    double target_return_residual = 0.0;
};

/// Efficient-frontier computation settings.
struct frontier_config {
    /// Number of frontier points; must be at least 2.
    std::size_t n_points = 50U;
    /// Annualized risk-free rate used for Sharpe ratios; must be finite.
    double risk_free_rate = 0.0;
    /// Allow weights down to `-max_weight` instead of 0.
    bool short_allowed = false;
    /// Per-asset upper bound; must be finite and greater than 0.
    double max_weight = 1.0;
    /// Annualization factor; must be positive and finite.
    double periods_per_year = 252.0;
};

/// Efficient-frontier points and endpoint portfolios.
struct efficient_frontier_result {
    /// Annualized expected return of each point, from the minimum-variance
    /// return to the maximum-return endpoint.
    std::vector<double> frontier_returns;
    /// Annualized volatility of each point.
    std::vector<double> frontier_volatilities;
    /// Sharpe ratio of each point; 0 where volatility is negligible.
    std::vector<double> frontier_sharpe;
    /// Asset weights of each point, in `asset_names` order.
    std::vector<std::vector<double>> frontier_weights;
    portfolio_point min_variance;  ///< Minimum-variance endpoint portfolio.
    portfolio_point max_sharpe;    ///< Maximum-Sharpe portfolio.
    /// Sharpe ratio of `max_sharpe`; 0 where volatility is negligible.
    double max_sharpe_ratio = 0.0;
    /// Asset names, in returns column order.
    std::vector<std::string> asset_names;
    /// Annualization factor copied from the configuration.
    double periods_per_year = 252.0;
    /// Residuals of the minimum-variance endpoint.
    frontier_point_diagnostics min_variance_diagnostics;
    /// Residuals of the max-Sharpe portfolio.
    frontier_point_diagnostics max_sharpe_diagnostics;
    /// Per-point residuals, parallel to `frontier_returns`.
    std::vector<frontier_point_diagnostics> frontier_diagnostics;
};

/// Computes an efficient frontier from daily simple returns.
///
/// Points are evenly spaced in expected return between the minimum-variance
/// portfolio and a maximum-return endpoint built by filling assets in
/// descending mean-return order up to the weight bounds. Returns and
/// volatilities are annualized with `config.periods_per_year`, and Sharpe
/// ratios use `config.risk_free_rate`. Solvers report constraint residuals in
/// `frontier_diagnostics` rather than a convergence status.
///
/// @param returns Daily simple returns with one finite double column per asset
///     and at least two observations.
/// @param config Frontier settings; `n_points` must be at least 2.
/// @return Frontier points, endpoint portfolios and per-point diagnostics.
/// @throws input_error If `returns` is empty or too short, fewer than two
///     assets are present, values are non-finite, or a configuration value is
///     invalid.
/// @throws optimization_error If the covariance matrix is not positive
///     semidefinite, the weight bounds cannot fund a fully invested portfolio,
///     or a solver returns infeasible weights.
efficient_frontier_result efficient_frontier(
    const dataframe& returns, const frontier_config& config = {});

}  // namespace fincore
