#pragma once

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>

#include <string_view>

namespace fincore {

/// Returns the annualized Sharpe ratio of `column`.
///
/// NaN observations are dropped; the mean of `value - risk_free` is divided by
/// the sample standard deviation (one degree of freedom) and scaled by
/// `sqrt(annualization)`. Fewer than two usable observations yield NaN, and a
/// zero deviation is not special-cased, so the quotient follows IEEE rules.
double sharpe_ratio(const dataframe& returns,
                    double risk_free = 0.0,
                    double annualization = daily_annualization,
                    std::string_view column = returns_column);

/// Returns the Sharpe ratio adjusted for the skewness and kurtosis of the
/// return distribution.
///
/// The Sharpe ratio is annualized with `daily_annualization` and multiplied by
/// a dampened adjustment built from the biased skewness and excess kurtosis of
/// the non-NaN values. The adjustment is clamped to [0.9, 1.1] for fewer than
/// 20 raw observations and to [0.8, 1.3] otherwise. Fewer than four raw values
/// or a NaN Sharpe ratio yield NaN; an embedded NUL in `column` raises
/// `input_error`.
double adjusted_sharpe_ratio(const dataframe& returns,
                             double risk_free = 0.0,
                             std::string_view column = returns_column);

/// Returns the annualized Sortino ratio of `column`.
///
/// NaN observations are dropped; the annualized mean excess over
/// `required_return` is divided by the annualized downside deviation, where
/// only negative excess values contribute and the squared deviations divide by
/// the number of usable observations. Fewer than two raw values, or no usable
/// observation, yield NaN, and a zero downside deviation is not special-cased.
double sortino_ratio(const dataframe& returns,
                     double required_return = 0.0,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);

/// Returns the annual return in excess of `risk_free` divided by the absolute
/// maximum drawdown.
///
/// The annual return is compounded at `annualization`. Returns NaN when the
/// series never declines (the maximum drawdown is zero or NaN) or the quotient
/// is not finite.
double calmar_ratio(const dataframe& returns,
                    double risk_free = 0.0,
                    double annualization = daily_annualization,
                    std::string_view column = returns_column);

/// Returns the annualized arithmetic mean return divided by the absolute
/// maximum drawdown.
///
/// The mean uses the non-NaN values and is scaled by `annualization` rather
/// than compounded. Returns NaN for fewer than two raw values, a maximum
/// drawdown that is non-negative or NaN, or a non-finite quotient. An embedded
/// NUL in `column` raises `input_error`.
double mar_ratio(const dataframe& returns,
                 double annualization = daily_annualization,
                 std::string_view column = returns_column);

/// Returns the Sharpe ratio of the left tail defined by `cutoff`.
///
/// The cutoff quantile is linearly interpolated at `(n - 1) * cutoff`; every
/// value at or below it forms the tail. The tail mean excess over `risk_free`
/// is divided by the tail sample deviation (one degree of freedom) and scaled
/// by `sqrt(annualization)`. Returns NaN when `cutoff` is not in (0, 1), any
/// raw value is NaN, the tail has fewer than two values, or the deviation is
/// zero or non-finite. An embedded NUL in `column` raises `input_error`.
double conditional_sharpe_ratio(const dataframe& returns,
                                double cutoff = 0.05,
                                double risk_free = 0.0,
                                double annualization = daily_annualization,
                                std::string_view column = returns_column);

/// Returns the deflated Sharpe ratio probability for `num_trials` trials.
///
/// NaN observations are dropped and the per-period Sharpe ratio uses a
/// one-degree-of-freedom deviation. The trial-adjusted benchmark uses the
/// sample skewness and kurtosis and the expected maximum of `num_trials`
/// independent trials (values below one are treated as one). The result is the
/// standard normal CDF of the observed excess over that benchmark, or exactly
/// 0 or 1 when the estimated Sharpe variance is non-positive. Fewer than three
/// usable values yield NaN. An embedded NUL in `column` raises `input_error`.
double deflated_sharpe_ratio(const dataframe& returns,
                             double risk_free = 0.0,
                             int num_trials = 1,
                             std::string_view column = returns_column);

/// Returns the annual return in excess of `risk_free` divided by the Burke
/// drawdown risk measure.
///
/// The risk measure is the square root of the sum of squared drawdown depths;
/// when there are no drawdowns or all are zero, it falls back to the population
/// standard deviation of the negative returns (or of all usable values when
/// none are negative). The annual return is compounded at `annualization`.
/// Returns positive infinity when the risk measure is below 1e-10 and the
/// excess annual return is positive, NaN in the corresponding non-positive
/// case, and NaN for fewer than two raw values or no usable value. An embedded
/// NUL in `column` raises `input_error`.
double burke_ratio(const dataframe& returns,
                   double risk_free = 0.0,
                   double annualization = daily_annualization,
                   std::string_view column = returns_column);

/// Returns the annual return in excess of `risk_free` divided by the average
/// drawdown magnitude.
///
/// The average drawdown is the absolute mean of all drawdown depths; with no
/// drawdowns or only zeros it falls back to the absolute mean of the negative
/// returns (or the absolute population deviation of all usable values when
/// none are negative). The annual return is compounded at `annualization`.
/// Returns positive infinity when the denominator is below 1e-10 and the excess
/// annual return is positive, NaN otherwise, and NaN for fewer than two raw
/// values or no usable value. An embedded NUL in `column` raises `input_error`.
double sterling_ratio(const dataframe& returns,
                      double risk_free = 0.0,
                      double annualization = daily_annualization,
                      std::string_view column = returns_column);

/// Returns the annualized Treynor ratio of `returns` against `benchmark`.
///
/// Both series are aligned on their common timestamps; the annual return in
/// excess of `risk_free` is divided by the beta of the aligned pair. Returns
/// NaN when fewer than two aligned returns remain or beta is NaN, zero or
/// negative. An embedded NUL in either column name raises `input_error`.
double cal_treynor_ratio(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the M-squared measure of `returns` against `benchmark`.
///
/// Both series are aligned on their common timestamps. The excess annual
/// return is rescaled by benchmark volatility over portfolio volatility and
/// `risk_free` is added back. Returns NaN when fewer than two aligned returns
/// remain, portfolio volatility is zero, negative or NaN, or benchmark
/// volatility is NaN. An embedded NUL in either column name raises
/// `input_error`.
double m_squared(const dataframe& returns,
                 const dataframe& benchmark,
                 double risk_free = 0.0,
                 double annualization = daily_annualization,
                 std::string_view returns_column_name = returns_column,
                 std::string_view benchmark_column_name = benchmark_column);

/// Returns the annualized geometric return of `returns` on dates where the
/// benchmark return is strictly positive.
///
/// Only common timestamps are considered. Selected NaN returns are treated as
/// zero when compounding. Returns NaN when no date qualifies and -1 when the
/// compounded wealth is non-positive. An embedded NUL in either column name
/// raises `input_error`.
double up_capture_return(
    const dataframe& returns,
    const dataframe& benchmark,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the annualized geometric return of `returns` on dates where the
/// benchmark return is strictly negative.
///
/// Matches up_capture_return apart from selecting dates where the benchmark
/// return is negative, including its NaN and -1 outcomes and its embedded-NUL
/// `input_error`.
double down_capture_return(
    const dataframe& returns,
    const dataframe& benchmark,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the common-sense ratio: the tail ratio times the win rate divided
/// by the loss rate.
///
/// Both components are taken from `column` with their own NaN rules. Returns
/// NaN for fewer than two raw values, a NaN component, or a zero win rate, and
/// positive infinity when every usable observation is a gain. An embedded NUL
/// in `column` raises `input_error`.
double common_sense_ratio(const dataframe& returns,
                          std::string_view column = returns_column);

/// Returns the Kappa 3 ratio: the mean excess over `mar` divided by the cube
/// root of the third lower partial moment.
///
/// The lower partial moment averages `max(0, mar - value)^3` over the non-NaN
/// values. Returns NaN for fewer than two usable values, a NaN `mar`, or a
/// moment below 1e-30 when the mean does not exceed `mar`; the same degenerate
/// case returns positive infinity when it does. An embedded NUL in `column`
/// raises `input_error`.
double kappa_three_ratio(const dataframe& returns,
                         double mar = 0.0,
                         std::string_view column = returns_column);

/// Returns the mean active return divided by its sample deviation.
///
/// Both series are aligned on their common timestamps and only finite pairs
/// are kept. The result is not annualized. Returns NaN when fewer than two
/// pairs remain or the tracking error is below 1e-15.
double excess_sharpe(const dataframe& returns,
                     const dataframe& benchmark,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);

/// Returns the annualized information ratio of `returns` against `benchmark`.
///
/// Only finite common pairs are used. The annualized mean active return is
/// divided by the active sample deviation scaled by `sqrt(annualization)`.
/// Returns NaN when fewer than two pairs remain, `annualization` is not finite
/// or not positive, or the annualized tracking error is zero or non-finite.
double information_ratio(const dataframe& returns,
                         const dataframe& benchmark,
                         double annualization = daily_annualization,
                         std::string_view returns_column_name = returns_column,
                         std::string_view benchmark_column_name = benchmark_column);

/// Returns the Omega ratio of `column` above the required threshold.
///
/// Non-finite values are dropped. The per-period threshold is `risk_free` plus
/// `required_return` converted to a per-period rate when `annualization` is not
/// one. Gains above and losses below the threshold are summed and the ratio is
/// gains over losses. Returns NaN for fewer than two usable values, a
/// non-finite parameter, a non-positive `annualization`, a required return at
/// or below -1 with an annualization other than one, or a non-positive or
/// non-finite gain/loss sum.
double omega_ratio(const dataframe& returns,
                   double risk_free = 0.0,
                   double required_return = 0.0,
                   double annualization = daily_annualization,
                   std::string_view column = returns_column);

/// Returns the R-squared of cumulative log returns regressed on a linear time
/// index.
///
/// Non-finite values are dropped and `log1p` is applied to each; the cumulative
/// sum is regressed on the observation position. Returns NaN for fewer than two
/// usable values, a non-finite log return (for example a return at or below
/// -1), an overflowing cumulative sum, or zero explained variance.
double stability_of_timeseries(const dataframe& returns,
                               std::string_view column = returns_column);

/// Returns the ratio of the annualized geometric return of `returns` to that
/// of `benchmark`.
///
/// Both series are aligned on their common timestamps and only finite pairs
/// are kept; each side is compounded over its paired observations and
/// annualized with `annualization`. Returns NaN when no pair remains, either
/// annual return is non-finite, the benchmark annual return is zero, or the
/// quotient is not finite.
double capture(const dataframe& returns,
               const dataframe& benchmark,
               double annualization = daily_annualization,
               std::string_view returns_column_name = returns_column,
               std::string_view benchmark_column_name = benchmark_column);

/// Returns the capture ratio on dates where the benchmark return is strictly
/// positive.
///
/// Follows capture on the selected common finite pairs. Returns NaN when no
/// date qualifies or the ratio is not finite.
double up_capture(const dataframe& returns,
                  const dataframe& benchmark,
                  double annualization = daily_annualization,
                  std::string_view returns_column_name = returns_column,
                  std::string_view benchmark_column_name = benchmark_column);

/// Returns the capture ratio on dates where the benchmark return is strictly
/// negative.
///
/// Follows capture on the selected common finite pairs. Returns NaN when no
/// date qualifies or the ratio is not finite.
double down_capture(const dataframe& returns,
                    const dataframe& benchmark,
                    double annualization = daily_annualization,
                    std::string_view returns_column_name = returns_column,
                    std::string_view benchmark_column_name = benchmark_column);

/// Returns up_capture divided by down_capture.
///
/// Both components use the common finite pairs. Returns NaN when either
/// component is non-finite or the down capture is zero.
double up_down_capture(const dataframe& returns,
                       const dataframe& benchmark,
                       double annualization = daily_annualization,
                       std::string_view returns_column_name = returns_column,
                       std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
