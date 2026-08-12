#pragma once

#include <fincore/dataframe.hpp>

#include <string_view>

namespace fincore {

inline constexpr std::string_view bayesian_cone_percentile_1_column{"percentile_1"};
inline constexpr std::string_view bayesian_cone_percentile_5_column{"percentile_5"};
inline constexpr std::string_view bayesian_cone_percentile_25_column{"percentile_25"};
inline constexpr std::string_view bayesian_cone_percentile_50_column{"percentile_50"};
inline constexpr std::string_view bayesian_cone_percentile_75_column{"percentile_75"};
inline constexpr std::string_view bayesian_cone_percentile_95_column{"percentile_95"};
inline constexpr std::string_view bayesian_cone_percentile_99_column{"percentile_99"};

[[nodiscard]] dataframe compute_bayes_cone(
    const dataframe& predicted_returns,
    double starting_value = 1.0);

}  // namespace fincore
