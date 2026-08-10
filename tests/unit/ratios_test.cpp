#include <fincore/metrics/ratios.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

TEST(Ratios, AnnualizesSharpeFromSampleStandardDeviation) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, -0.01});
    EXPECT_NEAR(fincore::sharpe_ratio(returns, 0.0, 252),
                6.928203230275507, 1e-12);
}

TEST(Ratios, MatchesReferenceSharpeSortinoAndCalmar) {
    const auto returns = fincore::make_series({1, 2, 3, 4}, "returns",
                                              {0.01, -0.02, 0.03, -0.01});
    EXPECT_NEAR(fincore::sharpe_ratio(returns), 1.789801617640478, 1e-12);
    EXPECT_NEAR(fincore::sortino_ratio(returns), 3.549647869859768, 1e-12);
    EXPECT_NEAR(fincore::calmar_ratio(returns), 39.58220278509036, 1e-12);
}

TEST(Ratios, IgnoresNanAndSupportsNamedReturnColumn) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "strategy",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.03});
    EXPECT_NEAR(fincore::sharpe_ratio(returns, 0.0, 252, "strategy"),
                22.44994432064365, 1e-12);
}

TEST(Ratios, ReturnsNanForUndefinedRatioAndMissingColumnThrows) {
    const auto flat = fincore::make_series({1, 2}, "returns", {0.01, 0.01});
    const auto gains = fincore::make_series({1, 2}, "returns", {0.01, 0.02});
    EXPECT_TRUE(std::isinf(fincore::sharpe_ratio(flat)));
    EXPECT_GT(fincore::sharpe_ratio(flat), 0.0);
    EXPECT_TRUE(std::isnan(fincore::calmar_ratio(gains)));
    EXPECT_THROW(fincore::sortino_ratio(gains, 0.0, 252, "missing"),
                 fincore::input_error);
}

TEST(Ratios, PreservesOriginalLengthForCalmarAnnualizationWithNan) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {-0.02, std::numeric_limits<double>::quiet_NaN(), 0.01});

    EXPECT_NEAR(fincore::calmar_ratio(returns), -28.86726241438432, 1e-12);
}

TEST(Ratios, ReturnsSignedInfinityForZeroDownsideSortino) {
    const auto returns = fincore::make_series(
        {1, 2}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN()});

    EXPECT_TRUE(std::isinf(fincore::sortino_ratio(returns)));
    EXPECT_GT(fincore::sortino_ratio(returns), 0.0);
}

}  // namespace
