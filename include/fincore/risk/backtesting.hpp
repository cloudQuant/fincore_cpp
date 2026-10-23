#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>

namespace fincore {

enum class risk_backtest_status { pass, fail, inconclusive, experimental };
enum class risk_backtest_method { var, es };

struct risk_backtest_result {
    risk_backtest_method method;
    double confidence_level;
    std::size_t observations;
    std::size_t exceptions;
    double expected_exceptions;
    double exception_rate;
    double kupiec_lr;
    double kupiec_pvalue;
    double christoffersen_lr;
    double christoffersen_pvalue;
    std::string inputs_digest;
    std::map<std::string, double> diagnostics;
    risk_backtest_status status;
};

double kupiec_lr(std::size_t observations, std::size_t exceptions,
                 double confidence_level);

double christoffersen_lr(std::span<const int> exceptions);

risk_backtest_result backtest_var(
    const dataframe& forecast, const dataframe& realized,
    double confidence_level = 0.99, double significance = 0.05,
    std::string_view forecast_column = "forecast",
    std::string_view realized_column = "realized");

risk_backtest_result backtest_es(
    const dataframe& forecast, const dataframe& realized,
    double confidence_level = 0.975, std::size_t n_bootstrap = 1000U,
    std::uint64_t seed = 20260817U,
    std::string_view forecast_column = "forecast",
    std::string_view realized_column = "realized");

}  // namespace fincore
