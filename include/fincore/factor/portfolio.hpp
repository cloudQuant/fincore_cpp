#pragma once

#include <fincore/factor/data.hpp>
#include <fincore/factor/performance.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Daily portfolio inputs built from one factor period.
struct factor_portfolio_inputs {
    /// Daily simple returns generated from the compounded period curve.
    factor_series_result returns;
    /// Daily gross-normalized asset weights plus one explicit ``cash`` row
    /// per timestamp.  Cash makes every row's signed total exactly one.
    std::vector<factor_weight_row> positions;
    /// Daily long-only equal-weight benchmark returns when the requested
    /// period labels a forward-return column; absent otherwise.
    std::optional<factor_series_result> benchmark_returns;
};

/// Builds overlapping gross-normalized trade positions for one factor period.
///
/// ``period_label`` counts whole business days (for example ``"3D"`` or
/// ``"1D2D"``); a holding expires on the corresponding Monday-Friday weekday
/// calendar without exchange-holiday knowledge, matching pandas' default
/// ``BusinessDay`` offset.  The result spans every trade and expiry
/// timestamp, including a zero row after the final holding expires.
///
/// @param factor_data Prepared factor panel with forward-return labels.
/// @param period_label Forward-return label whose day count sets the holding.
/// @param long_short Enables long-short weighting when true.
/// @param group_neutral Applies group-neutral weighting when true.
/// @param equal_weight Uses equal weights within each bucket when true.
/// @param quantiles Optional quantile filter; empty keeps all quantiles.
/// @param groups Optional group filter; empty keeps all groups.
/// @param weights Optional precomputed weights replacing derived weights.
/// @return Gross-normalized position rows in date/asset order.
/// @throws factor_data_error When the period label is absent or malformed, the
///         selection is empty, or grouped weights lack group labels.
std::vector<factor_weight_row> factor_positions(
    const prepared_factor_data& factor_data, std::string_view period_label,
    bool long_short = true, bool group_neutral = false,
    bool equal_weight = false, const std::vector<int>& quantiles = {},
    const std::vector<std::string>& groups = {},
    std::optional<std::vector<factor_weight_row>> weights = std::nullopt);

/// Compounds the selected period returns from one.
///
/// @param factor_data Prepared factor panel with forward-return labels.
/// @param period_label Forward-return label to compound.
/// @param long_short Enables long-short weighting when true.
/// @param group_neutral Applies group-neutral weighting when true.
/// @param equal_weight Uses equal weights within each bucket when true.
/// @param quantiles Optional quantile filter; empty keeps all quantiles.
/// @param groups Optional group filter; empty keeps all groups.
/// @return Compounded cumulative return series indexed by date.
/// @throws factor_data_error When the period label is absent, the selection is
///         empty, or grouped weights lack group labels.
factor_series_result factor_cumulative_returns(
    const prepared_factor_data& factor_data, std::string_view period_label,
    bool long_short = true, bool group_neutral = false,
    bool equal_weight = false, const std::vector<int>& quantiles = {},
    const std::vector<std::string>& groups = {});

/// Resamples the compounded period curve and overlapping positions to a daily
/// grid, adds an explicit cash overlay, and derives the optional benchmark.
///
/// Intraday holding remainders, capital-scaled dollar positions, and custom
/// trading calendars are not represented by this kernel.
///
/// @param factor_data Prepared factor panel with forward-return labels.
/// @param period_label Forward-return label whose day count sets the holding.
/// @param long_short Enables long-short weighting when true.
/// @param group_neutral Applies group-neutral weighting when true.
/// @param equal_weight Uses equal weights within each bucket when true.
/// @param quantiles Optional quantile filter; empty keeps all quantiles.
/// @param groups Optional group filter; empty keeps all groups.
/// @return Daily strategy returns, cash-overlaid positions, and benchmark
///         returns for the period.
/// @throws factor_data_error When the period label is absent, the selection is
///         empty, or grouped weights lack group labels.
factor_portfolio_inputs build_factor_portfolio_inputs(
    const prepared_factor_data& factor_data, std::string_view period_label,
    bool long_short = true, bool group_neutral = false,
    bool equal_weight = false, const std::vector<int>& quantiles = {},
    const std::vector<std::string>& groups = {});

}  // namespace fincore
