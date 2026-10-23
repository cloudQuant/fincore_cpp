#pragma once

#include <fincore/metrics/returns.hpp>

#include <string_view>
#include <utility>

namespace fincore {

double standard_error_of_mean(const dataframe& values,
                              std::string_view column = returns_column);

double sharpe_standard_error(const dataframe& returns,
                             double risk_free = 0.0,
                             std::string_view column = returns_column);

std::pair<double, double> sharpe_confidence_interval(
    const dataframe& returns,
    double risk_free = 0.0,
    double z = 1.96,
    std::string_view column = returns_column);

}  // namespace fincore
