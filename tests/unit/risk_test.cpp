#include <fincore/metrics/risk.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

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

TEST(Risk, GpdRiskEstimatesMatchPythonOracleWithinTolerance) {
    std::vector<fincore::timestamp> index;
    index.reserve(200);
    std::vector<double> values;
    values.reserve(200);
    for (int position = 0; position < 200; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
        values.push_back(0.01 * std::sin(static_cast<double>(position) * 0.7) +
                         0.004 * std::cos(static_cast<double>(position) * 0.31));
    }
    for (const auto& shock :
         std::vector<std::pair<int, double>>{{10, -0.08}, {40, -0.10},
                                             {75, -0.07}, {120, -0.09},
                                             {160, -0.11}}) {
        values[static_cast<std::size_t>(shock.first)] = shock.second;
    }
    const auto frame = fincore::make_series(index, "returns", values);
    const auto result = fincore::gpd_risk_estimates(frame);

    EXPECT_DOUBLE_EQ(result.threshold, 0.1);
    EXPECT_NEAR(result.scale, 0.10505964641693998, 1e-2);
    EXPECT_NEAR(result.shape, 7.484505235073074e-07, 1e-3);
    EXPECT_NEAR(result.var, 0.16571361940540646, 1e-2);
    EXPECT_NEAR(result.es, 0.2707733936377823, 1e-2);
    EXPECT_GT(result.shape, 0.0);
    EXPECT_GT(result.var, result.threshold);
    EXPECT_GT(result.es, result.var);

    const auto short_frame =
        fincore::make_series({1}, "returns", {0.01});
    const auto zero = fincore::gpd_risk_estimates(short_frame);
    EXPECT_DOUBLE_EQ(zero.threshold, 0.0);
    EXPECT_DOUBLE_EQ(zero.var, 0.0);
    EXPECT_DOUBLE_EQ(zero.es, 0.0);

    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::gpd_risk_estimates(
                     frame, 0.01, embedded_selector)),
                 fincore::input_error);
}

TEST(Risk, BetaFragilityHeuristicMatchesPythonOracle) {
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto benchmark = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "benchmark",
        {-0.04, -0.03, -0.02, -0.01, -0.005, 0.005, 0.01, 0.02, 0.03, 0.04});
    EXPECT_NEAR(fincore::beta_fragility_heuristic(ordinary, benchmark),
                0.012499999999999999, 1e-15);

    const auto short_returns =
        fincore::make_series({1, 2}, "returns", {0.01, 0.02});
    const auto short_benchmark =
        fincore::make_series({1, 2}, "benchmark", {-0.01, 0.02});
    EXPECT_TRUE(std::isnan(fincore::beta_fragility_heuristic(
        short_returns, short_benchmark)));

    const auto nan_returns = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, nan, -0.05, 0.03});
    const auto nan_benchmark = fincore::make_series(
        {1, 2, 3, 4}, "benchmark", {-0.04, -0.03, nan, 0.02});
    EXPECT_NEAR(fincore::beta_fragility_heuristic(nan_returns, nan_benchmark),
                -0.019999999999999997, 1e-15);

    const auto constant_returns = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, 0.02, 0.03, 0.04});
    const auto constant_benchmark = fincore::make_series(
        {1, 2, 3, 4}, "benchmark", {0.01, 0.01, 0.01, 0.01});
    EXPECT_NEAR(fincore::beta_fragility_heuristic(constant_returns,
                                                  constant_benchmark),
                -0.0049999999999999975, 1e-15);

    const auto two_clean_returns = fincore::make_series(
        {1, 2, 3}, "returns", {0.01, nan, 0.03});
    const auto two_clean_benchmark = fincore::make_series(
        {1, 2, 3}, "benchmark", {-0.02, 0.0, 0.02});
    EXPECT_NEAR(fincore::beta_fragility_heuristic(two_clean_returns,
                                                  two_clean_benchmark),
                -0.019999999999999997, 1e-15);

    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::beta_fragility_heuristic(
                     ordinary, benchmark, embedded_selector, "benchmark")),
                 fincore::input_error);
}

}  // namespace
