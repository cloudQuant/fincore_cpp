#pragma once

#include <fincore/dataframe.hpp>

#include <string_view>

namespace fincore {

// These functions require a caller-owned Dear ImGui context to be current.
void plot_cumulative_returns(const dataframe& frame, std::string_view column);

void plot_drawdown(const dataframe& frame, std::string_view column);

void plot_rolling_metric(const dataframe& frame, std::string_view column,
                         std::string_view label);

}  // namespace fincore
