#pragma once

#include <fincore/metrics/returns.hpp>

#include <optional>
#include <string_view>

namespace fincore {

struct daily_consecutive_statistics {
    double max_consecutive_up_days;
    double max_consecutive_down_days;
    double max_consecutive_gain;
    double max_consecutive_loss;
};

daily_consecutive_statistics daily_consecutive_stats(
    const dataframe& returns,
    std::string_view column = returns_column);

double max_consecutive_up_days(const dataframe& returns,
                               std::string_view column = returns_column);
double max_consecutive_down_days(const dataframe& returns,
                                 std::string_view column = returns_column);
double max_consecutive_gain(const dataframe& returns,
                            std::string_view column = returns_column);
double max_consecutive_loss(const dataframe& returns,
                            std::string_view column = returns_column);

double max_consecutive_up_weeks(const dataframe& returns,
                                std::string_view column = returns_column);
double max_consecutive_down_weeks(const dataframe& returns,
                                  std::string_view column = returns_column);
double max_consecutive_up_months(const dataframe& returns,
                                 std::string_view column = returns_column);
double max_consecutive_down_months(const dataframe& returns,
                                   std::string_view column = returns_column);
double max_single_day_gain(const dataframe& returns,
                           std::string_view column = returns_column);
double max_single_day_loss(const dataframe& returns,
                           std::string_view column = returns_column);

std::optional<timestamp> max_single_day_gain_date(
    const dataframe& returns,
    std::string_view column = returns_column);
std::optional<timestamp> max_single_day_loss_date(
    const dataframe& returns,
    std::string_view column = returns_column);
std::optional<timestamp> max_consecutive_up_start_date(
    const dataframe& returns,
    std::string_view column = returns_column);
std::optional<timestamp> max_consecutive_up_end_date(
    const dataframe& returns,
    std::string_view column = returns_column);
std::optional<timestamp> max_consecutive_down_start_date(
    const dataframe& returns,
    std::string_view column = returns_column);
std::optional<timestamp> max_consecutive_down_end_date(
    const dataframe& returns,
    std::string_view column = returns_column);

std::map<std::string, double> consecutive_stats(
    const dataframe& returns,
    std::string_view column = returns_column);

}  // namespace fincore
