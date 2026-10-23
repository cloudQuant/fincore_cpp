#include <fincore/metrics/alpha_beta.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

namespace {

TEST(AlphaBeta, UsesCovarianceOverPopulationBenchmarkVariance) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, 0.03});
    const auto benchmark = fincore::make_series({1, 2, 3}, "benchmark",
                                                {0.02, 0.04, 0.06});
    EXPECT_NEAR(fincore::beta(returns, benchmark), 0.5, 1e-12);
}

TEST(AlphaBeta, AlignsDataframesBeforeEstimatingBeta) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, 0.03});
    const auto benchmark = fincore::make_series({2, 3, 4}, "benchmark",
                                                {0.04, 0.06, 0.08});
    EXPECT_NEAR(fincore::beta(returns, benchmark), 0.5, 1e-12);
}

TEST(AlphaBeta, MatchesReferenceAlphaAfterAlignment) {
    const auto returns = fincore::make_series({1, 2, 3, 4}, "returns",
                                              {0.01, -0.02, 0.03, -0.01});
    const auto benchmark = fincore::make_series({1, 2, 3, 4}, "benchmark",
                                                {0.005, -0.01, 0.02, -0.005});
    EXPECT_NEAR(fincore::beta(returns, benchmark), 1.6666666666666667, 1e-12);
    EXPECT_NEAR(fincore::alpha(returns, benchmark), -0.3431833620809164, 1e-12);
}

TEST(AlphaBeta, IgnoresPairwiseNanAndSupportsNamedColumns) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "strategy", {0.01, std::numeric_limits<double>::quiet_NaN(), 0.03});
    const auto benchmark = fincore::make_series({1, 2, 3}, "market", {0.02, 0.04, 0.06});
    EXPECT_NEAR(fincore::beta(returns, benchmark, 0.0, "strategy", "market"), 0.5, 1e-12);
    const auto flat = fincore::make_series({1, 2}, "benchmark", {0.02, 0.02});
    EXPECT_TRUE(std::isnan(fincore::beta(returns, flat, 0.0, "strategy")));
    EXPECT_THROW(fincore::beta(returns, benchmark, 0.0, "missing", "market"),
                 fincore::input_error);
}

TEST(AlphaBeta, ReturnsNanForFlatBenchmarkWithMultipleValidPairs) {
    const auto returns =
        fincore::make_series({1, 2, 3}, "returns", {0.01, 0.02, 0.03});
    const auto benchmark =
        fincore::make_series({1, 2, 3}, "benchmark", {0.02, 0.02, 0.02});

    EXPECT_TRUE(std::isnan(fincore::beta(returns, benchmark)));
}

TEST(AlphaBeta, PercentileRankMatchesPythonOracle) {
    const std::vector<fincore::timestamp> index{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const std::vector<double> values{0.01, 0.02, -0.05, 0.03, 0.01,
                                     -0.02, 0.04, 0.01, 0.02, -0.03};
    const auto strategy = fincore::make_series(index, "returns", values);
    const auto benchmark = fincore::make_series(
        index, "benchmark",
        {-0.04, -0.03, -0.02, -0.01, -0.005, 0.005, 0.01, 0.02, 0.03, 0.04});

    std::vector<fincore::dataframe> peers;
    std::vector<double> first;
    std::vector<double> second;
    std::vector<double> third;
    for (const double value : values) {
        first.push_back(value * 1.1 + 0.001);
        second.push_back(value * 0.5 - 0.0005);
        third.push_back(value * -0.3 + 0.002);
    }
    peers.push_back(fincore::make_series(index, "returns", first));
    peers.push_back(fincore::make_series(index, "returns", second));
    peers.push_back(fincore::make_series(
        std::vector<fincore::timestamp>{1, 2}, "returns", {0.01, 0.02}));
    peers.push_back(fincore::make_series(index, "returns", third));

    EXPECT_NEAR(fincore::alpha_percentile_rank(strategy, peers, benchmark),
                0.6666666666666666, 1e-12);
    EXPECT_NEAR(fincore::alpha_percentile_rank(strategy, peers, benchmark,
                                               0.001),
                0.6666666666666666, 1e-12);

    const auto short_strategy =
        fincore::make_series(std::vector<fincore::timestamp>{1}, "returns",
                             {0.01});
    EXPECT_TRUE(std::isnan(
        fincore::alpha_percentile_rank(short_strategy, peers, benchmark)));
    EXPECT_TRUE(std::isnan(fincore::alpha_percentile_rank(
        strategy, std::span<const fincore::dataframe>{}, benchmark)));

    const auto owned = [&] {
        std::vector<fincore::dataframe> local_peers;
        local_peers.push_back(fincore::make_series(index, "returns", first));
        local_peers.push_back(fincore::make_series(index, "returns", second));
        local_peers.push_back(fincore::make_series(index, "returns", third));
        return fincore::alpha_percentile_rank(strategy, local_peers, benchmark);
    }();
    EXPECT_NEAR(owned, 0.6666666666666666, 1e-12);
}

}  // namespace
