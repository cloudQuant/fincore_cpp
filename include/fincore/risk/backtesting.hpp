#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace fincore {

/// Classification of a VaR or ES backtest.
enum class risk_backtest_status {
    pass,          ///< Coverage tests do not reject the model and the sample
                   ///< is adequate.
    fail,          ///< A coverage test rejects the model and the sample is
                   ///< adequate.
    inconclusive,  ///< Fewer than three paired observations or fewer than five
                   ///< expected exceptions; the sample is too small to test.
    experimental   ///< Experimental backtest; used by the ES bootstrap path.
};

/// Risk measure exercised by a backtest.
enum class risk_backtest_method {
    var,  ///< Value-at-risk exception backtest.
    es    ///< Expected-shortfall backtest.
};

/// Result of a VaR or ES backtest, including coverage statistics and a
/// deterministic input digest.
struct risk_backtest_result {
    risk_backtest_method method;  ///< Risk measure backtested.
    double confidence_level;      ///< Confidence level used for the forecast.
    std::size_t observations;  ///< Number of paired non-NaN observations.
    std::size_t exceptions;    ///< VaR: realized below forecast; ES: realized
                               ///< below the tail threshold.
    double expected_exceptions;  ///< `observations * (1 - confidence_level)`.
    double exception_rate;  ///< `exceptions / observations`; 0 when empty.
    double kupiec_lr;  ///< Kupiec unconditional-coverage statistic; 0 for ES.
    double kupiec_pvalue;  ///< Chi-square (df 1) p-value; 1 for ES.
    double christoffersen_lr;  ///< Independence statistic; 0 for ES.
    double christoffersen_pvalue;  ///< Chi-square (df 1) p-value; 1 for ES.
    std::string inputs_digest;  ///< Lowercase SHA-256 over aligned timestamps
                                ///< and values; native form, not the Python
                                ///< CSV digest.
    std::map<std::string, double> diagnostics;  ///< VaR adds `significance`
                                                ///< and `small_sample`; ES adds
                                                ///< `realized_es`,
                                                ///< `forecast_es`, `z_score`,
                                                ///< `n_bootstrap` and `seed`.
    risk_backtest_status status;  ///< Backtest classification.
};

/// Returns the Kupiec proportion-of-failures likelihood-ratio statistic.
///
/// The expected exception probability is `1 - confidence_level`. The statistic
/// is clamped at zero and is zero when `observations` is zero.
///
/// @param observations Number of backtest observations.
/// @param exceptions Number of observed exceptions.
/// @param confidence_level Model confidence level; the expected exception
///     probability is 1 - confidence_level.
/// @return Non-negative likelihood-ratio statistic.
double kupiec_lr(std::size_t observations, std::size_t exceptions,
                 double confidence_level);

/// Returns the Christoffersen independence likelihood-ratio statistic.
///
/// Flags are treated as binary; non-zero entries count as exceptions. Returns
/// zero when fewer than two flags are supplied.
///
/// @param exceptions Exception flags in chronological order.
/// @return Independence likelihood-ratio statistic.
double christoffersen_lr(std::span<const int> exceptions);

/// Runs a VaR exception backtest on aligned forecast and realized series.
///
/// Both frames are aligned on their common timestamps, and pairs with a NaN
/// forecast or realized value are dropped. An exception is a realized value
/// strictly below the forecast. The result is `inconclusive` when there are
/// fewer than three paired observations or fewer than five expected
/// exceptions; otherwise it is `fail` when either coverage p-value is below
/// `significance` and `pass` otherwise.
///
/// @param forecast Frame containing VaR forecasts.
/// @param realized Frame containing realized returns.
/// @param confidence_level Confidence level in (0, 1).
/// @param significance P-value threshold for rejecting coverage or
///     independence.
/// @param forecast_column Forecast column name.
/// @param realized_column Realized column name.
/// @return Backtest statistics, diagnostics, digest and status.
/// @throws input_error If `confidence_level` is not in (0, 1); a column is
///     missing, not a double column, or contains an embedded NUL; or the
///     frames share no timestamps.
risk_backtest_result backtest_var(
    const dataframe& forecast, const dataframe& realized,
    double confidence_level = 0.99, double significance = 0.05,
    std::string_view forecast_column = "forecast",
    std::string_view realized_column = "realized");

/// Runs an experimental bootstrap expected-shortfall backtest.
///
/// The realized tail contains realized values strictly below the linear
/// `1 - confidence_level` quantile; realized ES is the tail mean and forecast
/// ES the aligned forecast mean. A seeded bootstrap of the tail supplies the
/// standard deviation of resampled tail means used in the reported z-score.
/// The status is always `experimental`, and the Kupiec and Christoffersen
/// fields are neutral (0 and 1).
///
/// @param forecast Frame containing ES forecasts.
/// @param realized Frame containing realized returns.
/// @param confidence_level Confidence level in (0, 1).
/// @param n_bootstrap Number of bootstrap resamples of the realized tail.
/// @param seed Seed for the native `std::mt19937_64` bootstrap.
/// @param forecast_column Forecast column name.
/// @param realized_column Realized column name.
/// @return ES backtest diagnostics, digest and `experimental` status.
/// @throws input_error If `confidence_level` is not in (0, 1); a column is
///     missing, not a double column, or contains an embedded NUL; or the
///     frames share no timestamps.
risk_backtest_result backtest_es(
    const dataframe& forecast, const dataframe& realized,
    double confidence_level = 0.975, std::size_t n_bootstrap = 1000U,
    std::uint64_t seed = 20260817U,
    std::string_view forecast_column = "forecast",
    std::string_view realized_column = "realized");

/// Returns the expected number of VaR exceptions.
///
/// The expected count is `observations * (1 - confidence_level)`.
///
/// @param observations Number of observations.
/// @param confidence_level Model confidence level.
/// @return Expected exception count.
double expected_exception_count(std::size_t observations,
                                double confidence_level);

/// Classifies an exception count into a Basel traffic-light zone.
///
/// Zones are determined from binomial quantiles at 95% (green/yellow
/// boundary) and 99.99% (yellow/red boundary) for the expected exception
/// probability. Returns `green` when `observations` is zero.
///
/// @param exceptions Observed exception count.
/// @param observations Number of observations.
/// @param confidence_level Model confidence level in (0, 1).
/// @return `green`, `yellow`, or `red`.
/// @throws input_error If `observations` is non-zero and `confidence_level`
///     is not finite in (0, 1).
std::string basel_traffic_light(std::size_t exceptions,
                                std::size_t observations,
                                double confidence_level = 0.99);

/// Returns the relative realized-versus-forecast expected-shortfall score.
///
/// Realized ES is the mean of realized values at or below the linear
/// `1 - confidence_level` quantile, and is zero when that tail is empty. The
/// score is `(realized_es - forecast_es) / abs(forecast_es)`.
///
/// @param forecast_es Forecast expected shortfall.
/// @param realized Realized return observations.
/// @param confidence_level Confidence level used to select the realized tail.
/// @return Relative calibration score, or NaN when `abs(forecast_es)` is below
///     1e-15.
double es_calibration_score(double forecast_es,
                            std::span<const double> realized,
                            double confidence_level);

/// Returns the lowercase name of a backtest status.
///
/// @param status Backtest status.
/// @return `pass`, `fail`, `inconclusive`, or `experimental`.
std::string_view risk_backtest_status_name(risk_backtest_status status);

}  // namespace fincore
