#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Returns the standard performance statistics of `returns` keyed by display
/// name.
///
/// The map always holds the thirteen keys "Annual return", "Cumulative
/// returns", "Annual volatility", "Sharpe ratio", "Calmar ratio", "Stability",
/// "Max drawdown", "Omega ratio", "Sortino ratio", "Skew", "Kurtosis", "Tail
/// ratio" and "Daily value at risk". Period-sensitive metrics use
/// `annualization_factor(period)`, while the Omega ratio is always daily
/// annualized. The column must be a double column matching the frame index, or
/// `input_error` is thrown.
std::map<std::string, double> perf_stats(
    const dataframe& returns,
    frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column);

/// Returns the unary statistics plus "Alpha" and "Beta".
///
/// The thirteen unary keys are computed as in the unary overload. Alpha and
/// beta come from alpha_beta on the common timestamps with a zero risk-free
/// rate and `daily_annualization` regardless of `period`. Both selectors must
/// be double columns matching their frame indexes, or `input_error` is thrown.
std::map<std::string, double> perf_stats(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Descriptive summary of a set of metric samples.
///
/// Every field is NaN for an empty sample. Quantiles use linear interpolation
/// and `std` is the population deviation.
struct distribution_stats {
    double mean;   ///< Arithmetic mean.
    double median; ///< 50th percentile.
    double std;    ///< Population standard deviation (divides by the count).
    double p5;     ///< 5th percentile.
    double p25;    ///< 25th percentile.
    double p75;    ///< 75th percentile.
    double p95;    ///< 95th percentile.
    double iqr;    ///< Interquartile range, `p75 - p25`.
};

/// Returns the mean, deviation and percentiles of `values`.
///
/// Values are used as given (no NaN filtering); percentiles use linear
/// interpolation at `(n - 1) * p / 100` and `std` divides by the sample count.
/// An empty span yields NaN in every field.
distribution_stats calc_distribution_stats(std::span<const double> values);

/// Bootstrap distribution of one performance metric.
struct bootstrap_metric_stats {
    std::string name;            ///< Display name of the metric.
    std::vector<double> samples; ///< Statistic of each draw, in draw order.
    distribution_stats summary;  ///< Summary of `samples`.
};

/// Returns bootstrap distributions for the thirteen perf_stats metrics.
///
/// Each of the `n_samples` draws resamples `returns.size()` observations with
/// replacement from `column` using `std::mt19937_64` seeded with `seed`; NaN
/// values are drawn verbatim. Period-sensitive metrics use
/// `daily_annualization`. The result always has thirteen entries in the fixed
/// bootstrap metric order; an empty input yields empty samples and NaN
/// summaries. An embedded NUL, empty or invalid `column` raises `input_error`.
std::vector<bootstrap_metric_stats> perf_stats_bootstrap(
    const dataframe& returns,
    std::size_t n_samples = 1000,
    std::uint64_t seed = 0,
    std::string_view column = returns_column);

/// Returns bootstrap distributions from caller-supplied draw indices.
///
/// Each draw contains positional row indices into the original input column.
/// Rows must have the same length as the input population. Empty input accepts
/// only an empty draw matrix and preserves the seeded overload's empty result.
/// An out-of-range index or an embedded NUL in `column` raises `input_error`.
std::vector<bootstrap_metric_stats> perf_stats_bootstrap(
    const dataframe& returns,
    std::span<const std::vector<std::size_t>> draw_indices,
    std::string_view column = returns_column);

}  // namespace fincore
