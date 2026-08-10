#pragma once

#include <fincore/metrics/returns.hpp>

#include <string_view>

namespace fincore {

dataframe drawdown_series(const dataframe& returns,
                          std::string_view column = returns_column);

double max_drawdown(const dataframe& returns,
                    std::string_view column = returns_column);

}  // namespace fincore
