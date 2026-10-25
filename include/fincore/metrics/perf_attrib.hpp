#pragma once

#include <fincore/portfolio/positions.hpp>
#include <fincore/metrics/returns.hpp>

#include <map>
#include <span>
#include <string>
#include <string_view>

namespace fincore {

/// Default factor-loading column holding asset symbols.
inline constexpr std::string_view attribution_symbol_column{"symbol"};
/// Attribution column holding the total (input) returns.
inline constexpr std::string_view attribution_total_returns_column{
    "total_returns"};
/// Attribution column holding the sum of finite factor contributions.
inline constexpr std::string_view attribution_common_returns_column{
    "common_returns"};
/// Attribution column holding the residual return (`total - common`).
inline constexpr std::string_view attribution_specific_returns_column{
    "specific_returns"};
/// Attribution column holding the sum of mean-exposure tilt contributions.
inline constexpr std::string_view attribution_tilt_returns_column{
    "tilt_returns"};
/// Attribution column holding the timing return (`common - tilt`).
inline constexpr std::string_view attribution_timing_returns_column{
    "timing_returns"};
/// Column holding factor names in the factor summary frame.
inline constexpr std::string_view attribution_factor_column{"factor"};

/// Per-date risk exposures and return attribution.
struct perf_attribution_result {
  dataframe risk_exposures; ///< Factor exposures on the attribution index.
  dataframe attribution;    ///< Factor contributions plus the total, common,
                            ///< specific, tilt and timing return columns.
};

/// Summary statistics of a perf_attribution_result.
struct perf_attribution_statistics {
  std::map<std::string, double> summary; ///< Seven named return statistics.
  dataframe factor_summary;              ///< One row per factor: its name,
                                         ///< average exposure, and annualized
                                         ///< and cumulative contribution.
};

/// Returns positions converted to per-timestamp weights when requested.
///
/// Every double column except `cash_column_name` is kept; non-double columns
/// are ignored. With `positions_in_dollars`, each value is divided by that
/// row's gross (the sum of absolute non-NaN positions), so NaN positions stay
/// NaN and an all-zero row yields NaN weights. Otherwise values are copied.
/// The input index and order are preserved. Duplicate timestamps, a non-double
/// cash column, an empty asset name, or no asset columns raise `input_error`.
dataframe normalize_attribution_positions(
    const dataframe &positions, bool positions_in_dollars = true,
    std::string_view cash_column_name = cash_column);

/// Returns per-date factor exposures of `positions`.
///
/// For each requested factor, the exposure sums position value times factor
/// loading over the assets, matched by (timestamp, symbol) against
/// `factor_loadings`; NaN products are skipped. The result keeps the positions
/// index and one column per name in `factor_column_names`, in that order.
/// Duplicate position timestamps, empty, duplicated or symbol-colliding factor
/// names, a missing or non-string symbol column, a non-double factor-loading
/// column, a duplicate (timestamp, symbol) key, or a missing key for a
/// positioned asset raise `input_error`.
dataframe compute_exposures(
    const dataframe &positions, const dataframe &factor_loadings,
    std::span<const std::string_view> factor_column_names,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

/// Returns risk exposures and return attribution for normalized positions.
///
/// `returns`, `normalized_positions` and `factor_returns` must have identical,
/// duplicate-free indexes. Factors are all double columns of `factor_returns`,
/// and at least one is required; their names must differ from the symbol column
/// and the reserved attribution names. The attribution frame holds one
/// contribution column per factor in factor-return column order, followed by
/// "total_returns" (the input returns), "common_returns" (sum of finite
/// contributions), "specific_returns" (`total - common`), "tilt_returns" (mean
/// exposure per factor times that date's factor return) and "timing_returns"
/// (`common - tilt`). Both returned frames own their data.
///
/// Mismatched or duplicate indexes, missing or non-string symbol columns,
/// missing or non-double factor columns, reserved factor names, duplicate
/// factor-loading keys, or factor-loading keys that do not cover the positions
/// raise `input_error`.
perf_attribution_result perf_attrib_core(
    const dataframe &returns, const dataframe &normalized_positions,
    const dataframe &factor_returns, const dataframe &factor_loadings,
    std::string_view returns_column_name = returns_column,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

/// Returns risk exposures and attribution from raw positions.
///
/// With `positions_in_dollars`, positions are first passed through
/// normalize_attribution_positions; otherwise they are treated as weights and
/// forwarded unchanged. The computation and errors are those of
/// normalize_attribution_positions when used, followed by perf_attrib_core.
perf_attribution_result perf_attrib(
    const dataframe &returns, const dataframe &positions,
    const dataframe &factor_returns, const dataframe &factor_loadings,
    bool positions_in_dollars = true,
    std::string_view returns_column_name = returns_column,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

/// Returns the return summary and per-factor statistics of an attribution.
///
/// `summary` holds "Annualized Specific Return", "Annualized Common Return",
/// "Annualized Total Return", "Specific Sharpe Ratio", "Cumulative Specific
/// Return", "Cumulative Common Return" and "Total Returns"; annualized values
/// compound with `annualization`, and the specific Sharpe ratio also uses it.
/// `factor_summary` has a positional 0..n-1 index with columns "factor",
/// "Average Risk Factor Exposure" (mean non-NaN exposure), "Annualized Return"
/// and "Cumulative Return" of each factor's contribution. Mismatched indexes or
/// a missing standard attribution column raise `input_error`.
perf_attribution_statistics create_perf_attrib_stats(
    const perf_attribution_result &result,
    double annualization = daily_annualization);

/// Returns cumulative_returns of `returns_column_name`.
///
/// This overload applies no cost deduction.
dataframe cumulative_returns_less_costs(
    const dataframe &returns,
    std::string_view returns_column_name = returns_column);

/// Returns cumulative growth after subtracting per-date costs.
///
/// `returns` and `costs` must have identical indexes; each return is reduced by
/// the cost in the same row before the result is compounded with
/// cumulative_returns. Mismatched indexes, malformed columns or invalid
/// selectors raise `input_error`.
dataframe cumulative_returns_less_costs(
    const dataframe &returns, const dataframe &costs,
    std::string_view returns_column_name = returns_column,
    std::string_view costs_column_name = returns_column);

} // namespace fincore
