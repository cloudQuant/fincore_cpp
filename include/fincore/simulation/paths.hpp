#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace fincore {

inline constexpr std::string_view gbm_path_column_prefix{"path_"};

dataframe geometric_brownian_motion(
    double initial_value,
    double annualized_drift,
    double annualized_volatility,
    double total_years,
    double time_step,
    std::size_t path_count,
    std::optional<std::uint64_t> seed = std::nullopt);

}  // namespace fincore
