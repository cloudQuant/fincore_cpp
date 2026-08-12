#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <cstddef>
#include <string_view>

namespace fincore {

double skewness(const dataframe& returns,
                std::string_view column = returns_column);

double kurtosis(const dataframe& returns,
                std::string_view column = returns_column);

double serial_correlation(const dataframe& returns,
                          std::ptrdiff_t lag = 1,
                          std::string_view column = returns_column);

double win_rate(const dataframe& returns,
                std::string_view column = returns_column);

double loss_rate(const dataframe& returns,
                 std::string_view column = returns_column);

double safe_correlation(const dataframe& returns,
                        const dataframe& benchmark,
                        std::string_view returns_column_name = returns_column,
                        std::string_view benchmark_column_name = benchmark_column);

double r_cubed(const dataframe& returns,
               const dataframe& benchmark,
               std::string_view returns_column_name = returns_column,
               std::string_view benchmark_column_name = benchmark_column);

double relative_win_rate(const dataframe& returns,
                         const dataframe& benchmark,
                         std::string_view returns_column_name = returns_column,
                         std::string_view benchmark_column_name = benchmark_column);

double capm_r_squared(const dataframe& returns,
                      const dataframe& benchmark,
                      std::string_view returns_column_name = returns_column,
                      std::string_view benchmark_column_name = benchmark_column);

double tracking_difference(const dataframe& returns,
                           const dataframe& benchmark,
                           std::string_view returns_column_name = returns_column,
                           std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
