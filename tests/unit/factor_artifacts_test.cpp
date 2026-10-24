#include <fincore/calendar.hpp>
#include <fincore/factor/artifacts.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/performance.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr fincore::timestamp day = 86'400'000'000'000LL;

fincore::timestamp date(const int offset) {
    return fincore::utc_timestamp(2022, 1, 1) + static_cast<fincore::timestamp>(offset) * day;
}

fincore::quantile_date_return_table two_quantile_returns() {
    fincore::quantile_date_return_table table;
    table.dates = {date(0), date(1)};
    table.quantiles = {1, 2};
    table.labels = {"1D"};
    // row-major: date x quantile x label
    table.means = {0.01, 0.02, -0.01, 0.0};
    table.standard_errors = {0.001, 0.002, 0.001, 0.002};
    return table;
}

TEST(FactorArtifacts, BuildsCumulativeQuantileChartData) {
    const auto chart = fincore::build_cumulative_returns_by_quantile(
        two_quantile_returns(), "1D");
    ASSERT_EQ(chart.series.size(), 2U);
    EXPECT_EQ(chart.series[0].quantile, 2);
    EXPECT_EQ(chart.series[1].quantile, 1);
    ASSERT_EQ(chart.series[0].values.size(), 2U);
    EXPECT_NEAR(chart.series[0].values[0], 1.02, 1e-12);
    EXPECT_NEAR(chart.series[0].values[1], 1.02, 1e-12);
    EXPECT_NEAR(chart.series[1].values[0], 1.01, 1e-12);
    EXPECT_NEAR(chart.series[1].values[1], 0.9999, 1e-12);
    EXPECT_NEAR(chart.y_min, 0.9999, 1e-12);
    EXPECT_NEAR(chart.y_max, 1.02, 1e-12);
    EXPECT_TRUE(chart.log_scale);
    EXPECT_EQ(chart.reference, 1.0);
    EXPECT_EQ(chart.title,
              "Cumulative Return by Quantile\n(1D Period Forward Return)");
    EXPECT_EQ(fincore::DECIMAL_TO_BPS, 10'000.0);

    const std::string first = chart.to_svg();
    const std::string second = chart.to_svg();
    EXPECT_EQ(first, second);
    EXPECT_NE(first.find("<svg"), std::string::npos);
    EXPECT_NE(first.find("polyline"), std::string::npos);

    EXPECT_THROW(fincore::build_cumulative_returns_by_quantile(
                     two_quantile_returns(), "5D"),
                 fincore::factor_data_error);
    EXPECT_EQ(fincore::plot_cumulative_returns_by_quantile(
                  two_quantile_returns(), "1D")
                  .title,
              chart.title);
}

TEST(FactorArtifacts, BucketsEventsOnEqualTimeIntervals) {
    const std::vector<fincore::timestamp> dates{
        fincore::utc_timestamp(2022, 1, 1),
        fincore::utc_timestamp(2022, 1, 1, 12U),
        fincore::utc_timestamp(2022, 1, 2),
        fincore::utc_timestamp(2022, 1, 3),
        fincore::utc_timestamp(2022, 1, 5)};
    const std::vector<double> values{1.0, 2.0,
                                     std::numeric_limits<double>::quiet_NaN(),
                                     4.0, 5.0};
    const auto chart = fincore::build_events_distribution(dates, values, 4U);
    ASSERT_EQ(chart.bucket_starts.size(), 5U);
    ASSERT_EQ(chart.counts.size(), 5U);
    EXPECT_EQ(chart.counts, (std::vector<double>{2.0, 0.0, 1.0, 0.0, 1.0}));
    EXPECT_EQ(chart.bucket_starts[1], fincore::utc_timestamp(2022, 1, 2));
    EXPECT_EQ(chart.bucket_starts[4], fincore::utc_timestamp(2022, 1, 5));
    const std::string first = chart.to_svg();
    EXPECT_EQ(first, chart.to_svg());
    EXPECT_NE(first.find("<rect"), std::string::npos);

    EXPECT_THROW(fincore::build_events_distribution(dates, values, 0U),
                 fincore::factor_data_error);
}

TEST(FactorArtifacts, BuildsNormalQqScatterData) {
    fincore::factor_ic_table ic;
    ic.dates = {date(0), date(1), date(2), date(3), date(4)};
    ic.labels = {"1D"};
    ic.values = {0.1, 0.2, 0.3, 0.4, 0.5};
    const auto charts = fincore::build_ic_qq(ic);
    ASSERT_EQ(charts.size(), 1U);
    ASSERT_EQ(charts[0].series.size(), 1U);
    const auto& series = charts[0].series[0];
    EXPECT_EQ(series.period, "1D");
    ASSERT_EQ(series.theoretical_quantiles.size(), 5U);
    EXPECT_NEAR(series.theoretical_quantiles[2], 0.0, 1e-12);
    EXPECT_NEAR(series.theoretical_quantiles[0], -0.9674215661017015, 1e-9);
    EXPECT_NEAR(series.mean, 0.3, 1e-12);
    EXPECT_NEAR(series.scale, std::sqrt(0.02), 1e-12);
    EXPECT_NEAR(series.observed_quantiles[0], -1.4142135623730951, 1e-9);
    EXPECT_NEAR(series.observed_quantiles[4], 1.4142135623730951, 1e-9);
    EXPECT_EQ(series.reference_slope, 1.0);
    EXPECT_EQ(charts[0].to_svg(), charts[0].to_svg());
}

TEST(FactorArtifacts, BuildsSpreadSeriesWithRollingMeanAndBand) {
    std::vector<fincore::timestamp> dates;
    std::vector<double> spreads;
    std::vector<double> errors;
    for (int index = 0; index < 25; ++index) {
        dates.push_back(date(index));
        spreads.push_back(0.001);
        errors.push_back(0.0005);
    }
    const auto chart = fincore::build_mean_quantile_returns_spread_time_series(
        dates, spreads, "1D", errors, 1.0);
    ASSERT_EQ(chart.spreads.size(), 25U);
    EXPECT_NEAR(chart.spreads[0], 10.0, 1e-12);
    EXPECT_TRUE(std::isnan(chart.rolling_mean[20]));
    EXPECT_NEAR(chart.rolling_mean[21], 10.0, 1e-12);
    ASSERT_EQ(chart.band_lower.size(), 25U);
    EXPECT_NEAR(chart.band_lower[0], 5.0, 1e-12);
    EXPECT_NEAR(chart.band_upper[0], 15.0, 1e-12);
    EXPECT_NEAR(chart.y_limit, 10.0, 1e-12);
    const std::string first = chart.to_svg();
    EXPECT_EQ(first, chart.to_svg());
    EXPECT_NE(first.find("polygon"), std::string::npos);
}

TEST(FactorArtifacts, BuildsMonthlyIcHeatmapMatrix) {
    fincore::factor_time_information_table monthly;
    monthly.frequency = "M";
    monthly.dates = {fincore::utc_timestamp(2020, 1, 31),
                     fincore::utc_timestamp(2020, 2, 29),
                     fincore::utc_timestamp(2021, 1, 31)};
    monthly.labels = {"1D"};
    monthly.means = {0.01, 0.02, 0.03};
    const auto charts = fincore::build_monthly_ic_heatmap(monthly);
    ASSERT_EQ(charts.size(), 1U);
    const auto& chart = charts[0];
    EXPECT_EQ(chart.years, (std::vector<int>{2020, 2021}));
    ASSERT_EQ(chart.month_columns.size(), 12U);
    EXPECT_EQ(chart.month_columns[0], "Jan");
    ASSERT_EQ(chart.values.size(), 24U);
    EXPECT_NEAR(chart.values[0], 0.01, 1e-12);
    EXPECT_NEAR(chart.values[1], 0.02, 1e-12);
    EXPECT_FALSE(chart.present[2]);
    EXPECT_TRUE(std::isnan(chart.values[2]));
    EXPECT_NEAR(chart.values[12], 0.03, 1e-12);
    EXPECT_EQ(chart.title, "Monthly Mean 1D Period IC");
    EXPECT_EQ(chart.to_svg(), chart.to_svg());
}

TEST(FactorArtifacts, BuildsViolinSamplesAndQuartiles) {
    fincore::quantile_date_return_table table;
    table.dates = {date(0), date(1), date(2)};
    table.quantiles = {1, 2};
    table.labels = {"1D"};
    table.means = {0.01, 0.02,
                   0.02, 0.03,
                   std::numeric_limits<double>::quiet_NaN(), 0.04};
    const auto chart = fincore::build_quantile_returns_violin(table, "1D");
    ASSERT_EQ(chart.groups.size(), 2U);
    EXPECT_EQ(chart.groups[0].quantile, 1);
    EXPECT_EQ(chart.groups[0].samples, (std::vector<double>{100.0, 200.0}));
    EXPECT_NEAR(chart.groups[0].median, 150.0, 1e-12);
    EXPECT_EQ(chart.groups[1].samples,
              (std::vector<double>{200.0, 300.0, 400.0}));
    EXPECT_NEAR(chart.groups[1].first_quartile, 250.0, 1e-12);
    EXPECT_NEAR(chart.groups[1].median, 300.0, 1e-12);
    EXPECT_NEAR(chart.groups[1].third_quartile, 350.0, 1e-12);
    ASSERT_FALSE(chart.groups[1].density_x.empty());
    EXPECT_EQ(chart.groups[1].density_x.size(),
              chart.groups[1].density_y.size());
    EXPECT_EQ(chart.to_svg(), chart.to_svg());
}

TEST(FactorArtifacts, StyleContextsAreDeterministic) {
    const auto context = fincore::plotting_context();
    EXPECT_EQ(context.name, "notebook");
    EXPECT_EQ(context.font_scale, 1.5);
    EXPECT_EQ(context.rc.at("lines.linewidth"), "1.5");

    const auto style = fincore::axes_style("whitegrid", {{"axes.edgecolor", "black"}});
    EXPECT_EQ(style.name, "whitegrid");
    EXPECT_EQ(style.rc.at("axes.edgecolor"), "black");

    const auto custom = fincore::customize(false);
    EXPECT_FALSE(custom.set_context);
    EXPECT_TRUE(custom.colorblind_palette);
    EXPECT_TRUE(custom.despine_left);
    EXPECT_EQ(custom.palette.size(), 8U);
}

TEST(FactorArtifacts, GridFigureAllocatesCellsAndRowsWithLifecycle) {
    fincore::grid_figure grid(3U, 2U);
    EXPECT_EQ(grid.rows(), 3U);
    EXPECT_EQ(grid.cols(), 2U);
    EXPECT_DOUBLE_EQ(grid.width(), 14.0);
    EXPECT_DOUBLE_EQ(grid.height(), 21.0);
    EXPECT_DOUBLE_EQ(grid.wspace(), 0.4);
    EXPECT_DOUBLE_EQ(grid.hspace(), 0.3);
    EXPECT_EQ(grid.create_new_figure(), 1U);

    const auto first = grid.next_cell();
    EXPECT_EQ(first.row, 0U);
    EXPECT_EQ(first.col, 0U);
    EXPECT_FALSE(first.full_row);
    const auto second = grid.next_cell();
    EXPECT_EQ(second.row, 0U);
    EXPECT_EQ(second.col, 1U);
    const auto third = grid.next_cell();
    EXPECT_EQ(third.row, 1U);
    EXPECT_EQ(third.col, 0U);
    const auto row = grid.next_row();
    EXPECT_EQ(row.row, 2U);
    EXPECT_EQ(row.col, 0U);
    EXPECT_TRUE(row.full_row);
    EXPECT_EQ(row.index, 3U);
    EXPECT_EQ(grid.current_row(), 3U);
    EXPECT_THROW(grid.next_cell(), std::out_of_range);

    grid.close();
    EXPECT_TRUE(grid.closed());
    EXPECT_TRUE(grid.cells().empty());
    grid.close();
    EXPECT_THROW(grid.next_cell(), std::runtime_error);
    EXPECT_THROW(grid.next_row(), std::runtime_error);
    EXPECT_THROW(grid.create_new_figure(), std::runtime_error);

    EXPECT_THROW(fincore::grid_figure(0U, 2U), fincore::factor_data_error);
    EXPECT_THROW(fincore::grid_figure(2U, 0U), fincore::factor_data_error);
}

TEST(FactorArtifacts, OwnsOnlyItsOwnFigures) {
    fincore::owned_figures figures;
    figures.figures = {10U, 11U, 12U};
    EXPECT_EQ(fincore::show_owned_figures(figures),
              (std::vector<std::uint64_t>{10U, 11U, 12U}));
    fincore::close_owned_figures(figures);
    ASSERT_EQ(figures.closed.size(), 3U);
    EXPECT_TRUE(figures.closed[0]);
    EXPECT_TRUE(figures.closed[1]);
    EXPECT_TRUE(figures.closed[2]);
    fincore::close_owned_figures(figures);
    EXPECT_EQ(figures.figures.size(), 3U);
}

}  // namespace
