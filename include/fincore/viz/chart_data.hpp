#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/optimization/frontier.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr std::string_view month_labels[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

struct heat_grid {
    std::vector<int> years;
    std::vector<std::string> columns;  // twelve month labels
    std::vector<double> values;        // years x 12, row-major; NaN when missing
    std::vector<bool> present;         // same shape; missing cell is not zero
    std::string unit = "percent";
    double color_min = -10.0;
    double color_max = 10.0;
    bool color_centered = true;
};

heat_grid build_monthly_heatmap(
    const dataframe& returns,
    std::string_view column = returns_column);

struct chart_series {
    std::string label;
    std::vector<timestamp> index;
    std::vector<double> values;
    bool benchmark = false;
    bool dashed = false;
};

struct series_chart {
    std::string title;
    std::string y_label;
    std::vector<chart_series> series;
    bool zero_line = false;
    bool fill_to_zero = false;
    std::size_t window = 0U;
};

struct series_chart_spec {
    std::string column;
    std::string label;
    bool benchmark = false;
};

series_chart build_series_chart(
    const dataframe& frame, const std::vector<series_chart_spec>& specs,
    std::string title, std::string y_label, bool zero_line = false,
    bool fill_to_zero = false);

series_chart build_returns_chart(
    const dataframe& frame, std::string_view column = "cumulative_returns",
    std::string_view label = "Cumulative Returns");

series_chart build_rolling_sharpe_chart(
    const dataframe& portfolio, std::string_view column,
    std::size_t window,
    const dataframe* benchmark = nullptr,
    std::string_view benchmark_column = "benchmark_sharpe",
    std::string_view label = "Portfolio Sharpe",
    std::string_view benchmark_label = "Benchmark Sharpe");

series_chart build_drawdown_chart(
    const dataframe& frame, std::string_view column = "drawdown",
    std::string_view label = "Drawdown");

struct bar_chart {
    std::vector<std::string> group_labels;
    std::vector<std::string> series_labels;
    std::vector<double> values;  // groups x series, row-major
};

bar_chart build_bar_chart(std::vector<std::string> group_labels,
                          std::vector<std::string> series_labels,
                          std::vector<double> values);

struct histogram_bins {
    std::vector<double> edges;   // bins + 1
    std::vector<double> counts;  // bins
    std::vector<double> density; // counts / (n * width)
    std::size_t n_observations = 0U;
};

histogram_bins build_histogram(std::span<const double> values,
                               std::size_t bins = 10U);

struct correlation_matrix {
    std::vector<std::string> labels;
    std::vector<double> values;  // labels x labels, row-major; NaN when undefined
    std::vector<bool> present;
    std::string unit = "correlation";
};

correlation_matrix build_correlation_matrix(const dataframe& frame);

struct frontier_chart_data {
    std::vector<double> volatilities;
    std::vector<double> returns;
    std::vector<double> sharpe;
    double min_variance_volatility = 0.0;
    double min_variance_return = 0.0;
    double max_sharpe_volatility = 0.0;
    double max_sharpe_return = 0.0;
    std::string x_label = "Volatility";
    std::string y_label = "Return";
};

frontier_chart_data build_frontier_chart(
    const efficient_frontier_result& frontier);

}  // namespace fincore
