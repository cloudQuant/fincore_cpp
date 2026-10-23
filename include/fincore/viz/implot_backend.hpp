#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/viz/chart_data.hpp>

#include <string_view>

namespace fincore {

// These functions require caller-owned Dear ImGui and ImPlot contexts to be current.
void plot_cumulative_returns(const dataframe& frame, std::string_view column);

void plot_drawdown(const dataframe& frame, std::string_view column);

void plot_rolling_metric(const dataframe& frame, std::string_view column,
                         std::string_view label);

void plot_monthly_heatmap(const heat_grid& grid);

void plot_series_chart(const series_chart& chart);

void plot_drawdown_chart(const series_chart& chart);

void plot_bar_chart(const bar_chart& chart);

void plot_histogram(const histogram_bins& bins);

void plot_correlation_matrix(const correlation_matrix& matrix);

void plot_frontier_chart(const frontier_chart_data& chart);

}  // namespace fincore
