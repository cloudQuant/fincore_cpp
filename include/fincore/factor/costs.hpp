#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/factor/performance.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace fincore {

/// Spread, market-impact and participation settings for trading costs.
struct factor_cost_model {
    double half_spread_bps = 0.0; ///< Half-spread in basis points per traded weight.
    double impact_coefficient = 0.0; ///< Scale of the impact term.
    double max_participation = 1.0; ///< Participation cap in (0, 1].
    double impact_exponent = 0.5; ///< Participation exponent; must be positive.
};

/// Capacity estimate for a weight panel against dollar volume.
struct factor_capacity_result {
    double maximum_portfolio_value = 0.0; ///< Smallest finite per-date capacity.
    std::vector<timestamp> dates; ///< Panel dates in ascending order.
    /// Capacity per date; infinity when the date has no trade.
    std::vector<double> maximum_portfolio_value_by_date;
    std::vector<std::string> assets; ///< Panel assets in ascending order.
    std::vector<double> trade_weights; ///< Dates x assets absolute traded weights.
    double max_participation = 1.0; ///< Participation cap used by the estimate.
    timestamp binding_date = 0; ///< First date attaining the smallest capacity.
    std::string binding_asset; ///< Asset limiting capacity on the binding date.
};

/// Per-date cost ledger produced by applying a cost model to gross returns.
struct factor_cost_result {
    factor_cost_model model; ///< Cost model used for the ledger.
    double portfolio_value = 0.0; ///< Portfolio value the ledger was computed for.
    std::vector<timestamp> dates; ///< Panel dates in ascending order.
    std::vector<std::string> assets; ///< Panel assets in ascending order.
    std::vector<double> gross_returns; ///< Gross return per date, copied from the input.
    std::vector<double> net_returns; ///< Gross return less total cost per date.
    std::vector<double> turnover; ///< One-way turnover per date.
    std::vector<double> spread_cost; ///< Spread cost per date as a return fraction.
    std::vector<double> impact_cost; ///< Market-impact cost per date as a return fraction.
    std::vector<double> borrow_cost; ///< Borrow cost per date as a return fraction.
    std::vector<double> total_cost; ///< Sum of spread, impact and borrow costs per date.
    std::vector<double> trade_weights; ///< Dates x assets absolute traded weights.
    std::vector<double> participation; ///< Dates x assets traded value over dollar volume.
    factor_capacity_result capacity; ///< Capacity estimate at the model participation cap.
};

/// Estimates the largest portfolio value a weight panel can trade.
///
/// Each date is limited by the tightest asset-level
/// `max_participation * dollar_volume / |trade|`; the panel capacity is the
/// smallest finite date limit. The first date that attains the limit and the
/// asset limiting it are reported as the binding pair.
///
/// @param weights Gross-normalized weights, one row per (date, asset).
/// @param dollar_volume Tradable dollar volume on a sorted, unique index
///        spanning every weight date and asset, strictly positive.
/// @param max_participation Fraction of dollar volume allowed per asset,
///        in (0, 1].
/// @return Capacity summary carrying the trade panel and binding pair.
/// @throws factor_data_error When the weights are duplicated, non-finite or
///         not gross-normalized, when dollar volume is missing, unsorted,
///         non-finite or non-positive, or when no trade occurs.
factor_capacity_result estimate_factor_capacity(
    const std::vector<factor_weight_row>& weights,
    const dataframe& dollar_volume, double max_participation);

/// Applies spread, market-impact and borrow costs to gross returns.
///
/// Turnover is half the summed absolute weight change on each date. Spread
/// cost charges `half_spread_bps` on traded weight, impact cost charges
/// `impact_coefficient * participation^impact_exponent` per asset, and borrow
/// cost accrues on short positions from the supplied borrow panels.
///
/// @param gross_returns One gross return per weight date.
/// @param weights Gross-normalized weights, one row per (date, asset).
/// @param dollar_volume Tradable dollar volume on a sorted, unique index
///        spanning every weight date and asset, strictly positive.
/// @param portfolio_value Positive portfolio value, no larger than the
///        estimated capacity.
/// @param model Cost parameters applied to every date.
/// @param borrow_rates Short-rate panel on the weight grid; required when any
///        weight is negative and rejected otherwise.
/// @param borrow_available Availability panel on the weight grid, where 1.0
///        marks a lendable short; required alongside `borrow_rates`.
/// @return Cost ledger with gross, net, turnover, cost components, trades,
///         participation and capacity.
/// @throws factor_data_error When inputs are malformed, a short lacks borrow
///         data or availability, the portfolio exceeds capacity, or a cost or
///         participation value is non-finite.
factor_cost_result apply_factor_costs(
    std::span<const double> gross_returns,
    const std::vector<factor_weight_row>& weights,
    const dataframe& dollar_volume, double portfolio_value,
    const factor_cost_model& model,
    const dataframe* borrow_rates = nullptr,
    const dataframe* borrow_available = nullptr);

}  // namespace fincore
