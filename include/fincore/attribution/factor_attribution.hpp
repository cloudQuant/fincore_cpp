#pragma once

#include <fincore/attribution/factor_regression.hpp>

#include <span>
#include <string_view>

namespace fincore {

inline constexpr std::string_view factor_attribution_component_column{
    "component"};
inline constexpr std::string_view factor_attribution_contribution_column{
    "contribution"};

dataframe factor_attribution_decomposition(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    double risk_free_rate = 0.0,
    std::string_view returns_column_name = returns_column);

}  // namespace fincore
