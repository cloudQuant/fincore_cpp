#include <fincore/metrics/returns.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

namespace {

TEST(Returns, ComputesSimpleAndCumulativeReturnsWithOriginalIndex) {
    const auto prices =
        fincore::make_series({1, 2, 3}, "price", {100.0, 110.0, 99.0});

    const auto simple = fincore::simple_returns(prices, "price");

    EXPECT_EQ(simple.get_index(), (std::vector<fincore::timestamp>{2, 3}));
    const auto& simple_values =
        simple.get_column<double>(fincore::returns_column.data());
    ASSERT_EQ(simple_values.size(), 2U);
    EXPECT_NEAR(simple_values[0], 0.10, 1e-12);
    EXPECT_NEAR(simple_values[1], -0.10, 1e-12);
    EXPECT_NEAR(fincore::cumulative_return(simple), -0.01, 1e-12);
}

TEST(Returns, TreatsNanAsZeroDuringCumulativeCompounding) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.02});

    const auto cumulative = fincore::cumulative_returns(returns);
    const auto& values =
        cumulative.get_column<double>(fincore::returns_column.data());

    EXPECT_EQ(cumulative.get_index(), returns.get_index());
    EXPECT_NEAR(values[0], 0.01, 1e-12);
    EXPECT_NEAR(values[1], 0.01, 1e-12);
    EXPECT_NEAR(values[2], 0.0302, 1e-12);
}

TEST(Returns, CumulativeReturnCompoundsNanSafely) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.02});

    EXPECT_NEAR(fincore::cumulative_return(returns), 0.0302, 1e-12);
}

TEST(Returns, HandlesEmptyAndNonPositiveEndingWealth) {
    const auto wiped_out = fincore::make_series({1}, "returns", {-1.0});
    const auto empty = fincore::make_series({}, "returns", {});

    EXPECT_DOUBLE_EQ(fincore::annual_return(wiped_out), -1.0);
    EXPECT_TRUE(std::isnan(fincore::annual_return(empty)));
}

TEST(Returns, AnnualReturnUsesRequestedAnnualization) {
    const auto returns = fincore::make_series({1, 2}, "returns", {0.10, 0.10});

    EXPECT_NEAR(fincore::annual_return(returns, 2.0), 0.21, 1e-12);
}

TEST(Returns, IgnoresNanSamplesForAnnualVolatility) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.03});

    EXPECT_NEAR(fincore::annual_volatility(returns), 0.2244994432064365,
                1e-12);
}

TEST(Returns, AnnualVolatilityRequiresTwoValidSamples) {
    const auto returns = fincore::make_series(
        {1, 2}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN()});

    EXPECT_TRUE(std::isnan(fincore::annual_volatility(returns)));
}

TEST(Returns, ValidatesRequestedInputColumn) {
    const auto returns = fincore::make_series({1}, "returns", {0.01});

    EXPECT_THROW(fincore::simple_returns(returns, "missing"),
                 fincore::input_error);
}

}  // namespace
