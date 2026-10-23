#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/tears.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

fincore::factor_analysis_model tiny_model() {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D"};
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C"};
    for (int date = 0; date < 3; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            fincore::factor_row row;
            row.date = start + date * day;
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = asset + 1;
            row.forward_returns = {0.01 * (asset + 1)};
            data.rows.push_back(row);
        }
    }
    return fincore::analyze_factor(data);
}

TEST(FactorTears, BuildsNeutralChartData) {
    const auto model = tiny_model();
    const auto charts = fincore::build_factor_tear_charts(model);
    ASSERT_EQ(charts.information_coefficient.series.size(), 1U);
    EXPECT_EQ(charts.information_coefficient.series[0].label, "1D");
    ASSERT_EQ(charts.information_coefficient.series[0].values.size(), 3U);
    EXPECT_NEAR(charts.information_coefficient.series[0].values[0], 1.0,
                1e-12);
    EXPECT_TRUE(charts.information_coefficient.zero_line);

    ASSERT_EQ(charts.quantile_returns.group_labels.size(), 1U);
    ASSERT_EQ(charts.quantile_returns.series_labels.size(), 3U);
    ASSERT_EQ(charts.quantile_returns.values.size(), 3U);
    EXPECT_EQ(charts.quantile_returns.series_labels[0], "Q1");
    EXPECT_NEAR(charts.quantile_returns.values[0], -0.01, 1e-12);
    EXPECT_NEAR(charts.quantile_returns.values[2], 0.01, 1e-12);

    ASSERT_EQ(charts.turnover.series.size(), 3U);
    ASSERT_EQ(charts.spread.series.size(), 1U);
    EXPECT_NEAR(charts.spread.series[0].values[0], 0.02, 1e-12);
    ASSERT_EQ(charts.rank_autocorrelation.series.size(), 1U);
    EXPECT_NEAR(charts.rank_autocorrelation.series[0].values[1], 1.0,
                1e-12);
}

TEST(FactorTears, BuildsGroupedChartData) {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D"};
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C", "D"};
    const char* groups[] = {"G1", "G1", "G2", "G2"};
    const int quantiles[] = {1, 2, 1, 2};
    for (int date = 0; date < 4; ++date) {
        for (int asset = 0; asset < 4; ++asset) {
            fincore::factor_row row;
            row.date = start + date * day;
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = quantiles[asset];
            row.has_group = true;
            row.group = groups[asset];
            row.forward_returns = {
                0.01 * (asset + 1) + 0.001 * date};
            data.rows.push_back(row);
        }
    }
    fincore::factor_analysis_config config;
    config.by_group = true;
    config.turnover_periods = {1, 2};
    config.include_portfolio_inputs = false;
    const auto model = fincore::analyze_factor(data, config);
    const auto charts = fincore::build_factor_tear_charts(model);
    ASSERT_EQ(charts.grouped.size(), 2U);
    EXPECT_EQ(charts.grouped[0].group, "G1");
    EXPECT_EQ(charts.grouped[1].group, "G2");
    ASSERT_EQ(charts.grouped[0].information_coefficient.series.size(), 1U);
    EXPECT_EQ(charts.grouped[0].information_coefficient.series[0].label,
              "1D");
    EXPECT_EQ(charts.grouped[0]
                  .information_coefficient.series[0]
                  .values.size(),
              4U);
    ASSERT_EQ(charts.grouped[0].quantile_returns.series_labels.size(), 2U);
    EXPECT_EQ(charts.grouped[0].quantile_returns.series_labels[0], "Q1");
    EXPECT_EQ(charts.grouped[0].quantile_returns.series_labels[1], "Q2");
    EXPECT_EQ(charts.grouped[0].quantile_returns.values.size(), 2U);
    ASSERT_EQ(charts.grouped[0].turnover.series.size(), 2U);
    EXPECT_EQ(charts.grouped[0].turnover.series[0].label, "Q1");
    ASSERT_EQ(charts.grouped[0].rank_autocorrelation.series.size(), 2U);
    ASSERT_EQ(charts.grouped[0].spread.series.size(), 1U);
    EXPECT_NEAR(charts.grouped[0].spread.series[0].values[0], 0.01, 1e-12);
    EXPECT_NEAR(charts.grouped[1].spread.series[0].values[0], 0.01, 1e-12);
}

}  // namespace
