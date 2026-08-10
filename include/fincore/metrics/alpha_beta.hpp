#pragma once

#include <fincore/metrics/returns.hpp>

#include <string_view>
#include <utility>

namespace fincore {

inline constexpr std::string_view benchmark_column{"benchmark"};

double beta(const dataframe& returns,
            const dataframe& benchmark,
            double risk_free = 0.0,
            std::string_view returns_column_name = returns_column,
            std::string_view benchmark_column_name = benchmark_column);

double alpha(const dataframe& returns,
             const dataframe& benchmark,
             double risk_free = 0.0,
             double annualization = daily_annualization,
             std::string_view returns_column_name = returns_column,
             std::string_view benchmark_column_name = benchmark_column);

std::pair<double, double> alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

std::pair<double, double> up_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

std::pair<double, double> down_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
