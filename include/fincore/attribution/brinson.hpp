#pragma once

#include <fincore/dataframe.hpp>

#include <span>
#include <string_view>

namespace fincore {

inline constexpr std::string_view brinson_allocation_column{"allocation"};
inline constexpr std::string_view brinson_selection_column{"selection"};
inline constexpr std::string_view brinson_interaction_column{"interaction"};
inline constexpr std::string_view brinson_total_column{"total"};
inline constexpr std::string_view brinson_portfolio_return_column{
    "portfolio_return"};
inline constexpr std::string_view brinson_benchmark_return_column{
    "benchmark_return"};

struct brinson_period_result {
    double allocation;
    double selection;
    double interaction;
    double total;
    double portfolio_return;
    double benchmark_return;
};

struct brinson_cumulative_result {
    double allocation;
    double selection;
    double interaction;
    double total;
    double portfolio_cumulative;
    double benchmark_cumulative;
};

brinson_period_result brinson_attribution(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

dataframe brinson_results(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

brinson_cumulative_result brinson_cumulative(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

}  // namespace fincore
