#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/viz/chart_data.hpp>

#include <string_view>

namespace fincore {

struct quantile_cumulative_chart;
struct events_distribution_chart;
struct qq_chart;
struct spread_time_series_chart;
struct monthly_ic_heatmap_chart;
struct violin_chart;

// These functions require caller-owned Dear ImGui and ImPlot contexts to be current.
/// Plots a cumulative-returns column with a date axis.
void plot_cumulative_returns(const dataframe& frame, std::string_view column);

/// Plots a drawdown column with a date axis.
void plot_drawdown(const dataframe& frame, std::string_view column);

/// Plots a rolling metric column using `label` as title and legend.
void plot_rolling_metric(const dataframe& frame, std::string_view column,
                         std::string_view label);

/// Plots a monthly heatmap grid; empty grids are ignored.
void plot_monthly_heatmap(const heat_grid& grid);

/// Plots every series of a line chart, including an optional zero line.
void plot_series_chart(const series_chart& chart);

/// Plots drawdown series as shaded areas with overlaid lines.
void plot_drawdown_chart(const series_chart& chart);

/// Plots grouped bars from bar chart data.
void plot_bar_chart(const bar_chart& chart);

/// Plots histogram counts over bin centers.
void plot_histogram(const histogram_bins& bins);

/// Plots a correlation heatmap with labelled axes.
void plot_correlation_matrix(const correlation_matrix& matrix);

/// Plots the efficient frontier curve and its marker points.
void plot_frontier_chart(const frontier_chart_data& chart);

/// Plots cumulative quantile series by date.
void plot_quantile_cumulative_chart(const quantile_cumulative_chart& chart);
/// Plots event counts as bars over time buckets.
void plot_events_distribution_chart(const events_distribution_chart& chart);
/// Plots observed versus theoretical quantiles with a reference line.
void plot_qq_chart(const qq_chart& chart);
/// Plots spread, rolling mean, band and zero line.
void plot_spread_time_series(const spread_time_series_chart& chart);
/// Plots a monthly IC heatmap with a fixed -0.2 to 0.2 color range.
void plot_monthly_ic_heatmap(const monthly_ic_heatmap_chart& chart);
/// Plots quantile density shapes and medians per group.
void plot_violin_chart(const violin_chart& chart);

}  // namespace fincore
