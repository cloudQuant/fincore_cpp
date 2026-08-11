#include <fincore/metrics/perf_stats.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace {

TEST(PerfStatsPublicHeader, PublishesUnaryAndBenchmarkKeyContracts) {
    const auto returns = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3}, "returns",
        std::vector<double>{.01, -.02, .03});
    const auto benchmark = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3}, "benchmark",
        std::vector<double>{.005, -.01, .015});

    const auto unary = fincore::perf_stats(returns);
    const auto paired = fincore::perf_stats(returns, benchmark);

    EXPECT_EQ(unary.size(), 13U);
    EXPECT_EQ(paired.size(), 15U);
    for (const auto* key : {"Annual return", "Cumulative returns",
                            "Annual volatility", "Sharpe ratio",
                            "Calmar ratio", "Stability", "Max drawdown",
                            "Omega ratio", "Sortino ratio", "Skew",
                            "Kurtosis", "Tail ratio",
                            "Daily value at risk"}) {
        EXPECT_EQ(unary.count(key), 1U);
        EXPECT_EQ(paired.count(key), 1U);
    }
    EXPECT_EQ(unary.count("Alpha"), 0U);
    EXPECT_EQ(unary.count("Beta"), 0U);
    EXPECT_EQ(paired.count("Alpha"), 1U);
    EXPECT_EQ(paired.count("Beta"), 1U);
}

}  // namespace
