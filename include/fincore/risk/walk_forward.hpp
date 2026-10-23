#pragma once

#include <fincore/risk/backtesting.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

enum class risk_distribution { historical, normal };
enum class risk_forecast_target { var, es, pair };
enum class risk_tail { lower, upper };
enum class walk_forward_status { ok, insufficient_data, unsupported };

struct risk_model_spec {
    risk_forecast_target forecast_target = risk_forecast_target::var;
    double confidence_level = 0.99;
    std::string distribution = "normal";
    risk_tail tail = risk_tail::lower;
    std::string sign_convention = "losses_negative";
    std::size_t window = 252U;
    std::size_t refit_cadence = 1U;
    std::string model_version = "1.0";
};

struct refit_parameters {
    timestamp date;
    double forecast;
    double mean;
    double standard_deviation;
    std::size_t n_observations;
};

struct walk_forward_var_result {
    risk_model_spec spec;
    std::vector<timestamp> index;
    std::vector<double> forecast;
    std::vector<double> realized;
    std::vector<timestamp> refit_timestamps;
    std::vector<refit_parameters> fits;
    std::string inputs_digest;
    walk_forward_status status;
    std::string unsupported_reason;
    risk_backtest_result backtest;
    bool has_backtest;
};

walk_forward_var_result walk_forward_var(
    const dataframe& returns, const risk_model_spec& spec,
    std::string_view column = returns_column);

}  // namespace fincore
