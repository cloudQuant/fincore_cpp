#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/metrics/returns.hpp>

#include <span>
#include <string_view>

namespace fincore {

/// Name of the factor-label column in a coefficient frame.
inline constexpr std::string_view factor_regression_factor_column{"factor"};
/// Name of the coefficient-value column in a coefficient frame.
inline constexpr std::string_view factor_regression_coefficient_column{
    "coefficient"};
/// Name of the fitted-return column.
inline constexpr std::string_view factor_regression_fitted_column{
    "fitted_returns"};
/// Name of the residual column.
inline constexpr std::string_view factor_regression_residual_column{
    "residuals"};
/// Label used for the regression intercept.
inline constexpr std::string_view factor_regression_alpha_name{"alpha"};

/// Result of an ordinary least-squares factor regression.
///
/// All frames own their data. ``coefficients`` has an ordinal index and the
/// columns ``factor`` and ``coefficient``, with ``alpha`` first followed by
/// the requested factor names in request order.
struct factor_regression_result {
    double alpha;                 ///< Estimated intercept.
    double r_squared;             ///< `1 - SSE/SST`; 0 for exactly constant returns.
    dataframe coefficients;       ///< Alpha and factor coefficient table.
    dataframe fitted_returns;     ///< Fitted values on the returns index.
    dataframe residuals;          ///< Observed minus fitted values on the returns index.
};

/// Fits an ordinary least-squares factor regression with an intercept.
///
/// The dependent column is regressed on the named factor columns. Both frames
/// must share the same unique index, selected values must be finite, and at
/// least ``K + 1`` observations are required for ``K`` factors. The fit uses a
/// Householder QR solve and rejects rank-deficient designs.
///
/// @param returns Frame containing the dependent return column.
/// @param factor_returns Frame containing the factor columns.
/// @param factor_column_names Non-empty distinct factor names; ``alpha`` is
///     reserved and must not be selected.
/// @param returns_column_name Double column used as the dependent variable.
/// @return The intercept, R-squared, coefficient table, fitted values and
///     residuals.
/// @throws input_error If the selectors are empty, duplicated or reserved, the
///     indexes are duplicated or mismatched, a selected column is missing or
///     non-finite, there are fewer than ``K + 1`` observations, the design is
///     rank deficient, or an intermediate is not finite.
factor_regression_result fit_factor_regression(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::string_view returns_column_name = returns_column);

}  // namespace fincore
