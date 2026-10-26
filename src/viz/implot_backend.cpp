#include <fincore/viz/implot_backend.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/factor/artifacts.hpp>

#include <implot.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace fincore {
namespace {

// ImPlot's axis works in days rather than nanoseconds, so the timestamps are
// divided here and the tick formatter multiplies them back with a round rather a
// truncation: a value one ulp under a whole day would otherwise label the previous
// date. Only the leading date part of the formatted timestamp reaches the tick.
constexpr double backend_nanoseconds_per_day = 86'400'000'000'000.0;

int format_date_axis(double value, char* buffer, int size, void*);

void plot_series(const dataframe& frame, std::string_view column,
                 std::string_view title, std::string_view label) {
    const auto values = require_double_column(frame, column);
    if (values.empty()) {
        return;
    }

    std::vector<double> x;
    x.reserve(frame.get_index().size());
    for (const timestamp value : frame.get_index()) {
        x.push_back(static_cast<double>(value) / backend_nanoseconds_per_day);
    }

    const std::string plot_title{title};
    const std::string series_label{label};
    if (ImPlot::BeginPlot(plot_title.data())) {
        ImPlot::SetupAxisFormat(ImAxis_X1, format_date_axis);
        ImPlot::PlotLine(series_label.data(), x.data(), values.data(),
                         static_cast<int>(values.size()));
        ImPlot::EndPlot();
    }
}

std::vector<double> day_axis(const std::vector<timestamp>& index) {
    std::vector<double> values;
    values.reserve(index.size());
    for (const timestamp value : index) {
        values.push_back(static_cast<double>(value) / backend_nanoseconds_per_day);
    }
    return values;
}

ImVec4 benchmark_color() { return ImVec4(0.45F, 0.45F, 0.45F, 1.0F); }

int format_date_axis(double value, char* buffer, int size, void*) {
    const auto nanoseconds = static_cast<timestamp>(
        std::llround(value * backend_nanoseconds_per_day));
    const std::string text = detail::format_timestamp(nanoseconds);
    const std::string date = text.substr(0, 10);
    std::snprintf(buffer, static_cast<std::size_t>(size), "%s",
                  date.c_str());
    return static_cast<int>(date.size());
}


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
    if (!ImPlot::BeginPlot(grid.title.c_str())) {
        return;
    }
    std::vector<const char*> month_pointers;
    month_pointers.reserve(grid.columns.size());
    for (const auto& label : grid.columns) {
        month_pointers.push_back(label.c_str());
    }
    if (grid.columns.size() >= 2U) {
        ImPlot::SetupAxis(ImAxis_X1, nullptr);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr);
        ImPlot::SetupAxisTicks(
            ImAxis_X1, 0.5,
            static_cast<double>(grid.columns.size()) - 0.5,
            static_cast<int>(grid.columns.size()), month_pointers.data());
    } else {
        ImPlot::SetupAxis(ImAxis_X1, month_pointers.front(),
                          ImPlotAxisFlags_NoTickLabels);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr);
    }
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
    if (grid.years.size() >= 2U) {
        ImPlot::SetupAxisTicks(
            ImAxis_Y1, 0.5,
            static_cast<double>(grid.years.size()) - 0.5,
            static_cast<int>(grid.years.size()), year_pointers.data());
    } else {
        ImPlot::SetupAxis(ImAxis_Y1, year_pointers.front(),
                          ImPlotAxisFlags_NoTickLabels);
    }
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
    ImPlot::SetupAxisFormat(ImAxis_X1, format_date_axis);
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
    ImPlot::SetupAxisFormat(ImAxis_X1, format_date_axis);
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
    std::vector<const char*> pointers;
    pointers.reserve(matrix.labels.size());
    for (const auto& label : matrix.labels) {
        pointers.push_back(label.c_str());
    }
    const int count = static_cast<int>(matrix.labels.size());
    if (count >= 2) {
        ImPlot::SetupAxisTicks(ImAxis_X1, 0.0,
                               static_cast<double>(count) - 1.0, count,
                               pointers.data());
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0.0,
                               static_cast<double>(count) - 1.0, count,
                               pointers.data());
    } else {
        ImPlot::SetupAxis(ImAxis_X1, matrix.labels.front().c_str(),
                          ImPlotAxisFlags_NoTickLabels);
        ImPlot::SetupAxis(ImAxis_Y1, matrix.labels.front().c_str(),
                          ImPlotAxisFlags_NoTickLabels);
    }
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

void plot_quantile_cumulative_chart(const quantile_cumulative_chart& chart) {
    if (chart.series.empty() || !ImPlot::BeginPlot(chart.title.c_str())) {
        return;
    }
    ImPlot::SetupAxes("Date", chart.y_label.c_str());
    ImPlot::SetupAxisFormat(ImAxis_X1, format_date_axis);
    for (const auto& series : chart.series) {
        const auto x = day_axis(series.dates);
        if (x.empty() || x.size() != series.values.size()) {
            continue;
        }
        const std::string label = "Q" + std::to_string(series.quantile);
        ImPlot::PlotLine(label.c_str(), x.data(), series.values.data(),
                         static_cast<int>(x.size()));
    }
    ImPlot::EndPlot();
}

void plot_events_distribution_chart(const events_distribution_chart& chart) {
    if (!chart.valid || chart.counts.empty() ||
        !ImPlot::BeginPlot(chart.title.c_str())) {
        return;
    }
    ImPlot::SetupAxes(chart.x_label.c_str(), chart.y_label.c_str());
    ImPlot::SetupAxisFormat(ImAxis_X1, format_date_axis);
    const auto x = day_axis(chart.bucket_starts);
    if (x.size() != chart.counts.size() || x.empty()) {
        ImPlot::EndPlot();
        return;
    }
    const double width =
        x.size() > 1U ? (x.back() - x.front()) / static_cast<double>(x.size())
                      : 1.0;
    ImPlot::PlotBars("Events", x.data(), chart.counts.data(),
                     static_cast<int>(x.size()), width);
    ImPlot::EndPlot();
}

void plot_qq_chart(const qq_chart& chart) {
    for (const auto& series : chart.series) {
        if (series.theoretical_quantiles.empty() ||
            series.theoretical_quantiles.size() !=
                series.observed_quantiles.size()) {
            continue;
        }
        const std::string title = "IC QQ " + series.period;
        if (!ImPlot::BeginPlot(title.c_str())) {
            continue;
        }
        ImPlot::SetupAxes(chart.x_label.c_str(), chart.y_label.c_str());
        ImPlot::PlotScatter("Observed", series.theoretical_quantiles.data(),
                            series.observed_quantiles.data(),
                            static_cast<int>(
                                series.theoretical_quantiles.size()));
        const double x_min = series.theoretical_quantiles.front();
        const double x_max = series.theoretical_quantiles.back();
        const std::array<double, 2> reference_x{x_min, x_max};
        const std::array<double, 2> reference_y{
            series.reference_intercept + series.reference_slope * x_min,
            series.reference_intercept + series.reference_slope * x_max};
        ImPlot::PlotLine("Reference", reference_x.data(),
                         reference_y.data(), 2);
        ImPlot::EndPlot();
    }
}

void plot_spread_time_series(const spread_time_series_chart& chart) {
    if (chart.dates.empty() || !ImPlot::BeginPlot(chart.title.c_str())) {
        return;
    }
    ImPlot::SetupAxes("Date", chart.y_label.c_str());
    ImPlot::SetupAxisFormat(ImAxis_X1, format_date_axis);
    const auto x = day_axis(chart.dates);
    const std::size_t count = std::min(x.size(), chart.spreads.size());
    if (count != 0U) {
        ImPlot::PlotLine("Spread", x.data(), chart.spreads.data(),
                         static_cast<int>(count));
    }
    if (chart.rolling_mean.size() >= count && count != 0U) {
        ImPlot::PlotLine("Rolling mean", x.data(),
                         chart.rolling_mean.data(), static_cast<int>(count));
    }
    if (chart.band_lower.size() >= count && chart.band_upper.size() >= count &&
        count != 0U) {
        ImPlot::PlotShaded("Band", x.data(), chart.band_lower.data(),
                           chart.band_upper.data(), static_cast<int>(count));
    }
    const std::array<double, 2> zero_x{
        static_cast<double>(chart.dates.front()) / backend_nanoseconds_per_day,
        static_cast<double>(chart.dates.back()) / backend_nanoseconds_per_day};
    const std::array<double, 2> zero_y{0.0, 0.0};
    ImPlot::PlotLine("Zero", zero_x.data(), zero_y.data(), 2);
    ImPlot::EndPlot();
}

void plot_monthly_ic_heatmap(const monthly_ic_heatmap_chart& chart) {
    heat_grid grid;
    grid.years = chart.years;
    grid.columns = chart.month_columns;
    grid.values = chart.values;
    grid.present = chart.present;
    grid.title = chart.title;
    grid.unit = "ic";
    grid.color_min = -0.2;
    grid.color_max = 0.2;
    grid.color_centered = true;
    plot_monthly_heatmap(grid);
}

void plot_violin_chart(const violin_chart& chart) {
    if (chart.groups.empty() || !ImPlot::BeginPlot(chart.title.c_str())) {
        return;
    }
    ImPlot::SetupAxes(chart.y_label.c_str(), "Quantile");
    for (const auto& group : chart.groups) {
        if (group.density_x.empty() ||
            group.density_x.size() != group.density_y.size()) {
            continue;
        }
        const auto maximum =
            *std::max_element(group.density_y.begin(), group.density_y.end());
        const double scale = maximum > 0.0 ? 0.35 / maximum : 0.0;
        std::vector<double> lower;
        std::vector<double> upper;
        lower.reserve(group.density_y.size());
        upper.reserve(group.density_y.size());
        for (const double density : group.density_y) {
            lower.push_back(static_cast<double>(group.quantile) -
                            density * scale);
            upper.push_back(static_cast<double>(group.quantile) +
                            density * scale);
        }
        const std::string label = "Q" + std::to_string(group.quantile);
        ImPlot::PlotShaded(label.c_str(), group.density_x.data(), lower.data(),
                           upper.data(),
                           static_cast<int>(group.density_x.size()));
        const std::array<double, 1> median_x{group.median};
        const std::array<double, 1> median_y{
            static_cast<double>(group.quantile)};
        ImPlot::PlotScatter(label.c_str(), median_x.data(), median_y.data(), 1);
    }
    ImPlot::EndPlot();
}

}  // namespace fincore
