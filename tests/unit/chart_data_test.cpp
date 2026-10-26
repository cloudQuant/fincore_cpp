#include <fincore/viz/chart_data.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

fincore::dataframe daily_series(const fincore::timestamp start, int periods,
                                const std::vector<double>& values,
                                const std::string& name = "returns") {
    std::vector<fincore::timestamp> index;
    for (int position = 0; position < periods; ++position) {
        index.push_back(start +
                        static_cast<fincore::timestamp>(position) *
                            86'400'000'000'000LL);
    }
    return fincore::make_series(index, name, values);
}

fincore::dataframe multi_column_frame() {
    std::vector<fincore::timestamp> index;
    for (int position = 0; position < 5; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
    }
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>(
        "A", std::vector<double>{0.01, -0.02, 0.03, -0.01, 0.02},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "B",
        std::vector<double>{0.02, -0.01,
                            std::numeric_limits<double>::quiet_NaN(), 0.01,
                            0.0},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("C",
                              std::vector<double>{0.0, 0.01, 0.02, 0.03, 0.04},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("D", std::vector<double>{0.01, 0.01, 0.01, 0.01,
                                                       0.01},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

// build_monthly_heatmap rejects an index that steps backwards, so every fixture
// here is ascending by construction; repeated dates would be accepted but are not
// exercised by this file.
TEST(ChartData, MonthlyHeatmapCompoundsAndMarksMissing) {
    std::vector<double> values;
    for (int position = 0; position < 55; ++position) {
        values.push_back(0.01 * std::sin(position * 0.7));
    }
    const auto full =
        fincore::build_monthly_heatmap(daily_series(fincore::utc_timestamp(
                                                        2021, 1, 15),
                                                    55, values));
    ASSERT_EQ(full.years.size(), 1U);
    EXPECT_EQ(full.years.front(), 2021);
    ASSERT_EQ(full.columns.size(), 12U);
    EXPECT_EQ(full.columns.front(), "Jan");
    EXPECT_EQ(full.columns.back(), "Dec");
    ASSERT_EQ(full.values.size(), 12U);
    EXPECT_NEAR(full.values[0], 0.560685325129362, 1e-12);
    EXPECT_NEAR(full.values[1], -0.7060026454676072, 1e-12);
    EXPECT_NEAR(full.values[2], 0.07200391792694383, 1e-12);
    EXPECT_TRUE(full.present[0]);
    EXPECT_TRUE(full.present[1]);
    EXPECT_TRUE(full.present[2]);
    EXPECT_FALSE(full.present[3]);
    EXPECT_TRUE(std::isnan(full.values[3]));
    EXPECT_NEAR(full.color_min, -0.7060026454676072, 1e-12);
    EXPECT_NEAR(full.color_max, 0.560685325129362, 1e-12);
    EXPECT_TRUE(full.color_centered);
    EXPECT_EQ(full.unit, "percent");

    const std::vector<fincore::timestamp> missing_index{
        fincore::utc_timestamp(2021, 1, 5),
        fincore::utc_timestamp(2021, 1, 20),
        fincore::utc_timestamp(2021, 3, 3),
        fincore::utc_timestamp(2021, 3, 4),
        fincore::utc_timestamp(2021, 3, 5),
        fincore::utc_timestamp(2021, 3, 10)};
    const auto missing = fincore::build_monthly_heatmap(fincore::make_series(
        missing_index, "returns",
        {0.02, -0.01, 0.03, 0.04, -0.02, 0.01}));
    ASSERT_EQ(missing.years.size(), 1U);
    EXPECT_NEAR(missing.values[0], 0.9800000000000031, 1e-12);
    EXPECT_FALSE(missing.present[1]);
    EXPECT_TRUE(std::isnan(missing.values[1]));
    EXPECT_NEAR(missing.values[2], 6.0273760000000065, 1e-12);
    EXPECT_NEAR(missing.color_min, 0.9800000000000031, 1e-12);
    EXPECT_NEAR(missing.color_max, 6.0273760000000065, 1e-12);
    EXPECT_FALSE(missing.color_centered);
}

TEST(ChartData, MonthlyHeatmapHandlesConstantNegativeAndEmptyInputs) {
    const auto constant = fincore::build_monthly_heatmap(daily_series(
        fincore::utc_timestamp(2021, 2, 1), 28,
        std::vector<double>(28, 0.01)));
    ASSERT_EQ(constant.years.size(), 1U);
    EXPECT_NEAR(constant.values[1], 32.12909668982513, 1e-12);
    EXPECT_NEAR(constant.color_min, 31.129096689825133, 1e-12);
    EXPECT_NEAR(constant.color_max, 33.12909668982513, 1e-12);
    EXPECT_FALSE(constant.color_centered);

    const auto negative = fincore::build_monthly_heatmap(daily_series(
        fincore::utc_timestamp(2021, 4, 1), 30,
        std::vector<double>(30, -0.01)));
    ASSERT_EQ(negative.years.size(), 1U);
    EXPECT_NEAR(negative.values[3], -26.02996266117198, 1e-12);
    EXPECT_NEAR(negative.color_min, -27.02996266117198, 1e-12);
    EXPECT_NEAR(negative.color_max, -25.02996266117198, 1e-12);
    EXPECT_FALSE(negative.color_centered);

    const auto empty = fincore::build_monthly_heatmap(fincore::make_series(
        std::vector<fincore::timestamp>{}, "returns",
        std::vector<double>{}));
    EXPECT_TRUE(empty.years.empty());
    EXPECT_TRUE(empty.values.empty());
    EXPECT_EQ(empty.columns.size(), 12U);
    EXPECT_DOUBLE_EQ(empty.color_min, -10.0);
    EXPECT_DOUBLE_EQ(empty.color_max, 10.0);
    EXPECT_TRUE(empty.color_centered);

    const auto two_years = fincore::build_monthly_heatmap(
        fincore::make_series(
            std::vector<fincore::timestamp>{
                fincore::utc_timestamp(2020, 12, 15),
                fincore::utc_timestamp(2021, 1, 15)},
            "returns", {0.01, 0.02}));
    ASSERT_EQ(two_years.years.size(), 2U);
    EXPECT_EQ(two_years.years[0], 2020);
    EXPECT_EQ(two_years.years[1], 2021);
    ASSERT_EQ(two_years.values.size(), 24U);
    EXPECT_NEAR(two_years.values[11], 1.0, 1e-12);
    EXPECT_NEAR(two_years.values[12], 2.0, 1e-12);
}

TEST(ChartData, SeriesChartsOwnIndexValuesAndMetadata) {
    const auto portfolio = daily_series(
        fincore::utc_timestamp(2021, 1, 1), 4,
        {0.0, 0.01, 0.03, 0.02}, "cumulative_returns");
    const auto returns = fincore::build_returns_chart(portfolio);
    EXPECT_EQ(returns.title, "Cumulative Returns");
    EXPECT_EQ(returns.y_label, "Cumulative Return");
    EXPECT_TRUE(returns.zero_line);
    EXPECT_FALSE(returns.fill_to_zero);
    ASSERT_EQ(returns.series.size(), 1U);
    EXPECT_EQ(returns.series[0].label, "Cumulative Returns");
    ASSERT_EQ(returns.series[0].values.size(), 4U);
    EXPECT_DOUBLE_EQ(returns.series[0].values[2], 0.03);
    EXPECT_EQ(returns.series[0].index.size(), 4U);

    const auto sharpe = daily_series(
        fincore::utc_timestamp(2021, 1, 1), 4, {0.1, 0.2, 0.15, 0.3},
        "sharpe");
    // The benchmark is deliberately one day later and one row shorter, so the
    // size and front timestamp below prove the series keeps the benchmark frame's
    // own index instead of the portfolio's four-day index.
    const auto benchmark = daily_series(
        fincore::utc_timestamp(2021, 1, 2), 3, {0.05, 0.1, 0.2},
        "benchmark_sharpe");
    const auto chart = fincore::build_rolling_sharpe_chart(
        sharpe, "sharpe", 60, &benchmark, "benchmark_sharpe");
    EXPECT_EQ(chart.title, "Rolling Sharpe Ratio (60-day window)");
    EXPECT_EQ(chart.y_label, "Sharpe Ratio");
    EXPECT_EQ(chart.window, 60U);
    ASSERT_EQ(chart.series.size(), 2U);
    EXPECT_FALSE(chart.series[0].benchmark);
    EXPECT_TRUE(chart.series[1].benchmark);
    EXPECT_TRUE(chart.series[1].dashed);
    EXPECT_EQ(chart.series[1].label, "Benchmark Sharpe");
    ASSERT_EQ(chart.series[1].values.size(), 3U);
    EXPECT_EQ(chart.series[1].index.front(),
              fincore::utc_timestamp(2021, 1, 2));

    const auto drawdown = fincore::build_drawdown_chart(
        daily_series(fincore::utc_timestamp(2021, 1, 1), 3,
                     {0.0, -0.02, -0.01}, "drawdown"));
    EXPECT_EQ(drawdown.title, "Drawdown");
    EXPECT_TRUE(drawdown.fill_to_zero);
    EXPECT_FALSE(drawdown.zero_line);
    EXPECT_EQ(drawdown.series[0].label, "Drawdown");
}

TEST(ChartData, BarChartPreservesGroupedRowMajorOrder) {
    const auto chart = fincore::build_bar_chart(
        {"Q1", "Q2"}, {"Strategy", "Benchmark"},
        {0.01, 0.005, -0.02, 0.001});
    ASSERT_EQ(chart.group_labels.size(), 2U);
    ASSERT_EQ(chart.series_labels.size(), 2U);
    ASSERT_EQ(chart.values.size(), 4U);
    EXPECT_DOUBLE_EQ(chart.values[0], 0.01);
    EXPECT_DOUBLE_EQ(chart.values[1], 0.005);
    EXPECT_DOUBLE_EQ(chart.values[2], -0.02);
    EXPECT_DOUBLE_EQ(chart.values[3], 0.001);
    EXPECT_THROW(static_cast<void>(fincore::build_bar_chart(
                     {"Q1"}, {"S"}, {0.01, 0.02})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::build_bar_chart(
                     {}, {"S"}, {})),
                 fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::build_bar_chart(
            {"Q1"}, {"S"},
            {std::numeric_limits<double>::quiet_NaN()})),
        fincore::input_error);
}

TEST(ChartData, HistogramMatchesNumpyBins) {
    const std::vector<double> values{0.001, -0.02, 0.015, 0.03, -0.01,
                                     0.0,   0.025, -0.03, 0.005};
    const auto histogram = fincore::build_histogram(values, 4);
    ASSERT_EQ(histogram.counts.size(), 4U);
    ASSERT_EQ(histogram.edges.size(), 5U);
    EXPECT_DOUBLE_EQ(histogram.edges[0], -0.03);
    EXPECT_DOUBLE_EQ(histogram.edges[1], -0.015);
    EXPECT_NEAR(histogram.edges[2], 0.0, 1e-18);
    EXPECT_DOUBLE_EQ(histogram.edges[3], 0.015);
    EXPECT_DOUBLE_EQ(histogram.edges[4], 0.03);
    EXPECT_DOUBLE_EQ(histogram.counts[0], 2.0);
    EXPECT_DOUBLE_EQ(histogram.counts[1], 1.0);
    EXPECT_DOUBLE_EQ(histogram.counts[2], 3.0);
    EXPECT_DOUBLE_EQ(histogram.counts[3], 3.0);
    EXPECT_EQ(histogram.n_observations, 9U);
    EXPECT_NEAR(histogram.density[0], 14.814814814814817, 1e-12);
    EXPECT_NEAR(histogram.density[2], 22.22222222222222, 1e-12);

    const auto constant =
        fincore::build_histogram(std::vector<double>{0.01, 0.01, 0.01}, 3);
    EXPECT_DOUBLE_EQ(constant.edges[0], -0.49);
    EXPECT_NEAR(constant.edges[1], -0.15666666666666668, 1e-15);
    EXPECT_NEAR(constant.edges[2], 0.17666666666666664, 1e-15);
    EXPECT_DOUBLE_EQ(constant.edges[3], 0.51);
    EXPECT_DOUBLE_EQ(constant.counts[0], 0.0);
    EXPECT_DOUBLE_EQ(constant.counts[1], 3.0);
    EXPECT_DOUBLE_EQ(constant.counts[2], 0.0);

    const auto with_nan = fincore::build_histogram(
        std::vector<double>{0.01, std::numeric_limits<double>::quiet_NaN(),
                            0.02},
        2);
    EXPECT_EQ(with_nan.n_observations, 2U);
    EXPECT_THROW(static_cast<void>(fincore::build_histogram(
                     std::vector<double>{}, 4)),
                 fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::build_histogram(values, 0)),
        fincore::input_error);
}

TEST(ChartData, CorrelationMatrixMatchesPandasPairwiseSemantics) {
    const auto matrix =
        fincore::build_correlation_matrix(multi_column_frame());
    ASSERT_EQ(matrix.labels.size(), 4U);
    EXPECT_EQ(matrix.labels[0], "A");
    EXPECT_EQ(matrix.labels[3], "D");
    ASSERT_EQ(matrix.values.size(), 16U);
    EXPECT_NEAR(matrix.values[0], 1.0, 1e-15);
    EXPECT_NEAR(matrix.values[1], 0.42426406871192857, 1e-15);
    EXPECT_NEAR(matrix.values[2], 0.22874785549890703, 1e-15);
    EXPECT_TRUE(std::isnan(matrix.values[3]));
    EXPECT_FALSE(matrix.present[3]);
    EXPECT_NEAR(matrix.values[4], 0.42426406871192857, 1e-15);
    EXPECT_NEAR(matrix.values[5], 1.0, 1e-15);
    EXPECT_NEAR(matrix.values[6], -0.282842712474619, 1e-15);
    EXPECT_NEAR(matrix.values[9], -0.282842712474619, 1e-15);
    EXPECT_NEAR(matrix.values[10], 1.0, 1e-15);
    EXPECT_FALSE(matrix.present[15]);
    EXPECT_TRUE(std::isnan(matrix.values[15]));
    EXPECT_EQ(matrix.unit, "correlation");
}

TEST(ChartData, FrontierChartConsumesComputedFrontier) {
    fincore::efficient_frontier_result frontier;
    frontier.frontier_volatilities = {0.05, 0.06, 0.07};
    frontier.frontier_returns = {0.10, 0.20, 0.30};
    frontier.frontier_sharpe = {2.0, 3.0, 4.0};
    frontier.min_variance.weights = {0.5, 0.5};
    frontier.min_variance.expected_return = 0.10;
    frontier.min_variance.volatility = 0.05;
    frontier.max_sharpe.weights = {0.0, 1.0};
    frontier.max_sharpe.expected_return = 0.30;
    frontier.max_sharpe.volatility = 0.07;
    const auto chart = fincore::build_frontier_chart(frontier);
    ASSERT_EQ(chart.volatilities.size(), 3U);
    ASSERT_EQ(chart.returns.size(), 3U);
    ASSERT_EQ(chart.sharpe.size(), 3U);
    EXPECT_DOUBLE_EQ(chart.min_variance_volatility, 0.05);
    EXPECT_DOUBLE_EQ(chart.min_variance_return, 0.10);
    EXPECT_DOUBLE_EQ(chart.max_sharpe_volatility, 0.07);
    EXPECT_DOUBLE_EQ(chart.max_sharpe_return, 0.30);
    EXPECT_EQ(chart.x_label, "Volatility");
    EXPECT_EQ(chart.y_label, "Return");

    frontier.frontier_sharpe.pop_back();
    EXPECT_THROW(static_cast<void>(fincore::build_frontier_chart(frontier)),
                 fincore::input_error);
}

}  // namespace
