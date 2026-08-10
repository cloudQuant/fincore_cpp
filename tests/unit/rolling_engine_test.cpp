#include <fincore/rolling_engine.hpp>

#include <array>

#include <gtest/gtest.h>

namespace {

TEST(RollingEngine, PreservesWindowEndIndex) {
    fincore::rolling_engine engine{
        fincore::make_series({1, 2, 3, 4}, "returns", {0.01, 0.02, -0.01, 0.03}),
        std::nullopt, 3U};
    const std::array metrics{fincore::rolling_metric::mean_return};
    const auto result = engine.compute(metrics);

    EXPECT_EQ(result.at("mean_return").get_index(),
              (std::vector<fincore::timestamp>{3, 4}));
    const auto& mean_values = result.at("mean_return").get_column<double>(
        fincore::returns_column.data());
    EXPECT_NEAR(mean_values[0], 1.68, 1e-12);
    EXPECT_NEAR(mean_values[1], 3.36, 1e-12);
}

TEST(RollingEngine, RejectsInvalidWindowAndMissingBetaBenchmark) {
    const auto returns = fincore::make_series({1, 2}, "returns", {0.01, 0.02});
    EXPECT_THROW((fincore::rolling_engine{returns, std::nullopt, 1U}),
                 fincore::input_error);
    const fincore::rolling_engine engine{returns, std::nullopt, 2U};
    const std::array metrics{fincore::rolling_metric::beta};
    EXPECT_THROW(static_cast<void>(engine.compute(metrics)), fincore::input_error);
}

TEST(RollingEngine, ComputesAllSupportedMetricsAndRejectsUnknownEnumValues) {
    const auto returns =
        fincore::make_series({1, 2, 3}, "returns", {0.01, -0.02, 0.03});
    const auto benchmark =
        fincore::make_series({1, 2, 3}, "benchmark", {0.02, -0.04, 0.06});
    const fincore::rolling_engine engine{returns, benchmark, 3U};
    const std::array metrics{fincore::rolling_metric::volatility,
                             fincore::rolling_metric::sharpe,
                             fincore::rolling_metric::max_drawdown,
                             fincore::rolling_metric::beta};

    const auto result = engine.compute(metrics);
    EXPECT_EQ(result.size(), metrics.size());
    EXPECT_NEAR(result.at("max_drawdown").get_column<double>("returns")[0], -0.02,
                1e-12);

    const std::array invalid{static_cast<fincore::rolling_metric>(99)};
    EXPECT_THROW(static_cast<void>(engine.compute(invalid)), fincore::input_error);
}

}  // namespace
