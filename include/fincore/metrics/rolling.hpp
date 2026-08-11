#pragma once

#include <fincore/metrics/basic.hpp>
#include <fincore/metrics/alpha_beta.hpp>

#include <cstddef>
#include <optional>
#include <span>
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

dataframe roll_alpha(const dataframe& returns, const dataframe& benchmark,
                     std::size_t window = 252U, double risk_free = 0.0,
                     frequency period = frequency::daily,
                     std::optional<double> annualization = std::nullopt,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);

dataframe roll_beta(const dataframe& returns, const dataframe& benchmark,
                    std::size_t window = 252U, double risk_free = 0.0,
                    frequency period = frequency::daily,
                    std::optional<double> annualization = std::nullopt,
                    std::string_view returns_column_name = returns_column,
                    std::string_view benchmark_column_name = benchmark_column);

dataframe roll_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    std::size_t window = 252U,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

dataframe rolling_beta(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column,
    std::string_view factor_column_name = benchmark_column);

dataframe rolling_beta(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column);

dataframe rolling_regression(
    const dataframe& returns,
    const dataframe& benchmark,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

dataframe roll_up_capture(const dataframe& returns, const dataframe& benchmark,
                          std::size_t window = 252U,
                          frequency period = frequency::daily,
                          std::string_view returns_column_name = returns_column,
                          std::string_view benchmark_column_name = benchmark_column);

dataframe roll_down_capture(const dataframe& returns, const dataframe& benchmark,
                            std::size_t window = 252U,
                            frequency period = frequency::daily,
                            std::string_view returns_column_name = returns_column,
                            std::string_view benchmark_column_name = benchmark_column);

dataframe roll_up_down_capture(
    const dataframe& returns, const dataframe& benchmark,
    std::size_t window = 252U, frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
