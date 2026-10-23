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
    ASSERT_EQ(document.sections[0].series.size(), 1U);
    ASSERT_EQ(document.sections[0].series[0].y.size(), 5U);
    EXPECT_NEAR(document.sections[0].series[0].y[1], 1.01 * 1.02, 1e-15);

    const auto json = document.to_json();
    EXPECT_NE(json.find("\"domain\": \"portfolio\""), std::string::npos);
    EXPECT_NE(json.find("\"sharpe_ratio\""), std::string::npos);
    EXPECT_NE(json.find("\"benchmark\""), std::string::npos);
    EXPECT_EQ(json, document.to_json());

    const auto without_benchmark =
        fincore::build_portfolio_report(returns);
    EXPECT_EQ(without_benchmark.sections.size(), 1U);
}

}  // namespace
