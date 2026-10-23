#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace fincore {

enum class risk_forecast_target { var, es, pair };
enum class risk_tail { lower, upper };

struct risk_model_spec {
    risk_forecast_target forecast_target = risk_forecast_target::var;
    double confidence_level = 0.99;
    std::size_t horizon = 1U;
    std::string distribution = "normal";
    risk_tail tail = risk_tail::lower;
    std::string sign_convention = "losses_negative";
    std::size_t window = 252U;
    std::size_t refit_cadence = 1U;
    std::string model_version = "1.0";
};

void validate_risk_model_spec(const risk_model_spec& spec);

std::string_view risk_forecast_target_name(risk_forecast_target target);
std::string_view risk_tail_name(risk_tail tail);

}  // namespace fincore
