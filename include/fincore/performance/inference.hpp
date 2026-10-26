#pragma once

#include <fincore/metrics/returns.hpp>

#include <string_view>
#include <utility>

namespace fincore {

/// Returns the standard error of the sample mean of ``column``.
///
/// NaN observations are dropped. The result is the sample standard deviation
/// (``n - 1`` denominator) divided by ``sqrt(n)``. Fewer than two non-NaN
/// observations yield NaN.
///
/// @param values Frame containing the observations.
/// @param column Double column of observations.
/// @return Standard error in the same units as ``column``.
/// @throws input_error If the column name is empty, the column is absent, not
///     a double column, or does not match the frame index length.
double standard_error_of_mean(const dataframe& values,
                              std::string_view column = returns_column);

/// Returns the standard error of the per-period Sharpe ratio.
///
/// Each non-NaN observation is shifted by ``risk_free``. The Sharpe ratio is
/// the shifted mean divided by the sample standard deviation (zero when that
/// deviation is at most 1e-15), and the standard error is
/// ``sqrt((1 + 0.5 * S^2) / n)``. Fewer than two non-NaN observations yield
/// NaN. No annualization is applied.
///
/// @param returns Frame containing periodic simple returns.
/// @param risk_free Per-period risk-free rate subtracted from each return.
/// @param column Double column of returns.
/// @return Standard error of the per-period Sharpe ratio.
/// @throws input_error If the column name is empty, the column is absent, not
///     a double column, or does not match the frame index length.
double sharpe_standard_error(const dataframe& returns,
                             double risk_free = 0.0,
                             std::string_view column = returns_column);

/// Returns a normal-approximation confidence interval for the per-period
/// Sharpe ratio.
///
/// The interval is ``S - z * SE`` to ``S + z * SE``, using the Sharpe ratio
/// and standard error described by sharpe_standard_error(). Fewer than two
/// non-NaN observations yield ``{NaN, NaN}``. No annualization is applied.
///
/// @param returns Frame containing periodic simple returns.
/// @param risk_free Per-period risk-free rate subtracted from each return.
/// @param z Normal quantile multiplier, such as 1.96 for a 95% interval.
/// @param column Double column of returns.
/// @return Lower and upper interval bounds.
/// @throws input_error If the column name is empty, the column is absent, not
///     a double column, or does not match the frame index length.
std::pair<double, double> sharpe_confidence_interval(
    const dataframe& returns,
    double risk_free = 0.0,
    double z = 1.96,
    std::string_view column = returns_column);

}  // namespace fincore
