#pragma once

#include <fincore/simulation/parameters.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace fincore {

enum class bootstrap_statistic {
    mean,
    sample_standard_deviation,
    median,
    sharpe_ratio,
    sortino_ratio,
    minimum,
    maximum,
};

inline constexpr std::string_view bootstrap_statistic_column{"bootstrap"};

struct bootstrap_confidence_interval_result {
    double lower;
    double upper;
};

dataframe bootstrap_distribution(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

bootstrap_confidence_interval_result bootstrap_confidence_interval(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    double alpha = 0.05,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

}  // namespace fincore
