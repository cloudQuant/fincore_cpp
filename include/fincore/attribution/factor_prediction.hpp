#pragma once

#include <fincore/attribution/factor_regression.hpp>

#include <span>
#include <string_view>

namespace fincore {

/// Name of the predicted-return column.
inline constexpr std::string_view factor_prediction_column{"predicted_returns"};

/// Predicts returns from a fitted factor regression.
///
/// The coefficient table in ``regression`` must be structurally valid: an
/// ordinal index of length ``K + 1`` and the ``factor``/``coefficient``
/// columns, with ``alpha`` first and the remaining labels matching
/// ``factor_column_names`` in order. Predictions are
/// ``alpha + sum(beta_i * factor_i)`` for each input row.
///
/// @param regression Previously fitted factor regression.
/// @param factor_returns Frame containing the requested factor columns.
/// @param factor_column_names Non-empty distinct factor names matching the
///     coefficient labels; ``alpha`` is reserved.
/// @return A frame indexed like ``factor_returns`` with one
///     ``predicted_returns`` double column.
/// @throws input_error If the selectors, coefficient table, selected factor
///     values or arithmetic results are invalid or non-finite.
dataframe factor_regression_predictions(
    const factor_regression_result& regression,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names);

}  // namespace fincore
