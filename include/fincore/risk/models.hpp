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

enum class risk_forecast_method { historical, evt, garch };
enum class risk_estimate_status { ok, insufficient_data, failed };

inline constexpr std::string_view losses_negative_sign_convention =
    "losses_negative";

struct risk_forecast_config {
    risk_forecast_method method = risk_forecast_method::historical;
    double confidence_level = 0.95;
    std::size_t horizon = 1U;
    garch_model garch_kernel = garch_model::garch;
    evt_model evt_kernel = evt_model::gpd;
    evt_tail tail = evt_tail::lower;
    std::optional<double> threshold;
    std::optional<std::size_t> block_size;
};

struct risk_estimate {
    risk_forecast_method method = risk_forecast_method::historical;
    double confidence_level = 0.95;
    std::size_t horizon = 1U;
    std::string sign_convention{losses_negative_sign_convention};
    double estimate = 0.0;
    timestamp forecast_timestamp = 0;
    bool has_forecast_timestamp = false;
    std::string inputs_digest;
    bool has_forecast = false;
    std::vector<timestamp> forecast_index;
    std::vector<double> forecast_values;
    std::map<std::string, double> diagnostics;
    std::string kernel;
    std::string note;
    risk_estimate_status status = risk_estimate_status::ok;
};

risk_estimate forecast_var(
    const dataframe& returns,
    const risk_forecast_config& config = {},
    std::string_view column = returns_column);

risk_estimate forecast_es(
    const dataframe& returns,
    const risk_forecast_config& config = {},
    std::string_view column = returns_column);

}  // namespace fincore
