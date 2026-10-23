#pragma once

#include <fincore/portfolio/positions.hpp>
#include <fincore/metrics/returns.hpp>

#include <map>
#include <span>
#include <string>
#include <string_view>

namespace fincore {

inline constexpr std::string_view attribution_symbol_column{"symbol"};
inline constexpr std::string_view attribution_total_returns_column{
    "total_returns"};
inline constexpr std::string_view attribution_common_returns_column{
    "common_returns"};
inline constexpr std::string_view attribution_specific_returns_column{
    "specific_returns"};
inline constexpr std::string_view attribution_tilt_returns_column{
    "tilt_returns"};
inline constexpr std::string_view attribution_timing_returns_column{
    "timing_returns"};
inline constexpr std::string_view attribution_factor_column{"factor"};

struct perf_attribution_result {
  dataframe risk_exposures;
  dataframe attribution;
};

struct perf_attribution_statistics {
  std::map<std::string, double> summary;
  dataframe factor_summary;
};

dataframe normalize_attribution_positions(
    const dataframe &positions, bool positions_in_dollars = true,
    std::string_view cash_column_name = cash_column);

dataframe compute_exposures(
    const dataframe &positions, const dataframe &factor_loadings,
    std::span<const std::string_view> factor_column_names,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

perf_attribution_result perf_attrib_core(
    const dataframe &returns, const dataframe &normalized_positions,
    const dataframe &factor_returns, const dataframe &factor_loadings,
    std::string_view returns_column_name = returns_column,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

perf_attribution_result perf_attrib(
    const dataframe &returns, const dataframe &positions,
    const dataframe &factor_returns, const dataframe &factor_loadings,
    bool positions_in_dollars = true,
    std::string_view returns_column_name = returns_column,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

perf_attribution_statistics create_perf_attrib_stats(
    const perf_attribution_result &result,
    double annualization = daily_annualization);

dataframe cumulative_returns_less_costs(
    const dataframe &returns,
    std::string_view returns_column_name = returns_column);

dataframe cumulative_returns_less_costs(
    const dataframe &returns, const dataframe &costs,
    std::string_view returns_column_name = returns_column,
    std::string_view costs_column_name = returns_column);

} // namespace fincore
