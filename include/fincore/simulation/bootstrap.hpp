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

dataframe bootstrap_distribution(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

}  // namespace fincore
