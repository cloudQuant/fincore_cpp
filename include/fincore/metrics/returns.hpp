#pragma once

#include <fincore/dataframe.hpp>

#include <string_view>

namespace fincore {

inline constexpr std::string_view returns_column{"returns"};
inline constexpr double daily_annualization = 252.0;

dataframe simple_returns(const dataframe& prices, std::string_view price_column);

dataframe cumulative_returns(const dataframe& returns,
                             std::string_view column = returns_column);

double cumulative_return(const dataframe& returns,
                         std::string_view column = returns_column);

double annual_return(const dataframe& returns,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);

double annual_volatility(const dataframe& returns,
                         double annualization = daily_annualization,
                         std::string_view column = returns_column);

}  // namespace fincore
