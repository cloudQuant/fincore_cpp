#include <fincore/error.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/report/portfolio.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

fincore::dataframe series_fixture(std::string name,
                                  const std::vector<double>& values) {
    return fincore::make_series({1, 2, 3, 4, 5}, name, values);
}

TEST(ReportPortfolio, BuildsPerformanceAndBenchmarkSections) {
    const auto returns = series_fixture("returns", {0.01, 0.02, -0.01, 0.03, 0.0});
    const auto benchmark =
        series_fixture("benchmark", {0.005, 0.01, -0.005, 0.02, 0.0});
    const auto document =
        fincore::build_portfolio_report(returns, &benchmark, "Portfolio");
    EXPECT_EQ(document.domain, "portfolio");
    EXPECT_EQ(document.title, "Portfolio");
    ASSERT_EQ(document.sections.size(), 2U);
    EXPECT_EQ(document.sections[0].key, "performance");
    EXPECT_EQ(document.sections[1].key, "benchmark");
    bool has_sharpe = false;
    bool has_alpha = false;
    for (const auto& metric : document.sections[0].metrics) {
        if (metric.key == "sharpe_ratio") {
            has_sharpe = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_NEAR(*metric.number, fincore::sharpe_ratio(returns), 1e-15);
        }
        if (metric.key == "annual_return") {
            ASSERT_TRUE(metric.number.has_value());
        }
    }
    EXPECT_TRUE(has_sharpe);
    for (const auto& metric : document.sections[1].metrics) {
        if (metric.key == "tracking_error") {
            has_alpha = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_GT(*metric.number, 0.0);
        }
        if (metric.key == "beta") {
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_NEAR(*metric.number, fincore::beta(returns, benchmark),
                        1e-15);
        }
    }
    EXPECT_TRUE(has_alpha);
    ASSERT_EQ(document.sections[0].series.size(), 6U);
    EXPECT_EQ(document.sections[0].series[0].key, "returns");
    ASSERT_EQ(document.sections[0].series[1].key, "cumulative_returns");
    ASSERT_EQ(document.sections[0].series[1].y.size(), 5U);
    EXPECT_NEAR(document.sections[0].series[1].y[1], 1.01 * 1.02, 1e-15);
    EXPECT_EQ(document.sections[0].series[2].key, "drawdown");
    EXPECT_EQ(document.sections[0].series[3].key, "rolling_sharpe");
    EXPECT_EQ(document.sections[0].series[4].key, "rolling_volatility");
    EXPECT_EQ(document.sections[0].series[5].key, "monthly_returns");
    ASSERT_EQ(document.sections[0].tables.size(), 1U);
    EXPECT_EQ(document.sections[0].tables[0].key, "drawdowns");
    ASSERT_EQ(document.sections[1].series.size(), 2U);
    EXPECT_EQ(document.sections[1].series[1].key, "rolling_beta");
    bool has_alpha_metric = false;
    for (const auto& metric : document.sections[1].metrics) {
        if (metric.key == "alpha") {
            has_alpha_metric = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_NEAR(*metric.number, fincore::alpha(returns, benchmark),
                        1e-15);
        }
    }
    EXPECT_TRUE(has_alpha_metric);

    const auto json = document.to_json();
    EXPECT_NE(json.find("\"domain\": \"portfolio\""), std::string::npos);
    EXPECT_NE(json.find("\"sharpe_ratio\""), std::string::npos);
    EXPECT_NE(json.find("\"benchmark\""), std::string::npos);
    EXPECT_EQ(json, document.to_json());

    const auto without_benchmark =
        fincore::build_portfolio_report(returns);
    EXPECT_EQ(without_benchmark.sections.size(), 1U);

    const auto positions = fincore::make_series({1, 2, 3, 4, 5}, "A",
                                                {0.6, 0.6, 0.6, 0.6, 0.6});
    auto position_frame = positions;
    position_frame.load_column<double>(
        "B", {0.4, 0.4, 0.4, 0.4, 0.4}, hmdf::nan_policy::dont_pad_with_nans);
    position_frame.load_column<double>(
        "cash", {0.0, 0.0, 0.0, 0.0, 0.0},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto with_positions = fincore::build_portfolio_report(
        returns, &benchmark, "Portfolio", &position_frame);
    ASSERT_EQ(with_positions.sections.size(), 3U);
    EXPECT_EQ(with_positions.sections[2].key, "portfolio");
    EXPECT_EQ(with_positions.sections[2].series.front().key,
              "gross_leverage");
    ASSERT_EQ(with_positions.sections[2].series.front().y.size(), 5U);
    EXPECT_NEAR(with_positions.sections[2].series.front().y.front(), 1.0,
                1e-15);
    bool has_asset_count = false;
    for (const auto& metric : with_positions.sections[2].metrics) {
        if (metric.key == "asset_count") {
            has_asset_count = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_DOUBLE_EQ(*metric.number, 2.0);
        }
    }
    EXPECT_TRUE(has_asset_count);

    auto short_positions = position_frame;
    auto short_index = short_positions.get_index();
    short_index.pop_back();
    fincore::dataframe missing;
    missing.load_index(std::move(short_index));
    for (const auto& name : {"A", "B", "cash"}) {
        auto values = std::vector<double>(4U, 0.0);
        missing.load_column<double>(name, std::move(values),
                                    hmdf::nan_policy::dont_pad_with_nans);
    }
    EXPECT_THROW(static_cast<void>(fincore::build_portfolio_report(
                     returns, nullptr, "Portfolio", &missing)),
                 fincore::input_error);
}

TEST(ReportPortfolio, ProjectsTransactionActivity) {
    const auto returns =
        series_fixture("returns", {0.01, 0.02, -0.01, 0.03, 0.0});
    const auto day = 86'400'000'000'000LL;
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const std::vector<fincore::portfolio_transaction> transactions{
        {start, "A", 10.0, 5.0},
        {start, "B", -5.0, 20.0},
        {start + day, "A", -2.0, 6.0}};
    const auto document = fincore::build_portfolio_report(
        returns, nullptr, "Portfolio", nullptr, 63U, transactions);
    ASSERT_EQ(document.sections.size(), 2U);
    EXPECT_EQ(document.sections[1].key, "transactions");
    EXPECT_EQ(document.sections[1].series.size(), 2U);
    EXPECT_EQ(document.sections[1].series[0].key,
              "daily_transaction_count");
    ASSERT_EQ(document.sections[1].series[0].y.size(), 2U);
    EXPECT_DOUBLE_EQ(document.sections[1].series[0].y[0], 2.0);
    EXPECT_DOUBLE_EQ(document.sections[1].series[0].y[1], 1.0);
    ASSERT_EQ(document.sections[1].series[1].y.size(), 2U);
    EXPECT_DOUBLE_EQ(document.sections[1].series[1].y[0], 150.0);
    EXPECT_DOUBLE_EQ(document.sections[1].series[1].y[1], 12.0);
    bool has_count = false;
    bool has_days = false;
    for (const auto& metric : document.sections[1].metrics) {
        if (metric.key == "transaction_count") {
            has_count = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_DOUBLE_EQ(*metric.number, 3.0);
        }
        if (metric.key == "trading_days") {
            has_days = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_DOUBLE_EQ(*metric.number, 2.0);
        }
    }
    EXPECT_TRUE(has_count);
    EXPECT_TRUE(has_days);

    auto invalid = transactions;
    invalid[0].price = std::nan("");
    EXPECT_THROW(static_cast<void>(fincore::build_portfolio_report(
                     returns, nullptr, "Portfolio", nullptr, 63U, invalid)),
                 fincore::input_error);
}

TEST(ReportPortfolio, ProjectsTurnoverFromPositionsAndTransactions) {
    const auto day = 86'400'000'000'000LL;
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{start, start + day});
    returns.load_column<double>(
        "returns", {0.01, 0.02}, hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{start, start + day});
    positions.load_column<double>(
        "A", {100.0, 80.0}, hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>(
        "B", {-50.0, -50.0}, hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>(
        "cash", {10.0, 30.0}, hmdf::nan_policy::dont_pad_with_nans);
    const std::vector<fincore::portfolio_transaction> transactions{
        {start, "A", 10.0, 5.0},
        {start, "B", -5.0, 20.0},
        {start + day, "A", -2.0, 6.0}};
    const auto document = fincore::build_portfolio_report(
        returns, nullptr, "Portfolio", &positions, 63U, transactions);
    ASSERT_EQ(document.sections.size(), 3U);
    EXPECT_EQ(document.sections[1].key, "portfolio");
    EXPECT_EQ(document.sections[2].key, "transactions");
    bool has_average_turnover = false;
    for (const auto& metric : document.sections[1].metrics) {
        if (metric.key == "average_turnover") {
            has_average_turnover = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_NEAR(*metric.number, 1.042857142857143, 1e-12);
        }
    }
    EXPECT_TRUE(has_average_turnover);
    const fincore::report_series* turnover = nullptr;
    for (const auto& series : document.sections[1].series) {
        if (series.key == "turnover") {
            turnover = &series;
        }
    }
    ASSERT_NE(turnover, nullptr);
    ASSERT_EQ(turnover->y.size(), 2U);
    EXPECT_NEAR(turnover->y[0], 2.0, 1e-12);
    EXPECT_NEAR(turnover->y[1], 0.08571428571428572, 1e-12);
}

}  // namespace
