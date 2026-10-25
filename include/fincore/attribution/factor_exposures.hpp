#pragma once

#include <fincore/attribution/factor_regression.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace fincore {

/// Computes rolling or expanding factor exposures by least squares.
///
/// Each output row fits the requested factors on the window ending at that
/// row. A supplied ``rolling_window`` limits the window to that many trailing
/// observations; ``std::nullopt`` uses an expanding window from the first row.
/// Rows with at most ``K`` observations for ``K`` factors are NaN.
///
/// @param returns Frame containing the dependent return column.
/// @param factor_returns Frame containing the factor columns.
/// @param factor_column_names Non-empty distinct factor names; ``alpha`` is
///     reserved and must not be selected.
/// @param rolling_window Optional trailing window length; must be positive
///     when supplied.
/// @param returns_column_name Double column used as the dependent variable.
/// @return A frame indexed like ``returns`` with ``alpha`` followed by one
///     double column per requested factor.
/// @throws input_error If selectors are empty, duplicated, reserved or
///     malformed, the window is zero, an index is duplicated or mismatched, a
///     selected value is non-finite, an eligible window is rank deficient, or
///     arithmetic overflows.
dataframe rolling_factor_exposures(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::optional<std::size_t> rolling_window = std::nullopt,
    std::string_view returns_column_name = returns_column);

}  // namespace fincore
