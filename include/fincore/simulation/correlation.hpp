#pragma once

#include <fincore/dataframe.hpp>

#include <span>
#include <string_view>

namespace fincore {

dataframe generate_correlation_breakdown(
    std::span<const std::string_view> asset_names,
    double target_correlation = 1.0);

}  // namespace fincore
