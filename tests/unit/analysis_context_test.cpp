#include <fincore/analysis_context.hpp>

#include <gtest/gtest.h>

namespace {

TEST(AnalysisContext, CachesAndPublishesCoreSummary) {
    fincore::analysis_context context{
        fincore::make_series({1, 2, 3}, "returns", {0.01, -0.02, 0.03})};

    const auto summary = context.summary();
    const auto repeated_summary = context.summary();

    EXPECT_TRUE(summary.contains("Sharpe ratio"));
    EXPECT_NEAR(summary.at("Max drawdown"), -0.02, 1e-12);
    EXPECT_EQ(summary, repeated_summary);
    EXPECT_EQ(context.computation_count("Max drawdown"), 1U);
}

TEST(AnalysisContext, AddsBenchmarkMetricsOnlyWhenBenchmarkIsProvided) {
    const auto returns =
        fincore::make_series({1, 2, 3}, "returns", {0.01, 0.02, 0.03});
    fincore::analysis_context without_benchmark{returns};
    EXPECT_FALSE(without_benchmark.summary().contains("Beta"));

    fincore::analysis_context with_benchmark{
        returns, fincore::make_series({1, 2, 3}, "benchmark", {0.02, 0.04, 0.06})};
    EXPECT_NEAR(with_benchmark.summary().at("Beta"), 0.5, 1e-12);
}

}  // namespace
