#include <fincore/risk/specs.hpp>

#include <fincore/error.hpp>

#include <cmath>

namespace fincore {

void validate_risk_model_spec(const risk_model_spec& spec) {
    if (!std::isfinite(spec.confidence_level) ||
        !(spec.confidence_level > 0.0 && spec.confidence_level < 1.0)) {
        throw input_error{
            "confidence_level must be a finite real number in (0, 1)"};
    }
    if (spec.horizon < 1U) {
        throw input_error{"horizon must be an integer at least 1"};
    }
    if (spec.distribution != "historical" && spec.distribution != "normal") {
        throw input_error{
            "distribution must be one of: 'historical', 'normal'"};
    }
    if (spec.sign_convention != "losses_negative") {
        throw input_error{
            "sign_convention must be one of: 'losses_negative'"};
    }
    if (spec.window < 2U) {
        throw input_error{"window must be an integer at least 2"};
    }
    if (spec.refit_cadence < 1U) {
        throw input_error{"refit_cadence must be an integer at least 1"};
    }
    if (spec.model_version != "1.0") {
        throw input_error{"model_version must be one of: '1.0'"};
    }
}

std::string_view risk_forecast_target_name(const risk_forecast_target target) {
    switch (target) {
        case risk_forecast_target::var:
            return "var";
        case risk_forecast_target::es:
            return "es";
        case risk_forecast_target::pair:
            return "pair";
    }
    return "var";
}

std::string_view risk_tail_name(const risk_tail tail) {
    switch (tail) {
        case risk_tail::lower:
            return "lower";
        case risk_tail::upper:
            return "upper";
    }
    return "lower";
}

}  // namespace fincore
