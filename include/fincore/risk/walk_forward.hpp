#pragma once

#include <fincore/risk/backtesting.hpp>
#include <fincore/risk/specs.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

enum class risk_distribution { historical, normal };
enum class walk_forward_status { ok, insufficient_data, unsupported };

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
    std::size_t n_observations = 0U;
    risk_backtest_result backtest;
    bool has_backtest;
};

walk_forward_var_result walk_forward_var(
    const dataframe& returns, const risk_model_spec& spec,
    std::string_view column = returns_column);

std::string_view walk_forward_status_name(walk_forward_status status);

}  // namespace fincore
