#pragma once

#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>

#include <string_view>

namespace fincore {

inline constexpr std::string_view returns_column{"returns"};
inline constexpr double daily_annualization = 252.0;
inline constexpr std::string_view return_aggregation_year_column{"year"};
inline constexpr std::string_view return_aggregation_period_column{"period"};

enum class return_aggregation_period {
    weekly,
    monthly,
    quarterly,
    yearly,
};

dataframe simple_returns(const dataframe& prices, std::string_view price_column);

dataframe cumulative_returns(const dataframe& returns,
                             std::string_view column = returns_column);

dataframe cumulative_returns(const dataframe& returns,
                             double starting_value,
                             std::string_view column = returns_column);

double cumulative_return(const dataframe& returns,
                         std::string_view column = returns_column);

double cumulative_return(const dataframe& returns,
                         double starting_value,
                         std::string_view column = returns_column);

dataframe normalize(const dataframe& values,
                    double starting_value = 1.0,
                    std::string_view column = returns_column);

dataframe aggregate_returns(
    const dataframe& returns,
    return_aggregation_period period = return_aggregation_period::monthly,
    std::string_view column = returns_column);

double annual_return(const dataframe& returns,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);

double annual_volatility(const dataframe& returns,
                         double annualization = daily_annualization,
                         std::string_view column = returns_column);

}  // namespace fincore
