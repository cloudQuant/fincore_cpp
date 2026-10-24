#include <fincore/error.hpp>
#include <fincore/viz/bokeh_document.hpp>
#include <fincore/viz/chart_data.hpp>
#include <fincore/viz/plotly_document.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr fincore::timestamp day = 86'400'000'000'000LL;

std::vector<double> chart_values(const fincore::dataframe& frame) {
    const auto values =
        fincore::require_double_column(frame, "cumulative_returns");
    return std::vector<double>(values.begin(), values.end());
}

fincore::dataframe returns_frame() {
    std::vector<fincore::timestamp> index;
    std::vector<double> values;
    for (int position = 0; position < 5; ++position) {
        index.push_back(fincore::utc_timestamp(2021, 1, 4) +
                        static_cast<fincore::timestamp>(position) * day);
        values.push_back(0.01 * static_cast<double>(position + 1));
    }
    return fincore::make_series(std::move(index), "cumulative_returns",
                                std::move(values));
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

std::filesystem::path temporary_path(const std::string& name) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove(path);
    return path;
}

void expect_values_equal(const std::vector<double>& actual,
                         const std::vector<double>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t position = 0U; position < actual.size(); ++position) {
        if (std::isnan(expected[position])) {
            EXPECT_TRUE(std::isnan(actual[position])) << position;
        } else {
            EXPECT_EQ(actual[position], expected[position]) << position;
        }
    }
}

std::size_t count_occurrences(const std::string& text,
                              const std::string& needle) {
    std::size_t count = 0U;
    std::size_t position = text.find(needle);
    while (position != std::string::npos) {
        ++count;
        position = text.find(needle, position + needle.size());
    }
    return count;
}

}  // namespace

TEST(PlotlyDocument, ReturnsCopiesChartDataAndSerializes) {
    const auto frame = returns_frame();
    const auto chart = fincore::build_returns_chart(frame);
    fincore::plotly_backend backend;
    const auto document = backend.plot_returns(chart);

    ASSERT_EQ(document.traces.size(), 1U);
    EXPECT_EQ(document.title, "Cumulative Returns");
    EXPECT_EQ(document.traces[0].y_values, chart.series[0].values);
    ASSERT_EQ(document.traces[0].x_labels.size(), 5U);
    EXPECT_EQ(document.traces[0].x_labels.front(), "2021-01-04T00:00:00");
    EXPECT_EQ(document.traces[0].mode, "lines");
    ASSERT_EQ(document.shapes.size(), 1U);
    EXPECT_DOUBLE_EQ(document.shapes[0].y0, 0.0);
    EXPECT_TRUE(backend.has_figure());
    EXPECT_EQ(backend.figure().traces[0].y_values,
              chart.series[0].values);

    const std::string json = document.to_json();
    EXPECT_NE(json.find("\"data\": ["), std::string::npos);
    EXPECT_NE(json.find("\"type\": \"scatter\""), std::string::npos);
    EXPECT_NE(json.find("\"title\": \"Cumulative Returns\""),
              std::string::npos);
    EXPECT_NE(json.find("\"xaxis\""), std::string::npos);
    EXPECT_EQ(document.to_json(), json);
}

TEST(PlotlyDocument, DrawdownAndRollingSharpeMatchChartData) {
    const auto frame = returns_frame();
    const auto drawdown_chart =
        fincore::build_drawdown_chart(frame, "cumulative_returns");
    fincore::plotly_backend backend;
    const auto drawdown = backend.plot_drawdown(drawdown_chart);
    ASSERT_EQ(drawdown.traces.size(), 1U);
    EXPECT_EQ(drawdown.traces[0].fill, "tozeroy");
    EXPECT_EQ(drawdown.traces[0].line_color, "#D50000");
    EXPECT_EQ(drawdown.traces[0].y_values, drawdown_chart.series[0].values);
    EXPECT_EQ(drawdown.title, "Drawdown (Underwater Chart)");

    auto benchmark_frame = returns_frame();
    benchmark_frame.load_column<double>(
        "benchmark_sharpe",
        std::vector<double>{0.1, 0.2, 0.3, 0.4, 0.5},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto sharpe_chart = fincore::build_rolling_sharpe_chart(
        frame, "cumulative_returns", 60U, &benchmark_frame);
    const auto sharpe = backend.plot_rolling_sharpe(sharpe_chart);
    ASSERT_EQ(sharpe.traces.size(), 2U);
    EXPECT_EQ(sharpe.title, "Rolling Sharpe Ratio (60-day window)");
    expect_values_equal(sharpe.traces[0].y_values, chart_values(frame));
    expect_values_equal(sharpe.traces[1].y_values,
                        std::vector<double>({0.1, 0.2, 0.3, 0.4, 0.5}));
    EXPECT_EQ(sharpe.traces[1].line_dash, "dash");
    EXPECT_EQ(sharpe.traces[0].x_labels.size(), 5U);
    EXPECT_NE(sharpe.to_json().find("\"shapes\""), std::string::npos);
}

TEST(PlotlyDocument, MonthlyHeatmapUsesHeatGrid) {
    std::vector<fincore::timestamp> index;
    std::vector<double> values;
    for (int position = 0; position < 40; ++position) {
        index.push_back(fincore::utc_timestamp(2021, 1, 4) +
                        static_cast<fincore::timestamp>(position) * day);
        values.push_back(position % 2 == 0 ? 0.01 : -0.005);
    }
    const auto frame = fincore::make_series(std::move(index), "returns",
                                            std::move(values));
    const auto grid = fincore::build_monthly_heatmap(frame);
    fincore::plotly_backend backend;
    const auto document = backend.plot_monthly_heatmap(grid);

    ASSERT_EQ(document.traces.size(), 1U);
    const auto& trace = document.traces[0];
    EXPECT_EQ(trace.type, fincore::plotly_trace::kind::heatmap);
    expect_values_equal(trace.heatmap_z, grid.values);
    EXPECT_EQ(trace.heatmap_columns, 12U);
    ASSERT_EQ(trace.heatmap_y.size(), grid.years.size());
    EXPECT_EQ(trace.heatmap_y.front(), std::to_string(grid.years.front()));
    EXPECT_DOUBLE_EQ(trace.color_min, grid.color_min);
    EXPECT_DOUBLE_EQ(trace.color_max, grid.color_max);
    EXPECT_EQ(trace.colorbar_title, "Return (%)");

    const std::string json = document.to_json();
    const std::size_t missing = static_cast<std::size_t>(
        std::count(grid.present.begin(), grid.present.end(), false));
    EXPECT_EQ(count_occurrences(json, "null"), missing);
    EXPECT_NE(json.find("\"colorscale\""), std::string::npos);
    EXPECT_NE(document.to_svg().find("<rect"), std::string::npos);
}

TEST(PlotlyDocument, CorrelationMatrixAndFrontier) {
    std::vector<fincore::timestamp> index;
    for (int position = 0; position < 6; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
    }
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>(
        "A", std::vector<double>{0.01, 0.02, 0.03, 0.01, 0.02, 0.03},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "B", std::vector<double>{0.03, 0.02, 0.01, 0.03, 0.02, 0.01},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto matrix = fincore::build_correlation_matrix(frame);
    fincore::plotly_backend backend;
    const auto correlation = backend.plot_correlation_matrix(matrix);
    ASSERT_EQ(correlation.traces.size(), 1U);
    expect_values_equal(correlation.traces[0].heatmap_z, matrix.values);
    EXPECT_EQ(correlation.traces[0].heatmap_x, matrix.labels);
    EXPECT_EQ(correlation.traces[0].colorscale, "RdBu");
    EXPECT_TRUE(correlation.traces[0].zmid_zero);
    const std::string correlation_json = correlation.to_json();
    EXPECT_NE(correlation_json.find("\"colorscale\": \"RdBu\""),
              std::string::npos);
    EXPECT_NE(correlation_json.find("\"zmid\": 0.0"), std::string::npos);

    fincore::frontier_chart_data frontier;
    frontier.volatilities = {0.1, 0.15, 0.2};
    frontier.returns = {0.05, 0.08, 0.1};
    frontier.sharpe = {0.5, 0.6, 0.4};
    frontier.max_sharpe_volatility = 0.15;
    frontier.max_sharpe_return = 0.08;
    frontier.min_variance_volatility = 0.1;
    frontier.min_variance_return = 0.05;
    const auto efficient = backend.plot_efficient_frontier(frontier);
    ASSERT_EQ(efficient.traces.size(), 3U);
    expect_values_equal(efficient.traces[0].custom_data, frontier.sharpe);
    ASSERT_EQ(efficient.traces[1].x_values.size(), 1U);
    EXPECT_DOUBLE_EQ(efficient.traces[1].x_values[0], 0.15);
    EXPECT_EQ(efficient.traces[1].marker_symbol, "star");
    EXPECT_EQ(efficient.traces[2].marker_symbol, "diamond");
}

TEST(PlotlyDocument, SavesSelfContainedDeterministicArtifacts) {
    const auto chart = fincore::build_returns_chart(returns_frame());
    fincore::plotly_backend backend("dark", 320, 640);
    static_cast<void>(backend.plot_returns(chart));

    const auto html_path = temporary_path("fincore_plotly_document.html");
    backend.save_html(html_path.string());
    const std::string first = read_file(html_path);
    EXPECT_EQ(first.rfind("<!DOCTYPE html>", 0), 0U);
    EXPECT_NE(first.find("id=\"fincore-plotly-document\""),
              std::string::npos);
    EXPECT_NE(first.find("<svg"), std::string::npos);
    EXPECT_EQ(first.find("<script src"), std::string::npos);
    EXPECT_EQ(first.find("<link"), std::string::npos);
    EXPECT_NE(first.find("\"template\": \"plotly_dark\""),
              std::string::npos);
    backend.save_html(html_path.string());
    EXPECT_EQ(read_file(html_path), first);

    const auto svg_path = temporary_path("fincore_plotly_document.svg");
    backend.save_image(svg_path.string());
    const std::string svg = read_file(svg_path);
    EXPECT_EQ(svg.rfind(
                  "<svg xmlns=\"http://www.w3.org/2000/svg\"", 0),
              0U);
    EXPECT_NE(svg.find("width=\"640.00\""), std::string::npos);
    EXPECT_NE(svg.find("<polyline"), std::string::npos);
    backend.save_image(svg_path.string());
    EXPECT_EQ(read_file(svg_path), svg);

    EXPECT_FALSE(backend.show());
    EXPECT_EQ(backend.last_show_status(), fincore::NOT_RUN_INTERACTIVE);
    std::filesystem::remove(html_path);
    std::filesystem::remove(svg_path);
}

TEST(PlotlyDocument, RejectsMissingFigureAndEmptyPath) {
    fincore::plotly_backend backend;
    EXPECT_FALSE(backend.has_figure());
    EXPECT_THROW(backend.save_html("unused.html"), fincore::input_error);
    EXPECT_THROW(backend.save_image("unused.svg"), fincore::input_error);
    EXPECT_THROW(backend.show(), fincore::input_error);
}

TEST(BokehDocument, ReturnsAndDrawdownMatchChartData) {
    const auto frame = returns_frame();
    const auto chart = fincore::build_returns_chart(frame);
    fincore::bokeh_backend backend("dark", 320, 640);
    const auto returns = backend.plot_returns(chart);
    ASSERT_EQ(returns.glyphs.size(), 1U);
    EXPECT_EQ(returns.title, "Cumulative Returns");
    EXPECT_EQ(returns.x_axis_type, "datetime");
    EXPECT_EQ(returns.glyphs[0].y_values, chart.series[0].values);
    EXPECT_EQ(returns.glyphs[0].legend_label, "Cumulative Returns");
    ASSERT_EQ(returns.hovers.size(), 1U);
    EXPECT_EQ(returns.hovers[0].mode, "vline");
    EXPECT_EQ(returns.legend_location, "top_left");
    EXPECT_EQ(returns.height, 320);
    EXPECT_EQ(returns.width, 640);
    EXPECT_EQ(returns.background_fill_color, "#1E1E1E");

    const auto drawdown_chart =
        fincore::build_drawdown_chart(frame, "cumulative_returns");
    const auto drawdown = backend.plot_drawdown(drawdown_chart);
    ASSERT_EQ(drawdown.glyphs.size(), 2U);
    EXPECT_EQ(drawdown.glyphs[0].type, fincore::bokeh_glyph::kind::varea);
    EXPECT_EQ(drawdown.glyphs[0].fill_color, "#D50000");
    EXPECT_DOUBLE_EQ(drawdown.glyphs[0].fill_alpha, 0.3);
    EXPECT_EQ(drawdown.glyphs[1].type, fincore::bokeh_glyph::kind::line);
    EXPECT_EQ(drawdown.glyphs[1].y_values, drawdown_chart.series[0].values);
    EXPECT_NE(drawdown.to_json().find("\"type\": \"varea\""),
              std::string::npos);
}

TEST(BokehDocument, RollingSharpeAndMonthlyHeatmap) {
    auto frame = returns_frame();
    auto benchmark = returns_frame();
    benchmark.load_column<double>(
        "benchmark_sharpe", std::vector<double>{0.5, 0.4, 0.3, 0.2, 0.1},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto chart = fincore::build_rolling_sharpe_chart(
        frame, "cumulative_returns", 30U, &benchmark);
    fincore::bokeh_backend backend;
    const auto sharpe = backend.plot_rolling_sharpe(chart);
    ASSERT_EQ(sharpe.glyphs.size(), 2U);
    EXPECT_EQ(sharpe.glyphs[1].line_dash, "dashed");
    expect_values_equal(sharpe.glyphs[1].y_values,
                        std::vector<double>({0.5, 0.4, 0.3, 0.2, 0.1}));
    EXPECT_EQ(sharpe.title, "Rolling Sharpe Ratio (30-day window)");
    ASSERT_EQ(sharpe.spans.size(), 1U);
    EXPECT_DOUBLE_EQ(sharpe.spans[0].location, 0.0);

    fincore::heat_grid grid;
    grid.years = {2021, 2022};
    grid.columns.assign(std::begin(fincore::month_labels),
                        std::end(fincore::month_labels));
    grid.values.assign(24U, -1.0);
    grid.values[0] = 2.5;
    grid.values[13] = 1.5;
    grid.present.assign(24U, true);
    grid.color_min = -1.0;
    grid.color_max = 2.5;
    const auto heatmap = backend.plot_monthly_heatmap(grid);
    ASSERT_EQ(heatmap.glyphs.size(), 1U);
    EXPECT_EQ(heatmap.glyphs[0].type, fincore::bokeh_glyph::kind::rect);
    expect_values_equal(heatmap.glyphs[0].rect_values, grid.values);
    EXPECT_EQ(heatmap.glyphs[0].palette, "RdYlGn11");
    EXPECT_DOUBLE_EQ(heatmap.glyphs[0].mapper_low, grid.color_min);
    EXPECT_DOUBLE_EQ(heatmap.glyphs[0].mapper_high, grid.color_max);
    ASSERT_EQ(heatmap.y_factors.size(), 2U);
    EXPECT_EQ(heatmap.y_factors.front(), "2022");
    EXPECT_EQ(heatmap.y_factors.back(), "2021");
    EXPECT_TRUE(heatmap.color_bar);
    EXPECT_EQ(heatmap.axis_location, "above");
    EXPECT_FALSE(heatmap.grid_visible);
}

TEST(BokehDocument, HtmlIsSelfContainedAndShowIsHeadless) {
    const auto chart = fincore::build_returns_chart(returns_frame());
    fincore::bokeh_backend backend;
    const auto document = backend.plot_returns(chart);
    const auto path = temporary_path("fincore_bokeh_document.html");
    backend.save_html(document, path.string());
    const std::string html = read_file(path);
    EXPECT_EQ(html.rfind("<!DOCTYPE html>", 0), 0U);
    EXPECT_NE(html.find("id=\"fincore-bokeh-document\""),
              std::string::npos);
    EXPECT_NE(html.find("<svg"), std::string::npos);
    EXPECT_EQ(html.find("<script src"), std::string::npos);
    EXPECT_EQ(html.find("<link"), std::string::npos);
    backend.save_html(document, path.string());
    EXPECT_EQ(read_file(path), html);

    EXPECT_FALSE(backend.show(document));
    EXPECT_EQ(backend.last_show_status(), "NOT_RUN_INTERACTIVE");
    std::filesystem::remove(path);
}

TEST(PrintfTickFormatter, FormatsPythonStyleSpecifiers) {
    EXPECT_EQ(fincore::printf_tick_formatter{}.format(1.234), "1.2");
    EXPECT_EQ(fincore::printf_tick_formatter{"%.2f"}.format(12.345),
              "12.35");
    EXPECT_EQ(fincore::printf_tick_formatter{"%.1f%%"}.format(12.0),
              "12.0%");
    EXPECT_EQ(fincore::printf_tick_formatter{"%d"}.format(12.9), "12");
    EXPECT_EQ(fincore::printf_tick_formatter{"%+.1f"}.format(-2.0),
              "-2.0");
    EXPECT_EQ(fincore::printf_tick_formatter{"%.1f"}.format(0.0), "0.0");
    EXPECT_THROW(fincore::printf_tick_formatter{"%s"},
                 fincore::input_error);
    EXPECT_THROW(fincore::printf_tick_formatter{"%.1f %.1f"},
                 fincore::input_error);
    EXPECT_THROW(fincore::printf_tick_formatter{"abc"},
                 fincore::input_error);
    EXPECT_THROW(fincore::printf_tick_formatter{"%.1f %"},
                 fincore::input_error);
}
