#include <fincore/viz/implot_backend.hpp>

#include <implot.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace fincore {
namespace {

constexpr double nanoseconds_per_day = 86'400'000'000'000.0;

void plot_series(const dataframe& frame, std::string_view column,
                 std::string_view title, std::string_view label) {
    const auto values = require_double_column(frame, column);
    if (values.empty()) {
        return;
    }

    std::vector<double> x;
    x.reserve(frame.get_index().size());
    for (const timestamp value : frame.get_index()) {
        x.push_back(static_cast<double>(value) / nanoseconds_per_day);
    }

    const std::string plot_title{title};
    const std::string series_label{label};
    if (ImPlot::BeginPlot(plot_title.data())) {
        ImPlot::PlotLine(series_label.data(), x.data(), values.data(),
                         static_cast<int>(values.size()));
        ImPlot::EndPlot();
    }
}

std::vector<double> day_axis(const std::vector<timestamp>& index) {
    std::vector<double> values;
    values.reserve(index.size());
    for (const timestamp value : index) {
        values.push_back(static_cast<double>(value) / nanoseconds_per_day);
    }
    return values;
}

ImVec4 benchmark_color() { return ImVec4(0.45F, 0.45F, 0.45F, 1.0F); }

}  // namespace

void plot_cumulative_returns(const dataframe& frame, std::string_view column) {
    plot_series(frame, column, "Cumulative Returns", "Cumulative Returns");
}

void plot_drawdown(const dataframe& frame, std::string_view column) {
    plot_series(frame, column, "Drawdown", "Drawdown");
}

void plot_rolling_metric(const dataframe& frame, std::string_view column,
                         std::string_view label) {
    plot_series(frame, column, label, label);
}

void plot_monthly_heatmap(const heat_grid& grid) {
    if (grid.years.empty() || grid.values.empty()) {
        return;
    }
    if (!ImPlot::BeginPlot("Monthly Returns (%)")) {
        return;
    }
    ImPlot::SetupAxes(nullptr, nullptr);
    std::vector<const char*> month_pointers;
    month_pointers.reserve(grid.columns.size());
    for (const auto& label : grid.columns) {
        month_pointers.push_back(label.c_str());
    }
    ImPlot::SetupAxisTicks(ImAxis_X1, 0.5, 11.5,
                           static_cast<int>(grid.columns.size()),
                           month_pointers.data());
    std::vector<std::string> year_text;
    year_text.reserve(grid.years.size());
    std::vector<const char*> year_pointers;
    year_pointers.reserve(grid.years.size());
    for (const int year : grid.years) {
        year_text.push_back(std::to_string(year));
    }
    for (const auto& text : year_text) {
        year_pointers.push_back(text.c_str());
    }
    ImPlot::SetupAxisTicks(
        ImAxis_Y1, 0.5,
        static_cast<double>(grid.years.size()) - 0.5,
        static_cast<int>(grid.years.size()), year_pointers.data());
    const int rows = static_cast<int>(grid.years.size());
    const int columns = static_cast<int>(grid.columns.size());
    ImPlot::PlotHeatmap("Monthly returns", grid.values.data(), rows, columns,
                        grid.color_min, grid.color_max, "%.2f",
                        ImPlotPoint(0.0, 0.0),
                        ImPlotPoint(static_cast<double>(columns),
                                    static_cast<double>(rows)));
    ImPlot::EndPlot();
}

void plot_series_chart(const series_chart& chart) {
    if (chart.series.empty() || !ImPlot::BeginPlot(chart.title.c_str())) {
        return;
    }
    ImPlot::SetupAxes(nullptr, chart.y_label.c_str());
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (const auto& series : chart.series) {
        if (series.index.empty()) {
            continue;
        }
        const auto x = day_axis(series.index);
        minimum = std::min(minimum, x.front());
        maximum = std::max(maximum, x.back());
        ImPlotSpec spec;
        if (series.benchmark) {
            spec.LineColor = benchmark_color();
            spec.LineWeight = 1.2F;
        }
        ImPlot::PlotLine(series.label.c_str(), x.data(),
                         series.values.data(),
                         static_cast<int>(series.values.size()), spec);
    }
    if (chart.zero_line && std::isfinite(minimum) &&
        std::isfinite(maximum)) {
        const double x[2]{minimum, maximum};
        const double y[2]{0.0, 0.0};
        ImPlotSpec spec;
        spec.Flags = ImPlotItemFlags_NoLegend;
        spec.LineColor = ImVec4(0.0F, 0.0F, 0.0F, 0.5F);
        ImPlot::PlotLine("Zero", x, y, 2, spec);
    }
    ImPlot::EndPlot();
}

void plot_drawdown_chart(const series_chart& chart) {
    if (chart.series.empty() || !ImPlot::BeginPlot(chart.title.c_str())) {
        return;
    }
    ImPlot::SetupAxes(nullptr, chart.y_label.c_str());
    for (const auto& series : chart.series) {
        if (series.index.empty()) {
            continue;
        }
        const auto x = day_axis(series.index);
        ImPlotSpec fill;
        fill.FillColor = ImVec4(0.85F, 0.1F, 0.1F, 1.0F);
        fill.FillAlpha = 0.3F;
        fill.LineColor = ImVec4(0.85F, 0.1F, 0.1F, 1.0F);
        ImPlot::PlotShaded(series.label.c_str(), x.data(),
                           series.values.data(),
                           static_cast<int>(series.values.size()), 0.0,
                           fill);
        ImPlot::PlotLine(series.label.c_str(), x.data(),
                         series.values.data(),
                         static_cast<int>(series.values.size()));
    }
    ImPlot::EndPlot();
}

void plot_bar_chart(const bar_chart& chart) {
    if (chart.values.empty() || !ImPlot::BeginPlot("Grouped bars")) {
        return;
    }
    ImPlot::SetupAxes(nullptr, nullptr);
    std::vector<const char*> series_pointers;
    series_pointers.reserve(chart.series_labels.size());
    for (const auto& label : chart.series_labels) {
        series_pointers.push_back(label.c_str());
    }
    std::vector<const char*> group_pointers;
    group_pointers.reserve(chart.group_labels.size());
    for (const auto& label : chart.group_labels) {
        group_pointers.push_back(label.c_str());
    }
    ImPlot::SetupAxisTicks(
        ImAxis_X1, 0.0,
        static_cast<double>(chart.group_labels.size()) - 1.0,
        static_cast<int>(chart.group_labels.size()),
        group_pointers.data());
    ImPlot::PlotBarGroups(
        series_pointers.data(), chart.values.data(),
        static_cast<int>(chart.series_labels.size()),
        static_cast<int>(chart.group_labels.size()));
    ImPlot::EndPlot();
}

void plot_histogram(const histogram_bins& bins) {
    if (bins.counts.empty() || !ImPlot::BeginPlot("Distribution")) {
        return;
    }
    std::vector<double> centers;
    centers.reserve(bins.counts.size());
    for (std::size_t index = 0; index < bins.counts.size(); ++index) {
        centers.push_back(
            0.5 * (bins.edges[index] + bins.edges[index + 1U]));
    }
    const double width =
        bins.edges.size() > 1U ? bins.edges[1U] - bins.edges[0U] : 1.0;
    ImPlot::SetupAxes(nullptr, "Count");
    ImPlot::PlotBars("Counts", centers.data(), bins.counts.data(),
                     static_cast<int>(bins.counts.size()), width);
    ImPlot::EndPlot();
}

void plot_correlation_matrix(const correlation_matrix& matrix) {
    if (matrix.labels.empty() || !ImPlot::BeginPlot("Correlation")) {
        return;
    }
    ImPlot::SetupAxes(nullptr, nullptr);
    std::vector<const char*> pointers;
    pointers.reserve(matrix.labels.size());
    for (const auto& label : matrix.labels) {
        pointers.push_back(label.c_str());
    }
    const int count = static_cast<int>(matrix.labels.size());
    ImPlot::SetupAxisTicks(ImAxis_X1, 0.0,
                           static_cast<double>(count) - 1.0, count,
                           pointers.data());
    ImPlot::SetupAxisTicks(ImAxis_Y1, 0.0,
                           static_cast<double>(count) - 1.0, count,
                           pointers.data());
    ImPlot::PlotHeatmap("Correlation", matrix.values.data(), count, count,
                        -1.0, 1.0, "%.2f",
                        ImPlotPoint(-0.5, -0.5),
                        ImPlotPoint(static_cast<double>(count) - 0.5,
                                    static_cast<double>(count) - 0.5));
    ImPlot::EndPlot();
}

void plot_frontier_chart(const frontier_chart_data& chart) {
    if (chart.volatilities.empty() ||
        !ImPlot::BeginPlot("Efficient Frontier")) {
        return;
    }
    ImPlot::SetupAxes(chart.x_label.c_str(), chart.y_label.c_str());
    ImPlot::PlotLine("Efficient frontier", chart.volatilities.data(),
                     chart.returns.data(),
                     static_cast<int>(chart.volatilities.size()));
    ImPlotSpec minimum;
    minimum.Marker = ImPlotMarker_Circle;
    minimum.MarkerSize = 6.0F;
    double minimum_volatility = chart.min_variance_volatility;
    double minimum_return = chart.min_variance_return;
    ImPlot::PlotScatter("Min variance", &minimum_volatility, &minimum_return,
                        1, minimum);
    ImPlotSpec maximum;
    maximum.Marker = ImPlotMarker_Square;
    maximum.MarkerSize = 6.0F;
    double maximum_volatility = chart.max_sharpe_volatility;
    double maximum_return = chart.max_sharpe_return;
    ImPlot::PlotScatter("Max Sharpe", &maximum_volatility, &maximum_return, 1,
                        maximum);
    ImPlot::EndPlot();
}

}  // namespace fincore
