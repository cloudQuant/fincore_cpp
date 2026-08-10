#pragma once

#include <fincore/metrics/drawdown.hpp>

#include <string_view>

namespace fincore {

double sharpe_ratio(const dataframe& returns,
                    double risk_free = 0.0,
                    double annualization = daily_annualization,
                    std::string_view column = returns_column);

double sortino_ratio(const dataframe& returns,
                     double required_return = 0.0,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);

double calmar_ratio(const dataframe& returns,
                    double risk_free = 0.0,
                    double annualization = daily_annualization,
                    std::string_view column = returns_column);

}  // namespace fincore
