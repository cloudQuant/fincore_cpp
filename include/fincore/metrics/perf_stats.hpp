#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <map>
#include <string>
#include <string_view>

namespace fincore {

std::map<std::string, double> perf_stats(
    const dataframe& returns,
    frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column);

std::map<std::string, double> perf_stats(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
