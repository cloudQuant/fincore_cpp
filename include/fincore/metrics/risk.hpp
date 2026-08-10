#pragma once

#include <fincore/metrics/returns.hpp>

#include <string_view>

namespace fincore {

double historical_value_at_risk(const dataframe& returns,
                                double cutoff = 0.05,
                                std::string_view column = returns_column);

double conditional_value_at_risk(const dataframe& returns,
                                 double cutoff = 0.05,
                                 std::string_view column = returns_column);

}  // namespace fincore
