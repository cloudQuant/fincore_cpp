#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <cstddef>
#include <string_view>

namespace fincore {

/// Returns the biased (population) skewness of `column`, ignoring NaN values.
///
/// Returns NaN with fewer than three usable values or zero variance.
double skewness(const dataframe& returns,
                std::string_view column = returns_column);

/// Returns the biased excess kurtosis (fourth moment over squared variance,
/// minus 3) of `column`, ignoring NaN values.
///
/// Returns NaN with fewer than four usable values or zero variance.
double kurtosis(const dataframe& returns,
                std::string_view column = returns_column);

/// Returns the Pearson autocorrelation of `column` at `lag` observations.
///
/// NaN values are removed before lagging. Returns NaN when `lag` is less than
/// one or fewer than `lag + 2` values remain.
double serial_correlation(const dataframe& returns,
                          std::ptrdiff_t lag = 1,
                          std::string_view column = returns_column);

/// Returns the rescaled-range Hurst exponent of `column`, ignoring NaN values.
///
/// The estimate is clamped to [0, 1]. Returns NaN with fewer than eight usable
/// values or a degenerate range/deviation.
double hurst_exponent(const dataframe& returns,
                      std::string_view column = returns_column);

/// Returns the Stutzer index of `column` relative to `target_return`.
///
/// NaN values are ignored; fewer than two usable values yield NaN. Returns 0
/// when the mean excess return is effectively zero.
double stutzer_index(const dataframe& returns,
                     double target_return = 0.0,
                     std::string_view column = returns_column);

/// Returns the RAR/Turtle ratio: the annualized slope of NAV against time
/// divided by the mean annual maximum drawdown magnitude.
///
/// Returns infinity when that mean drawdown is effectively zero and the
/// annualized slope is positive; NaN when it cannot be computed.
double r_cubed_turtle(const dataframe& returns,
                      double annualization = daily_annualization,
                      std::string_view column = returns_column);

/// Returns the fraction of usable observations in `column` that are strictly
/// positive.
///
/// NaN values are ignored; an empty input yields NaN.
double win_rate(const dataframe& returns,
                std::string_view column = returns_column);

/// Returns the fraction of usable observations in `column` that are strictly
/// negative.
///
/// NaN values are ignored; an empty input yields NaN.
double loss_rate(const dataframe& returns,
                 std::string_view column = returns_column);

/// Returns the Pearson correlation of the two series on their common finite
/// timestamps.
///
/// Returns NaN with fewer than two pairs or a near-zero standard deviation in
/// either series.
double safe_correlation(const dataframe& returns,
                        const dataframe& benchmark,
                        std::string_view returns_column_name = returns_column,
                        std::string_view benchmark_column_name = benchmark_column);

/// Returns the cube of safe_correlation.
///
/// Returns NaN when the correlation itself is NaN. Embedded NUL characters in
/// the column names raise `input_error`.
double r_cubed(const dataframe& returns,
               const dataframe& benchmark,
               std::string_view returns_column_name = returns_column,
               std::string_view benchmark_column_name = benchmark_column);

/// Returns the fraction of common finite dates where the strategy return is
/// strictly greater than the benchmark return.
double relative_win_rate(const dataframe& returns,
                         const dataframe& benchmark,
                         std::string_view returns_column_name = returns_column,
                         std::string_view benchmark_column_name = benchmark_column);

/// Returns the squared CAPM correlation, clamped to [0, 1].
///
/// Returns NaN when the correlation is unavailable.
double capm_r_squared(const dataframe& returns,
                      const dataframe& benchmark,
                      std::string_view returns_column_name = returns_column,
                      std::string_view benchmark_column_name = benchmark_column);

/// Returns the cumulative strategy return minus the cumulative benchmark
/// return over their common finite timestamps.
///
/// Returns NaN when no common finite pair exists.
double tracking_difference(const dataframe& returns,
                           const dataframe& benchmark,
                           std::string_view returns_column_name = returns_column,
                           std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
