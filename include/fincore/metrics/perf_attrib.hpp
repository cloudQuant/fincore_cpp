#pragma once

#include <fincore/metrics/positions.hpp>

#include <span>
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

dataframe normalize_attribution_positions(
    const dataframe &positions, bool positions_in_dollars = true,
    std::string_view cash_column_name = cash_column);

dataframe compute_exposures(
    const dataframe &positions, const dataframe &factor_loadings,
    std::span<const std::string_view> factor_column_names,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

} // namespace fincore
