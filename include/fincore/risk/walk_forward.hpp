#pragma once

#include <fincore/risk/backtesting.hpp>
#include <fincore/risk/specs.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Distribution labels for walk-forward VaR.
enum class risk_distribution {
    historical,  ///< Empirical distribution using the Weibull quantile.
    normal       ///< Normal distribution using the mean and sample deviation.
};

/// Completion status of a walk-forward VaR run.
enum class walk_forward_status {
    ok,                 ///< Forecast path and backtest were produced.
    insufficient_data,  ///< Fewer observations than one full window plus one.
    unsupported         ///< The specification combination is not supported.
};

/// Parameters recorded for one walk-forward refit.
struct refit_parameters {
    timestamp date;  ///< Timestamp of the first forecast step from this refit.
    double forecast;  ///< VaR forecast for the refit segment.
    double mean;      ///< Mean of the estimation window.
    double standard_deviation;  ///< Sample standard deviation (n - 1) of the
                                ///< estimation window.
    std::size_t n_observations;  ///< Estimation window length.
};

/// Result of a walk-forward VaR run.
struct walk_forward_var_result {
    risk_model_spec spec;  ///< Specification used to produce the result.
    std::vector<timestamp> index;  ///< Out-of-sample timestamps; empty unless
                                   ///< `status` is `ok`.
    std::vector<double> forecast;  ///< VaR forecast path aligned with `index`;
                                   ///< empty unless `status` is `ok`.
    std::vector<double> realized;  ///< Realized returns aligned with `index`;
                                   ///< empty unless `status` is `ok`.
    std::vector<timestamp> refit_timestamps;  ///< Timestamps of all refits;
                                              ///< the first equals
                                              ///< `index.front()` when `ok`.
    std::vector<refit_parameters> fits;  ///< Parameters recorded for each
                                         ///< refit, one per
                                         ///< `refit_timestamps` entry.
    std::string inputs_digest;  ///< Lowercase SHA-256 hex digest of the input
                                ///< values and the confidence level, window,
                                ///< refit cadence, distribution and model
                                ///< version.
    walk_forward_status status;  ///< Run status.
    std::string unsupported_reason;  ///< Reason for an `unsupported` or
                                     ///< `insufficient_data` result; empty for
                                     ///< `ok`.
    std::size_t n_observations = 0U;  ///< Number of input observations.
    risk_backtest_result backtest;  ///< Out-of-sample VaR backtest; meaningful
                                    ///< only when `has_backtest` is true.
    bool has_backtest;  ///< Whether `backtest` was populated.
};

/// Runs a rolling-window walk-forward VaR backtest.
///
/// The first forecast is produced for the observation after the initial
/// `spec.window`; refits occur at that point and every `spec.refit_cadence`
/// steps, each using exactly the preceding window. Normal forecasts use the
/// mean plus the sample deviation times the normal quantile, falling back to
/// the mean on zero deviation; historical forecasts use the type-6 (Weibull)
/// quantile. The resulting path is backtested at a 0.05 significance level. A
/// path requires ``forecast_target='var'``, ``tail='lower'``,
/// ``sign_convention='losses_negative'`` and ``horizon=1``; a horizon above 1,
/// another target, tail or sign convention, or a historical rank
/// ``(window + 1) * (1 - confidence_level)`` outside ``[1, window]`` up to
/// floating-point tolerance returns ``unsupported`` with a reason.
///
/// @param returns Return series with strictly increasing, duplicate-free
///     timestamps and finite values.
/// @param spec Model specification.
/// @param column Column holding decimal simple returns.
/// @return Forecast path, refit parameters, backtest and status. Unsupported
///     and insufficient-data results carry an empty path and a reason.
/// @throws input_error If the specification is invalid, the index is not
///     strictly increasing, a value is not finite, or the column is missing or
///     contains an embedded NUL.
walk_forward_var_result walk_forward_var(
    const dataframe& returns, const risk_model_spec& spec,
    std::string_view column = returns_column);

/// Returns the lowercase name of a walk-forward status.
///
/// @param status Walk-forward status.
/// @return `ok`, `insufficient_data`, or `unsupported`.
std::string_view walk_forward_status_name(walk_forward_status status);

}  // namespace fincore
