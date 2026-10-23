#pragma once

#include <fincore/risk/walk_forward.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr std::string_view basel_reference_disclosure =
    "Basel traffic-light and VaR backtesting fields are a reference "
    "implementation; they are not regulatory approval or compliance "
    "certification.";
inline constexpr int risk_validation_report_schema_version = 1;

struct risk_forecast_event {
    timestamp date;
    double forecast;
    double realized;
    bool exception;
    bool refit;
};

struct risk_refit_event {
    timestamp date;
    double forecast;
    double mean;
    double standard_deviation;
    std::size_t n_observations;
};

struct risk_report_diagnostics {
    std::string method;
    std::string quantile_method;
    std::size_t window = 0U;
    std::size_t refit_cadence = 0U;
    std::size_t forecast_count = 0U;
    std::string backtest_status;
    std::string reason;
    std::size_t n_observations = 0U;
};

struct risk_validation_report {
    walk_forward_status status = walk_forward_status::unsupported;
    std::string inputs_digest;
    risk_model_spec specification;
    std::vector<risk_forecast_event> forecast_events;
    std::vector<risk_refit_event> refits;
    risk_report_diagnostics diagnostics;
    bool has_backtest = false;
    risk_backtest_result backtest{};
    std::string traffic_light_zone;
    std::optional<std::string> timestamp_index_name;
    std::optional<std::string> timestamp_timezone;
    std::string disclosure{basel_reference_disclosure};
    int schema_version = risk_validation_report_schema_version;

    std::string to_json() const;
};

risk_validation_report build_risk_validation_report(
    const walk_forward_var_result& result,
    std::optional<std::string> timestamp_index_name = std::nullopt);

}  // namespace fincore
