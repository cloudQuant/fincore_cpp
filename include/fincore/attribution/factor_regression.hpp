#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/metrics/returns.hpp>

#include <span>
#include <string_view>

namespace fincore {

inline constexpr std::string_view factor_regression_factor_column{"factor"};
inline constexpr std::string_view factor_regression_coefficient_column{
    "coefficient"};
inline constexpr std::string_view factor_regression_fitted_column{
    "fitted_returns"};
inline constexpr std::string_view factor_regression_residual_column{
    "residuals"};
inline constexpr std::string_view factor_regression_alpha_name{"alpha"};

struct factor_regression_result {
    double alpha;
    double r_squared;
    dataframe coefficients;
    dataframe fitted_returns;
    dataframe residuals;
};

factor_regression_result fit_factor_regression(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::string_view returns_column_name = returns_column);

}  // namespace fincore
