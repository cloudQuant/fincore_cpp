#pragma once

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/basic.hpp>

#include <optional>
#include <string_view>

namespace fincore {

dataframe annual_return_by_year(
    const dataframe& returns,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

dataframe annual_volatility_by_year(
    const dataframe& returns,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

dataframe sharpe_ratio_by_year(
    const dataframe& returns,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

dataframe max_drawdown_by_year(
    const dataframe& returns,
    std::string_view column = returns_column);

double annual_active_return(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

dataframe annual_active_return_by_year(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

dataframe information_ratio_by_year(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
