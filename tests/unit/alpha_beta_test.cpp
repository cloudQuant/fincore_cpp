#include <fincore/metrics/alpha_beta.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

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

}  // namespace
