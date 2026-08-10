#include <fincore/metrics/basic.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace {

TEST(BasicMetrics, UsesNativeFrequencyAnnualization) {
    EXPECT_EQ(fincore::annualization_factor(fincore::frequency::daily), 252.0);
    EXPECT_EQ(fincore::annualization_factor(fincore::frequency::monthly), 12.0);
    EXPECT_EQ(fincore::annualization_factor(fincore::frequency::daily, 365.0), 365.0);
}

TEST(BasicMetrics, AdjustReturnsPreservesIndexAndColumn) {
    const auto returns =
        fincore::make_series({10, 20}, "returns", {0.03, -0.01});

    const auto adjusted = fincore::adjust_returns(returns, 0.01);

    EXPECT_EQ(adjusted.get_index(), (std::vector<fincore::timestamp>{10, 20}));
    const auto& adjusted_values = adjusted.get_column<double>("returns");
    ASSERT_EQ(adjusted_values.size(), 2U);
    EXPECT_NEAR(adjusted_values[0], 0.02, 1e-12);
    EXPECT_NEAR(adjusted_values[1], -0.02, 1e-12);
}

TEST(BasicMetrics, ZeroAdjustmentStillReturnsAnIndependentFrame) {
    auto returns = fincore::make_series({10}, "returns", {0.03});

    const auto adjusted = fincore::adjust_returns(returns, 0.0);
    returns.get_column<double>("returns")[0] = 0.50;

    EXPECT_EQ(adjusted.get_column<double>("returns"),
              (std::vector<double>{0.03}));
}

TEST(BasicMetrics, ValidatesRequestedColumn) {
    const auto returns = fincore::make_series({10}, "returns", {0.03});

    EXPECT_THROW(fincore::adjust_returns(returns, 0.01, "missing"),
                 fincore::input_error);
}

}  // namespace
