#include <fincore/metrics/drawdown.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

namespace {

TEST(Drawdown, UsesPeakToTroughCompoundedReturns) {
    const auto returns =
        fincore::make_series({1, 2, 3}, "returns", {0.10, -0.20, 0.05});

    EXPECT_NEAR(fincore::max_drawdown(returns), -0.20, 1e-12);
}

TEST(Drawdown, PreservesReturnsIndexInDrawdownSeries) {
    const auto returns =
        fincore::make_series({10, 20, 30}, "returns", {0.10, -0.20, 0.05});

    const auto drawdown = fincore::drawdown_series(returns);

    EXPECT_EQ(drawdown.get_index(), returns.get_index());
    const auto& values =
        drawdown.get_column<double>(fincore::returns_column.data());
    ASSERT_EQ(values.size(), 3U);
    EXPECT_NEAR(values[0], 0.0, 1e-12);
    EXPECT_NEAR(values[1], -0.20, 1e-12);
    EXPECT_NEAR(values[2], -0.16, 1e-12);
}

TEST(Drawdown, ReturnsNanForEmptyInput) {
    const auto returns = fincore::make_series({}, "returns", {});

    EXPECT_TRUE(std::isnan(fincore::max_drawdown(returns)));
}

TEST(Drawdown, TreatsNanReturnAsZeroWhenCalculatingWealth) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.10, std::numeric_limits<double>::quiet_NaN(), -0.10});

    EXPECT_NEAR(fincore::max_drawdown(returns), -0.10, 1e-12);
}

}  // namespace
