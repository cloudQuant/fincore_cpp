#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <string_view>

namespace fincore {

/// Returns the empirical `cutoff` quantile of `column` using linear
/// interpolation.
///
/// No annualization is applied. Returns NaN unless every value is finite and
/// `cutoff` lies in [0, 1].
double historical_value_at_risk(const dataframe &returns, double cutoff = 0.05,
                                std::string_view column = returns_column);

/// Returns `mean - sigma * sample_standard_deviation` of `column`.
///
/// Requires at least two values; NaN observations propagate to the result.
double trading_value_at_risk(
    const dataframe& returns, double sigma = 2.0,
    std::string_view column = returns_column);

/// Returns the annualized return in excess of `risk_free`, divided by the
/// absolute historical value at risk at `cutoff`.
///
/// The return is annualized with `annualization_factor(period,
/// annualization)`. Returns NaN with fewer than two values, a zero or NaN
/// value at risk, or a NaN annual return.
double var_excess_return(
    const dataframe& returns, double cutoff = 0.05, double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

/// Returns the mean of all observations at or below the empirical `cutoff`
/// quantile of `column`.
///
/// Returns NaN unless every value is finite and `cutoff` lies in [0, 1].
double conditional_value_at_risk(const dataframe &returns, double cutoff = 0.05,
                                 std::string_view column = returns_column);

/// Returns the annualized downside deviation relative to `required_return`.
///
/// The result is the square root of the mean squared shortfall over all finite
/// observations, scaled by `sqrt(annualization)`. Returns NaN when there are no
/// finite values or `annualization` is negative or non-finite.
double downside_risk(const dataframe &returns, double required_return = 0.0,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);

/// Returns the ratio of the absolute 95th percentile to the absolute 5th
/// percentile of the finite values in `column`.
///
/// Returns NaN for empty input or a zero lower tail.
double tail_ratio(const dataframe &returns,
                  std::string_view column = returns_column);

/// Returns the annualized sample standard deviation of active returns
/// (strategy minus benchmark) on common finite timestamps.
///
/// Returns NaN with fewer than two pairs, non-finite/negative `annualization`,
/// or a non-finite variance.
double
tracking_error(const dataframe &returns, const dataframe &benchmark,
               double annualization = daily_annualization,
               std::string_view returns_column_name = returns_column,
               std::string_view benchmark_column_name = benchmark_column);

/// Returns the annualized sample standard deviation of the residuals from a
/// CAPM regression of excess strategy returns on excess benchmark returns.
///
/// Returns NaN when `risk_free` or beta is not finite.
double residual_risk(const dataframe &returns, const dataframe &benchmark,
                     double risk_free = 0.0,
                     double annualization = daily_annualization,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);

/// Returns the variance-covariance value at risk under a normal assumption:
/// `portfolio_value * (z(confidence) * standard_deviation - mean)`.
///
/// No annualization is applied. Returns NaN when `confidence` is outside
/// [0, 1] or `standard_deviation` is not positive.
double var_cov_var_normal(double portfolio_value, double confidence,
                          double mean = 0.0, double standard_deviation = 1.0);

/// Generalized Pareto tail estimates of a return distribution.
struct gpd_risk_estimates_result {
    double threshold; ///< Loss threshold selected for the tail fit.
    double scale;     ///< GPD scale parameter.
    double shape;     ///< GPD shape parameter (positive for a fitted tail).
    double var;       ///< Value-at-risk estimate at the requested probability.
    double es;        ///< Expected shortfall estimate.
};

/// Returns GPD tail estimates of losses (negated negative returns), fitted by
/// maximum likelihood with a threshold search starting at 0.2 and halving.
///
/// `var_p` is the tail probability. All fields are zero when there are fewer
/// than three returns, no positive-shape fit is found, or the search does not
/// converge.
gpd_risk_estimates_result gpd_risk_estimates(
    const dataframe& returns,
    double var_p = 0.01,
    std::string_view column = returns_column);

/// Returns a heuristic beta-fragility score comparing returns at the low and
/// high ends of the factor-sorted sample with the middle observation.
///
/// Requires at least three aligned non-NaN pairs; otherwise returns NaN.
/// Embedded NUL characters in the column names raise `input_error`.
double beta_fragility_heuristic(
    const dataframe& returns,
    const dataframe& benchmark,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

} // namespace fincore
