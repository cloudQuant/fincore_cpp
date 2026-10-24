#pragma once

#include <fincore/simulation/parameters.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

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

struct bootstrap_summary_statistic {
    double value;
    double standard_error;
    bootstrap_confidence_interval_result confidence_interval;
};

struct bootstrap_summary_result {
    bootstrap_summary_statistic mean;
    bootstrap_summary_statistic sample_standard_deviation;
    bootstrap_summary_statistic sharpe_ratio;
    bootstrap_summary_statistic sortino_ratio;
};

dataframe bootstrap_distribution(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

// Draw indices address the NaN-cleaned positional input population. Every row
// must have the same length as that population; an empty draw span returns an
// empty distribution after validating the input and statistic.
dataframe bootstrap_distribution(
    const dataframe& returns,
    std::span<const std::vector<std::size_t>> draw_indices,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::string_view column = returns_column);

bootstrap_confidence_interval_result bootstrap_confidence_interval(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    double alpha = 0.05,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

bootstrap_summary_result bootstrap_summary(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    double alpha = 0.05,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

}  // namespace fincore
