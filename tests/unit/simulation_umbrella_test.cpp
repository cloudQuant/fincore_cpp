#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

TEST(SimulationUmbrella, RunsStressApiAndBuildsNumericScenarioTable) {
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{7});
    returns.load_column<double>("strategy", {0.02},
                                hmdf::nan_policy::dont_pad_with_nans);

    constexpr std::array scenarios{
        fincore::predefined_stress_scenario::crash,
        fincore::predefined_stress_scenario::volatility_crush};
    const auto results = fincore::stress_test(returns, scenarios, {}, "strategy");

    ASSERT_EQ(results.size(), 2U);
    EXPECT_EQ(results[0].name, "crash");
    EXPECT_DOUBLE_EQ(results[0].stressed_returns.get_column<double>("strategy")[0],
                     -0.20);

    const auto summary = fincore::scenario_table(results);
    EXPECT_EQ(summary.get_column<std::string>("scenario")[1], "vol_crush");
    EXPECT_DOUBLE_EQ(summary.get_column<double>("cumulative_return")[0], -0.20);
}
