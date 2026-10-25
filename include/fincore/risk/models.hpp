#pragma once

#include <fincore/risk/evt.hpp>
#include <fincore/risk/garch.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Risk-estimation method used by `forecast_var` and `forecast_es`.
enum class risk_forecast_method {
    historical,  ///< Historical quantile and tail-mean estimate.
    evt,         ///< Extreme-value estimate using the configured EVT kernel.
    garch        ///< GARCH-family conditional estimate.
};

/// Completion status of a risk estimate.
enum class risk_estimate_status {
    ok,                 ///< At least two non-NaN observations were available
                        ///< and any GARCH fit converged.
    insufficient_data,  ///< Fewer than two non-NaN observations were available.
    failed              ///< The GARCH fit did not converge or failed its
                        ///< stationarity checks.
};

/// Sign-convention label stating that losses are negative returns.
inline constexpr std::string_view losses_negative_sign_convention =
    "losses_negative";

/// Settings for a point VaR or expected-shortfall estimate.
struct risk_forecast_config {
    risk_forecast_method method = risk_forecast_method::historical;  ///< Estimation method.
    double confidence_level = 0.95;  ///< Confidence level in (0, 1); the tail probability is 1 - confidence_level.
    std::size_t horizon = 1U;  ///< Forecast horizon in observations; used by the GARCH method and must be at least 1.
    garch_model garch_kernel = garch_model::garch;  ///< GARCH family used when `method` is `garch`.
    evt_model evt_kernel = evt_model::gpd;  ///< EVT model used when `method` is `evt`.
    evt_tail tail = evt_tail::lower;  ///< EVT tail used when `method` is `evt`.
    std::optional<double> threshold;  ///< GPD threshold in absolute magnitude
                                      ///< units; empty selects the automatic
                                      ///< threshold and GEV ignores it.
    std::optional<std::size_t> block_size;  ///< GEV block size in observations;
                                            ///< empty selects the automatic
                                            ///< block size and GPD ignores it.
};

/// Point risk estimate plus provenance and diagnostics.
struct risk_estimate {
    risk_forecast_method method = risk_forecast_method::historical;  ///< Estimation method.
    double confidence_level = 0.95;  ///< Confidence level used for the estimate.
    std::size_t horizon = 1U;  ///< Configured forecast horizon.
    std::string sign_convention{losses_negative_sign_convention};  ///< Sign convention label; `losses_negative`.
    double estimate = 0.0;  ///< Point estimate; negative for losses under `losses_negative`.
    timestamp forecast_timestamp = 0;  ///< Timestamp of the last cleaned observation used by the estimate.
    bool has_forecast_timestamp = false;  ///< Whether `forecast_timestamp` was populated.
    std::string inputs_digest;  ///< Lowercase SHA-256 hex digest of the cleaned timestamps and values.
    bool has_forecast = false;  ///< Whether a forward forecast path is populated; current point estimates leave it false.
    std::vector<timestamp> forecast_index;  ///< Optional forecast timestamps; empty for point estimates.
    std::vector<double> forecast_values;  ///< Optional forecast values; empty for point estimates.
    std::map<std::string, double> diagnostics;  ///< Method-specific numeric diagnostics; keys depend on `method`.
    std::string kernel;  ///< Backend kernel: empty for historical,
                         ///< `evt_var`/`evt_cvar` for EVT, and
                         ///< `conditional_var`/`conditional_es` for GARCH.
    std::string note;  ///< Note populated when a GARCH fit does not converge; empty otherwise.
    risk_estimate_status status = risk_estimate_status::ok;  ///< Estimate status.
};

/// Estimates the value at risk of a return series.
///
/// NaN observations are dropped before estimation, and the index must be
/// strictly ascending and duplicate-free. Historical and EVT methods return a
/// single point estimate and do not use `config.horizon`; the GARCH method
/// composes the conditional variance forecast over the configured horizon.
///
/// @param returns Return series with ascending, duplicate-free timestamps.
/// @param config Estimation method, confidence level and method settings.
/// @param column Column holding decimal simple returns.
/// @return Point VaR estimate, diagnostics, digest and status. The estimate is
///     negative for losses under the `losses_negative` convention.
/// @throws input_error If the configuration is invalid; the column is missing,
///     not a double column, or contains an embedded NUL; the index is
///     duplicated or unsorted; no non-NaN observation remains; or the selected
///     EVT or GARCH kernel rejects the data.
risk_estimate forecast_var(
    const dataframe& returns,
    const risk_forecast_config& config = {},
    std::string_view column = returns_column);

/// Estimates the expected shortfall of a return series.
///
/// NaN observations are dropped before estimation, and the index must be
/// strictly ascending and duplicate-free. Historical and EVT methods return a
/// single point estimate and do not use `config.horizon`; the GARCH method
/// composes the conditional variance forecast over the configured horizon.
///
/// @param returns Return series with ascending, duplicate-free timestamps.
/// @param config Estimation method, confidence level and method settings.
/// @param column Column holding decimal simple returns.
/// @return Point expected-shortfall estimate, diagnostics, digest and status.
///     The estimate is negative for losses under the `losses_negative`
///     convention.
/// @throws input_error If the configuration is invalid; the column is missing,
///     not a double column, or contains an embedded NUL; the index is
///     duplicated or unsorted; no non-NaN observation remains; or the selected
///     EVT or GARCH kernel rejects the data.
risk_estimate forecast_es(
    const dataframe& returns,
    const risk_forecast_config& config = {},
    std::string_view column = returns_column);

}  // namespace fincore
