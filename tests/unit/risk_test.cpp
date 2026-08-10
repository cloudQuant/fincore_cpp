#include <fincore/metrics/risk.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

TEST(Risk, ComputesHistoricalVarAndTailAverage) {
    const auto returns = fincore::make_series({1, 2, 3, 4, 5}, "returns",
                                              {-0.10, -0.05, 0.00, 0.02, 0.10});
    EXPECT_NEAR(fincore::historical_value_at_risk(returns, 0.20), -0.06, 1e-12);
    EXPECT_NEAR(fincore::conditional_value_at_risk(returns, 0.20), -0.10, 1e-12);
}

TEST(Risk, RejectsNonFiniteHistoricalRiskInput) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {-0.10, std::numeric_limits<double>::quiet_NaN(), 0.10});
    EXPECT_TRUE(std::isnan(fincore::historical_value_at_risk(returns, 0.20)));
    EXPECT_TRUE(std::isnan(fincore::conditional_value_at_risk(returns, 0.20)));
}

TEST(Risk, ValidatesCutoffAndNamedReturnColumn) {
    const auto returns = fincore::make_series({1, 2}, "strategy", {-0.1, 0.1});
    EXPECT_NEAR(fincore::historical_value_at_risk(returns, 0.5, "strategy"),
                0.0, 1e-12);
    EXPECT_NEAR(fincore::historical_value_at_risk(returns, 0.0, "strategy"),
                -0.1, 1e-12);
    EXPECT_NEAR(fincore::conditional_value_at_risk(returns, 0.0, "strategy"),
                -0.1, 1e-12);
    EXPECT_NEAR(fincore::historical_value_at_risk(returns, 1.0, "strategy"),
                0.1, 1e-12);
    EXPECT_NEAR(fincore::conditional_value_at_risk(returns, 1.0, "strategy"),
                0.0, 1e-12);
    EXPECT_TRUE(std::isnan(fincore::historical_value_at_risk(returns, -0.1, "strategy")));
    EXPECT_TRUE(std::isnan(fincore::conditional_value_at_risk(returns, 1.1, "strategy")));
    EXPECT_THROW(fincore::historical_value_at_risk(returns, 0.5, "missing"),
                 fincore::input_error);
}

}  // namespace
