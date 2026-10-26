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

/// English month labels used by monthly heatmap columns.
inline constexpr std::string_view month_labels[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/// Monthly return heatmap data in percent.
struct heat_grid {
    std::string title = "Monthly returns";  ///< Chart title.
    std::vector<int> years;                 ///< Row year labels.
    std::vector<std::string> columns;  ///< Month label per column.
    std::vector<double> values;        ///< Years x 12 row-major; NaN when missing.
    std::vector<bool> present;         ///< Same shape as values; missing is not zero.
    std::string unit = "percent";      ///< Value unit.
    double color_min = -10.0;          ///< Lower color-scale bound.
    double color_max = 10.0;           ///< Upper color-scale bound.
    bool color_centered = true;        ///< Whether the color scale is centered.
};

/// Builds a monthly percent heatmap from a return column.
///
/// The frame index must be sorted ascending and `column` must name a double
/// column. Months without a finite compounded value remain NaN with
/// `present == false`; the color range covers observed values or defaults to
/// -10..10 when no value exists. Throws `input_error` for invalid input.
heat_grid build_monthly_heatmap(
    const dataframe& returns,
    std::string_view column = returns_column);

/// One labelled line series in a chart.
struct chart_series {
    std::string label;              ///< Legend label.
    std::vector<timestamp> index;   ///< Observation timestamps.
    std::vector<double> values;     ///< Values aligned with `index`.
    bool benchmark = false;         ///< Whether this is a benchmark series.
    bool dashed = false;            ///< Whether the line is dashed.
};

/// Owning multi-series line chart data.
struct series_chart {
    std::string title;                       ///< Chart title.
    std::string y_label;                     ///< Y-axis label.
    std::vector<chart_series> series;        ///< Series in draw order.
    bool zero_line = false;                  ///< Whether to draw a zero line.
    bool fill_to_zero = false;               ///< Whether to fill down to zero.
    std::size_t window = 0U;                 ///< Recorded rolling window, or 0.
};

/// Description of one series to copy from a frame.
struct series_chart_spec {
    std::string column;             ///< Source double column name.
    std::string label;              ///< Legend label.
    bool benchmark = false;         ///< Whether the series is a benchmark.
};

/// Builds a line chart from named frame columns.
///
/// Each series copies the frame index and the requested double column. A
/// benchmark series is marked `benchmark` and `dashed`. Throws `input_error`
/// when `specs` is empty, a label is empty, or a column is missing or invalid.
series_chart build_series_chart(
    const dataframe& frame, const std::vector<series_chart_spec>& specs,
    std::string title, std::string y_label, bool zero_line = false,
    bool fill_to_zero = false);

/// Builds the cumulative-return chart from a frame column.
///
/// The chart includes a zero line and uses "Cumulative Returns" and
/// "Cumulative Return" as title and axis label. Throws `input_error` when the
/// column is missing or invalid.
series_chart build_returns_chart(
    const dataframe& frame, std::string_view column = "cumulative_returns",
    std::string_view label = "Cumulative Returns");

/// Builds a rolling-Sharpe chart and records the window.
///
/// When `benchmark` is non-null, a dashed benchmark series is read from
/// `benchmark_column`. Throws `input_error` for a missing or invalid column,
/// an empty portfolio label, or an empty benchmark label when a benchmark is
/// supplied.
series_chart build_rolling_sharpe_chart(
    const dataframe& portfolio, std::string_view column,
    std::size_t window,
    const dataframe* benchmark = nullptr,
    std::string_view benchmark_column = "benchmark_sharpe",
    std::string_view label = "Portfolio Sharpe",
    std::string_view benchmark_label = "Benchmark Sharpe");

/// Builds a drawdown chart with fill-to-zero enabled.
///
/// Throws `input_error` when the column is missing or invalid.
series_chart build_drawdown_chart(
    const dataframe& frame, std::string_view column = "drawdown",
    std::string_view label = "Drawdown");

/// Grouped bar chart data in row-major order.
struct bar_chart {
    std::vector<std::string> group_labels;   ///< Group labels on the x axis.
    std::vector<std::string> series_labels;  ///< Series names in draw order.
    std::vector<double> values;  ///< Groups x series values in row-major order.
};

/// Builds grouped bar data from labels and row-major values.
///
/// Throws `input_error` when either label list is empty, a label is empty,
/// `values` does not contain `groups x series` entries, or a value is
/// non-finite.
bar_chart build_bar_chart(std::vector<std::string> group_labels,
                          std::vector<std::string> series_labels,
                          std::vector<double> values);

/// Histogram edges, counts and densities.
struct histogram_bins {
    std::vector<double> edges;   ///< Ascending bin edges; one more than bins.
    std::vector<double> counts;  ///< Finite observation count per bin.
    std::vector<double> density; ///< Counts / (`n_observations` * bin width).
    std::size_t n_observations = 0U;  ///< Number of finite observations.
};

/// Builds histogram bins from finite observations.
///
/// Non-finite values are ignored. When all finite values are equal, the range
/// is widened by 0.5 on each side. Throws `input_error` when `bins` is zero or
/// no finite observation exists.
histogram_bins build_histogram(std::span<const double> values,
                               std::size_t bins = 10U);

/// Pairwise Pearson correlation matrix.
struct correlation_matrix {
    std::vector<std::string> labels;  ///< Asset labels in matrix order.
    std::vector<double> values;  ///< Labels x labels row-major; NaN when undefined.
    std::vector<bool> present;   ///< Whether each matrix cell is defined.
    std::string unit = "correlation";  ///< Value unit.
};

/// Builds a pairwise correlation matrix from double columns.
///
/// Pairs with fewer than two finite observations or zero variance remain NaN
/// with `present == false`. Throws `input_error` when no double column exists
/// or a column name is empty.
correlation_matrix build_correlation_matrix(const dataframe& frame);

/// Chart-ready efficient frontier points and marker coordinates.
struct frontier_chart_data {
    std::vector<double> volatilities;          ///< Frontier volatility points.
    std::vector<double> returns;               ///< Frontier return points.
    std::vector<double> sharpe;                ///< Frontier Sharpe points.
    double min_variance_volatility = 0.0;      ///< Minimum-variance volatility.
    double min_variance_return = 0.0;          ///< Minimum-variance return.
    double max_sharpe_volatility = 0.0;        ///< Maximum-Sharpe volatility.
    double max_sharpe_return = 0.0;            ///< Maximum-Sharpe return.
    std::string x_label = "Volatility";        ///< X-axis label.
    std::string y_label = "Return";            ///< Y-axis label.
};

/// Builds chart data from an efficient frontier result.
///
/// Throws `input_error` when point vectors differ in length or any value or
/// marker coordinate is non-finite.
frontier_chart_data build_frontier_chart(
    const efficient_frontier_result& frontier);

}  // namespace fincore
