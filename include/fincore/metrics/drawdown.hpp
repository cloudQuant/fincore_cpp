#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace fincore {

struct drawdown_period {
    timestamp start;
    timestamp valley;
    std::optional<timestamp> recovery;
    double value;
    std::size_t duration_periods;
    std::optional<std::size_t> recovery_duration_periods;
};

struct drawdown_window {
    timestamp peak;
    timestamp valley;
    std::optional<timestamp> recovery;
};

dataframe drawdown_series(const dataframe& returns,
                          std::string_view column = returns_column);

double max_drawdown(const dataframe& returns,
                    std::string_view column = returns_column);

std::vector<double> get_all_drawdowns(
    const dataframe& returns,
    std::string_view column = returns_column);

std::vector<drawdown_period> get_all_drawdowns_detailed(
    const dataframe& returns,
    std::string_view column = returns_column);

std::optional<drawdown_window> max_drawdown_window(
    const dataframe& returns,
    std::string_view column = returns_column);

double max_drawdown_days(const dataframe& returns,
                         std::string_view column = returns_column);
double max_drawdown_weeks(const dataframe& returns,
                          std::string_view column = returns_column);
double max_drawdown_months(const dataframe& returns,
                           std::string_view column = returns_column);
double max_drawdown_recovery_days(
    const dataframe& returns,
    std::string_view column = returns_column);
double max_drawdown_recovery_weeks(
    const dataframe& returns,
    std::string_view column = returns_column);
double max_drawdown_recovery_months(
    const dataframe& returns,
    std::string_view column = returns_column);
double second_max_drawdown(const dataframe& returns,
                           std::string_view column = returns_column);
double third_max_drawdown(const dataframe& returns,
                          std::string_view column = returns_column);
double second_max_drawdown_days(
    const dataframe& returns,
    std::string_view column = returns_column);
double second_max_drawdown_recovery_days(
    const dataframe& returns,
    std::string_view column = returns_column);
double third_max_drawdown_days(
    const dataframe& returns,
    std::string_view column = returns_column);
double third_max_drawdown_recovery_days(
    const dataframe& returns,
    std::string_view column = returns_column);

}  // namespace fincore
