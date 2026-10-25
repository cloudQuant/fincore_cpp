#pragma once

#include <fincore/attribution/factor_regression.hpp>

#include <span>
#include <string_view>

namespace fincore {

/// Name of the component-label column in an attribution summary.
inline constexpr std::string_view factor_attribution_component_column{
    "component"};
/// Name of the contribution-value column in an attribution summary.
inline constexpr std::string_view factor_attribution_contribution_column{
    "contribution"};

/// Decomposes mean returns into alpha, factor, specific, common and
/// unexplained components.
///
/// The factors are fitted by ordinary least squares, each factor contributes
/// ``beta * mean(factor)``, and ``specific_return`` is the sum of those
/// contributions. ``common_return`` is ``mean(returns) - risk_free_rate`` and
/// ``unexplained`` is ``common_return - alpha - specific_return``. The output
/// index is ordinal and rows follow this order: ``alpha``, each
/// ``<factor>_attribution`` in request order, ``specific_return``,
/// ``common_return`` and ``unexplained``.
///
/// @param returns Frame containing the dependent return column.
/// @param factor_returns Frame containing the factor columns.
/// @param factor_column_names Non-empty distinct factor names; ``alpha`` is
///     reserved and must not be selected.
/// @param risk_free_rate Risk-free rate subtracted when forming the common
///     return; must be finite.
/// @param returns_column_name Double column used as the dependent variable.
/// @return A frame with ``component`` and ``contribution`` columns and an
///     ordinal index.
/// @throws input_error If the selectors, risk-free rate, regression inputs or
///     arithmetic results are invalid or non-finite.
dataframe factor_attribution_decomposition(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    double risk_free_rate = 0.0,
    std::string_view returns_column_name = returns_column);

}  // namespace fincore
