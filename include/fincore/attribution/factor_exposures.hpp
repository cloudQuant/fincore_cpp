#pragma once

#include <fincore/attribution/factor_regression.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace fincore {

dataframe rolling_factor_exposures(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::optional<std::size_t> rolling_window = std::nullopt,
    std::string_view returns_column_name = returns_column);

}  // namespace fincore
