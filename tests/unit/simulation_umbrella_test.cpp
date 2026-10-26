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
    // The crash scenario replaces the upper-middle observation with a fixed
    // -0.20 rather than scaling it, so a single-row frame lands on exactly -0.20
    // whatever the input was.
    EXPECT_EQ(results[0].name, "crash");
    EXPECT_DOUBLE_EQ(results[0].stressed_returns.get_column<double>("strategy")[0],
                     -0.20);

    const auto summary = fincore::scenario_table(results);
    // The reported name is the scenario's own label, not the enum spelling:
    // volatility_crush is published as "vol_crush", and the table keeps that.
    EXPECT_EQ(summary.get_column<std::string>("scenario")[1], "vol_crush");
    EXPECT_DOUBLE_EQ(summary.get_column<double>("cumulative_return")[0], -0.20);
}
