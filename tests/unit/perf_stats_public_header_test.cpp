#include <fincore/metrics/perf_stats.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace {

TEST(PerfStatsPublicHeader, PublishesUnaryAndBenchmarkKeyContracts) {
    // This file includes only perf_stats.hpp, so the series helpers below arrive
    // through its transitive includes rather than from their own headers.
    const auto returns = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3}, "returns",
        std::vector<double>{.01, -.02, .03});
    const auto benchmark = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3}, "benchmark",
        std::vector<double>{.005, -.01, .015});

    const auto unary = fincore::perf_stats(returns);
    const auto paired = fincore::perf_stats(returns, benchmark);

    // The two sizes are what make the loop below exhaustive: each listed key
    // present in both maps would still pass if the benchmark map carried an
    // extra display name the case never mentions.
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
