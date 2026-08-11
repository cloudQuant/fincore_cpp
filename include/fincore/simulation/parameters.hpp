#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <string_view>

namespace fincore {

struct simulation_parameters {
    double drift;
    double volatility;
};

simulation_parameters estimate_simulation_parameters(
    const dataframe& returns,
    std::size_t periods_per_year = 252,
    std::string_view column = returns_column);

}  // namespace fincore
