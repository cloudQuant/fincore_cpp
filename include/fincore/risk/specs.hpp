#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace fincore {

/// Target risk measure produced by a forecast.
enum class risk_forecast_target {
    var,   ///< Value at risk.
    es,    ///< Expected shortfall.
    pair   ///< Pair target label for cross-component metadata.
};

/// Tail of the return distribution used by a risk model.
enum class risk_tail {
    lower,  ///< Lower (loss) tail.
    upper   ///< Upper (gain) tail.
};

/// Versioned model specification shared by walk-forward VaR and validation
/// reports.
struct risk_model_spec {
    risk_forecast_target forecast_target = risk_forecast_target::var;  ///< Forecast target; walk-forward VaR supports `var` only.
    double confidence_level = 0.99;  ///< Confidence level; must be finite in (0, 1).
    std::size_t horizon = 1U;  ///< Forecast horizon in observations; must be at least 1.
    std::string distribution = "normal";  ///< Distribution label; `historical` or `normal`.
    risk_tail tail = risk_tail::lower;  ///< Tail used by the model; walk-forward VaR supports `lower` only.
    std::string sign_convention = "losses_negative";  ///< Sign convention label; only `losses_negative` is accepted.
    std::size_t window = 252U;  ///< Estimation window length in observations; at least 2.
    std::size_t refit_cadence = 1U;  ///< Number of forecast steps between refits; at least 1.
    std::string model_version = "1.0";  ///< Model version label; only `1.0` is accepted.
};

/// Validates a model specification against the supported contract.
///
/// @param spec Specification to validate.
/// @throws input_error If `confidence_level` is not finite in (0, 1),
///     `horizon` is below 1, `distribution` is not `historical` or `normal`,
///     `sign_convention` is not `losses_negative`, `window` is below 2,
///     `refit_cadence` is below 1, or `model_version` is not `1.0`.
void validate_risk_model_spec(const risk_model_spec& spec);

/// Returns the lowercase name of a forecast target.
///
/// @param target Forecast target.
/// @return `var`, `es`, or `pair`.
std::string_view risk_forecast_target_name(risk_forecast_target target);

/// Returns the lowercase name of a distribution tail.
///
/// @param tail Distribution tail.
/// @return `lower` or `upper`.
std::string_view risk_tail_name(risk_tail tail);

}  // namespace fincore
