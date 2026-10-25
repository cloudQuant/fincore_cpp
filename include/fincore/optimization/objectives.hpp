#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fincore {

/// Portfolio optimization objective.
enum class portfolio_objective {
    max_sharpe,     ///< Maximize the Sharpe ratio using `risk_free_rate`.
    min_variance,   ///< Minimize portfolio variance.
    target_return,  ///< Minimize variance subject to the expected return
                    ///< equaling `target_return`.
    target_risk     ///< Maximize expected return subject to the volatility
                    ///< equaling `target_volatility`.
};

/// Bounds on the summed weights of the assets mapped to one sector.
struct sector_allocation_constraint {
    /// Sector name matched against `portfolio_optimization_config::sector_map`
    /// values.
    std::string sector;
    double lower;  ///< Lower bound on the summed sector weight.
    double upper;  ///< Upper bound on the summed sector weight.
};

/// Inputs for optimize_portfolio.
struct portfolio_optimization_config {
    /// Objective to optimize.
    portfolio_objective objective = portfolio_objective::max_sharpe;
    /// Annualized risk-free rate used by `max_sharpe` and the reported Sharpe
    /// ratio; must be finite.
    double risk_free_rate = 0.0;
    /// Annualized expected return required by `target_return`; must be finite
    /// when used.
    std::optional<double> target_return;
    /// Annualized volatility required by `target_risk`; must be finite and
    /// attainable.
    std::optional<double> target_volatility;
    /// Allow weights down to `-max_weight` when `min_weight` is unset.
    bool short_allowed = false;
    /// Per-asset upper bound; must be finite and greater than 0.
    double max_weight = 1.0;
    /// Optional per-asset lower bound; must not exceed `max_weight` and
    /// overrides the `short_allowed` default.
    std::optional<double> min_weight;
    /// Sector weight bounds; requires `sector_map`, and sectors without a
    /// mapped asset are ignored.
    std::vector<sector_allocation_constraint> sector_constraints;
    /// Asset name to sector name mapping used by `sector_constraints`.
    std::map<std::string, std::string> sector_map;
};

/// Optimized weights and their annualized summary statistics.
struct portfolio_optimization_result {
    /// Asset weights in `asset_names` order; they sum to 1.
    std::vector<double> weights;
    /// Annualized expected return, using 252 periods per year.
    double expected_return = 0.0;
    /// Annualized volatility, using 252 periods per year.
    double volatility = 0.0;
    /// `(expected_return - risk_free_rate) / volatility`; 0 where volatility
    /// is negligible.
    double sharpe = 0.0;
    /// Asset names, in returns column order.
    std::vector<std::string> asset_names;
    /// Objective that produced the result.
    portfolio_objective objective = portfolio_objective::max_sharpe;
};

/// Optimizes portfolio weights for one supported objective.
///
/// Returns are annualized with a fixed 252-period year. Sector constraints
/// apply only to assets listed in `sector_map`, and infeasible targets are
/// reported by throwing rather than through a status field.
///
/// @param returns Daily simple returns with one finite double column per asset
///     and at least two observations.
/// @param config Objective, bounds and optional sector constraints.
/// @return Optimized weights, annualized return, volatility, Sharpe ratio and
///     asset names.
/// @throws input_error If `returns` is empty, too short or non-finite, an
///     objective-specific target is missing, weight bounds are invalid, sector
///     constraints lack a sector map, or the objective is unknown.
/// @throws optimization_error If the weight bounds cannot produce a fully
///     invested portfolio, `target_volatility` lies outside the attainable
///     range, or the solver returns infeasible weights.
portfolio_optimization_result optimize_portfolio(
    const dataframe& returns,
    const portfolio_optimization_config& config = {});

}  // namespace fincore
