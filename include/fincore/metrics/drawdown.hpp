#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
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

struct drawdown_table_row {
    double net_drawdown_percent = std::numeric_limits<double>::quiet_NaN();
    std::optional<timestamp> peak;
    std::optional<timestamp> valley;
    std::optional<timestamp> recovery;
    std::optional<std::size_t> duration_business_days;
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

// Peak/valley/recovery from a precomputed underwater series. The source
// helper returns (NaT, NaT, NaT) when the series never goes below zero; the
// native form returns nullopt for that case and for empty/non-finite data.
std::optional<drawdown_window> max_drawdown_underwater(
    const dataframe& underwater,
    std::string_view column = "drawdown");

std::vector<drawdown_window> get_top_drawdowns(
    const dataframe& returns,
    std::size_t top = 10,
    std::string_view column = returns_column);

std::vector<drawdown_table_row> gen_drawdown_table(
    const dataframe& returns,
    std::size_t top = 10,
    std::string_view column = returns_column);

std::pair<std::optional<timestamp>, std::optional<timestamp>>
max_drawdown_period(const dataframe& returns,
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
