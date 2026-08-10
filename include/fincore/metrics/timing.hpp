#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <string_view>

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

}  // namespace fincore
