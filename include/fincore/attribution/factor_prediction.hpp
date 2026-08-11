#pragma once

#include <fincore/attribution/factor_regression.hpp>

#include <span>
#include <string_view>

namespace fincore {

inline constexpr std::string_view factor_prediction_column{"predicted_returns"};

dataframe factor_regression_predictions(
    const factor_regression_result& regression,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names);

}  // namespace fincore
