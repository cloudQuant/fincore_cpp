#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace fincore {

double treynor_mazuy_timing(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

double henriksson_merton_timing(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

double market_timing_return(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

double cornell_timing(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

struct named_returns {
    std::string name;
    dataframe returns;
};

std::vector<named_returns> extract_interesting_date_ranges(
    const dataframe& returns,
    std::string_view column = returns_column);

}  // namespace fincore
