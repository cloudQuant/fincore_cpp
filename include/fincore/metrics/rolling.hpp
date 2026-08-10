#pragma once

#include <fincore/metrics/basic.hpp>

#include <cstddef>
#include <optional>
#include <string_view>

namespace fincore {

dataframe roll_sharpe_ratio(const dataframe& returns,
                            std::size_t window = 252U,
                            double risk_free = 0.0,
                            frequency period = frequency::daily,
                            std::optional<double> annualization = std::nullopt,
                            std::string_view column = returns_column);

dataframe roll_max_drawdown(const dataframe& returns,
                            std::size_t window = 252U,
                            std::string_view column = returns_column);

dataframe rolling_volatility(const dataframe& returns,
                             std::size_t window,
                             frequency period = frequency::daily,
                             std::optional<double> annualization = std::nullopt,
                             std::string_view column = returns_column);

dataframe rolling_sharpe(const dataframe& returns,
                         std::size_t window,
                         frequency period = frequency::daily,
                         std::optional<double> annualization = std::nullopt,
                         std::string_view column = returns_column);

}  // namespace fincore
