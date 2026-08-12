#pragma once

#include <fincore/simulation/parameters.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace fincore {

inline constexpr std::string_view gbm_path_column_prefix{"path_"};
inline constexpr std::string_view latin_hypercube_dimension_column_prefix{
    "lhs_dimension_"};
inline constexpr std::string_view antithetic_path_column_prefix{
    "antithetic_"};

dataframe latin_hypercube_sampling(
    std::size_t sample_count,
    std::size_t dimension_count,
    std::optional<std::uint64_t> seed = std::nullopt);

dataframe geometric_brownian_motion(
    double initial_value,
    double annualized_drift,
    double annualized_volatility,
    double total_years,
    double time_step,
    std::size_t path_count,
    std::optional<std::uint64_t> seed = std::nullopt);

dataframe gbm_from_returns(
    const dataframe& returns,
    std::size_t horizon = 252U,
    std::size_t path_count = 1000U,
    std::size_t periods_per_year = 252U,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

dataframe antithetic_variates(const dataframe& paths);

}  // namespace fincore
