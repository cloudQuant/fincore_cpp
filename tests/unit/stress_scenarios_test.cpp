#include <fincore/simulation/scenarios.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <tuple>
#include <typeindex>
#include <vector>

namespace {

constexpr double tolerance = 1.0e-12;

fincore::dataframe fixture() {
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{42, 5, 5, -1});
    returns.load_column<double>("strategy", {0.01, -0.02, 0.03, -0.01},
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<int>("ignored_metadata", {7},
                             hmdf::nan_policy::dont_pad_with_nans);
    return returns;
}

void expect_values(const std::vector<double>& actual,
                   const std::vector<double>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t position = 0; position < expected.size(); ++position) {
        if (std::isnan(expected[position])) {
            EXPECT_TRUE(std::isnan(actual[position])) << "position " << position;
        } else if (std::isinf(expected[position])) {
            EXPECT_EQ(actual[position], expected[position]) << "position " << position;
        } else {
            EXPECT_NEAR(actual[position], expected[position], tolerance)
                << "position " << position;
        }
    }
}

void expect_summary(const fincore::stress_scenario_result& actual,
                    const std::string_view name,
                    const std::vector<double>& values,
                    const double cumulative,
                    const double drawdown,
                    const double volatility,
                    const double final_value) {
    EXPECT_EQ(actual.name, name);
    EXPECT_EQ(actual.stressed_returns.get_index(),
              (std::vector<fincore::timestamp>{42, 5, 5, -1}));
    EXPECT_EQ(actual.stressed_returns.shape().second, 1U);
    expect_values(actual.stressed_returns.get_column<double>("strategy"), values);
    EXPECT_NEAR(actual.cumulative_return, cumulative, tolerance);
    EXPECT_NEAR(actual.max_drawdown, drawdown, tolerance);
    EXPECT_NEAR(actual.volatility, volatility, tolerance);
    EXPECT_NEAR(actual.final_value, final_value, tolerance);
    EXPECT_NEAR(actual.final_value, 1.0 + actual.cumulative_return, tolerance);
}

TEST(StressScenarios, MatchesPythonGoldensAndPreservesDefaultSourceOrder) {
    const auto results = fincore::stress_test(fixture(), "strategy");

    ASSERT_EQ(results.size(), 4U);
    expect_summary(results[0], "crash", {0.01, -0.02, -0.20, -0.01},
                   -0.2160784, -0.22384, 1.5472556349873154, 0.7839216);
    expect_summary(results[1], "spike", {0.01, -0.02, 0.10, -0.01},
                   0.0778922, -0.02, 0.8694826047713664, 1.0778922);
    expect_summary(results[2], "vol_crush", {0.005, -0.01, 0.015, -0.005},
                   0.004824878749999817, -0.01, 0.1759971590679804,
                   1.0048248787499998);
    expect_summary(results[3], "vol_spike", {0.02, -0.04, 0.06, -0.02},
                   0.017192960000000035, -0.04, 0.7039886362719217,
                   1.01719296);
}

TEST(StressScenarios, PreservesExplicitAndCustomOrderAndAppliesShiftBeforeMultiplier) {
    const fincore::predefined_stress_scenario predefined[]{
        fincore::predefined_stress_scenario::volatility_spike,
        fincore::predefined_stress_scenario::crash};
    const fincore::custom_stress_scenario custom[]{
        {"shift_then_mult", 0.01, 1.5}, {"zero", 0.0, 0.0}};

    const auto results = fincore::stress_test(fixture(), predefined, custom, "strategy");

    ASSERT_EQ(results.size(), 4U);
    EXPECT_EQ(results[0].name, "vol_spike");
    EXPECT_EQ(results[1].name, "crash");
    expect_summary(results[2], "shift_then_mult", {0.03, -0.015, 0.06, 0.0},
                   0.07542300000000002, -0.015, 0.5279914772039412, 1.075423);
    expect_values(results[3].stressed_returns.get_column<double>("strategy"),
                  {0.0, 0.0, 0.0, 0.0});

    const std::span<const fincore::predefined_stress_scenario> no_predefined;
    const fincore::custom_stress_scenario custom_only[]{{"custom_only", -.01, 2.0}};
    const auto custom_results =
        fincore::stress_test(fixture(), no_predefined, custom_only, "strategy");
    ASSERT_EQ(custom_results.size(), 1U);
    EXPECT_EQ(custom_results.front().name, "custom_only");
    expect_values(custom_results.front().stressed_returns.get_column<double>("strategy"),
                  {0.0, -0.06, 0.04, -0.04});

    const std::span<const fincore::custom_stress_scenario> no_custom;
    EXPECT_TRUE(fincore::stress_test(fixture(), no_predefined, no_custom, "strategy")
                    .empty());
}

TEST(StressScenarios, ProducesTypedNumericScenarioTablesIncludingEmptyInput) {
    const auto results = fincore::stress_test(fixture(), "strategy");
    const auto table = fincore::scenario_table(results);

    EXPECT_EQ(table.get_index(),
              (std::vector<fincore::timestamp>{0, 1, 2, 3}));
    ASSERT_EQ(table.shape().second, 5U);
    const auto columns = table.get_columns_info<std::string, double>();
    ASSERT_EQ(columns.size(), 5U);
    const std::vector<std::string> expected_names{
        "scenario", "cumulative_return", "max_drawdown", "volatility", "final_value"};
    const std::vector<std::type_index> expected_types{
        typeid(std::string), typeid(double), typeid(double), typeid(double), typeid(double)};
    for (std::size_t position = 0; position < columns.size(); ++position) {
        EXPECT_EQ(std::get<0>(columns[position]), expected_names[position].c_str());
        EXPECT_EQ(std::get<2>(columns[position]), expected_types[position]);
    }
    EXPECT_EQ(table.get_column<std::string>("scenario"),
              (std::vector<std::string>{"crash", "spike", "vol_crush", "vol_spike"}));
    EXPECT_NEAR(table.get_column<double>("cumulative_return")[0],
                results[0].cumulative_return, tolerance);
    EXPECT_NEAR(table.get_column<double>("max_drawdown")[1],
                results[1].max_drawdown, tolerance);
    EXPECT_NEAR(table.get_column<double>("volatility")[2], results[2].volatility,
                tolerance);
    EXPECT_NEAR(table.get_column<double>("final_value")[3], results[3].final_value,
                tolerance);

    const std::vector<fincore::stress_scenario_result> empty_results;
    const auto empty = fincore::scenario_table(empty_results);
    EXPECT_TRUE(empty.get_index().empty());
    ASSERT_EQ(empty.shape().second, 5U);
    const auto empty_columns = empty.get_columns_info<std::string, double>();
    ASSERT_EQ(empty_columns.size(), 5U);
    for (std::size_t position = 0; position < empty_columns.size(); ++position) {
        EXPECT_EQ(std::get<0>(empty_columns[position]), expected_names[position].c_str());
        EXPECT_EQ(std::get<2>(empty_columns[position]), expected_types[position]);
    }
    EXPECT_TRUE(empty.get_column<std::string>("scenario").empty());
    EXPECT_TRUE(empty.get_column<double>("cumulative_return").empty());
}

TEST(StressScenarios, RejectsInvalidReturnColumnAndSelectedValues) {
    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{1});
    EXPECT_THROW(fincore::stress_test(missing, "strategy"), fincore::input_error);
    EXPECT_THROW(fincore::stress_test(fixture(), ""), fincore::input_error);

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("strategy", {1}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::stress_test(wrong_type, "strategy"), fincore::input_error);

    fincore::dataframe short_column;
    short_column.load_index(std::vector<fincore::timestamp>{1, 2});
    short_column.load_column<double>("strategy", {0.01},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::stress_test(short_column, "strategy"), fincore::input_error);

    const auto empty = fincore::make_series({}, "strategy", {});
    EXPECT_THROW(fincore::stress_test(empty, "strategy"), fincore::input_error);
    const auto nan = fincore::make_series(
        {1}, "strategy", {std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(fincore::stress_test(nan, "strategy"), fincore::input_error);
}

TEST(StressScenarios, RejectsInvalidScenarioConfiguration) {
    const auto returns = fixture();
    const fincore::predefined_stress_scenario duplicate_predefined[]{
        fincore::predefined_stress_scenario::crash,
        fincore::predefined_stress_scenario::crash};
    const fincore::predefined_stress_scenario unknown[]{
        static_cast<fincore::predefined_stress_scenario>(99)};
    const fincore::custom_stress_scenario empty_name[]{{"", 0.0, 1.0}};
    const fincore::custom_stress_scenario reserved_name[]{{"crash", 0.0, 1.0}};
    const fincore::custom_stress_scenario duplicate_name[]{{"same", 0.0, 1.0},
                                                            {"same", 1.0, 1.0}};
    const fincore::custom_stress_scenario infinite_parameter[]{{
        "bad", std::numeric_limits<double>::infinity(), 1.0}};
    const std::span<const fincore::custom_stress_scenario> no_custom;

    EXPECT_THROW(fincore::stress_test(returns, duplicate_predefined, no_custom,
                                      "strategy"),
                 fincore::input_error);
    EXPECT_THROW(fincore::stress_test(returns, unknown, no_custom, "strategy"),
                 fincore::input_error);
    EXPECT_THROW(fincore::stress_test(returns, {}, empty_name, "strategy"),
                 fincore::input_error);
    EXPECT_THROW(fincore::stress_test(returns, {}, reserved_name, "strategy"),
                 fincore::input_error);
    EXPECT_THROW(fincore::stress_test(returns, {}, duplicate_name, "strategy"),
                 fincore::input_error);
    EXPECT_THROW(fincore::stress_test(returns, {}, infinite_parameter, "strategy"),
                 fincore::input_error);
}

TEST(StressScenarios, RetainsSignedInfinityAndPublicVolatilityBoundary) {
    const auto infinity_returns = fincore::make_series(
        {2, 1}, "strategy", {0.01, -std::numeric_limits<double>::infinity()});
    const fincore::predefined_stress_scenario crush[]{
        fincore::predefined_stress_scenario::volatility_crush};
    const auto infinite = fincore::stress_test(infinity_returns, crush, {}, "strategy");
    ASSERT_EQ(infinite.size(), 1U);
    EXPECT_TRUE(std::isinf(
        infinite.front().stressed_returns.get_column<double>("strategy")[1]));
    EXPECT_LT(infinite.front().stressed_returns.get_column<double>("strategy")[1], 0.0);

    const auto one_row = fincore::make_series({4}, "strategy", {0.01});
    const auto single = fincore::stress_test(one_row, crush, {}, "strategy");
    ASSERT_EQ(single.size(), 1U);
    EXPECT_TRUE(std::isnan(single.front().volatility));
}

TEST(StressScenarios, ResultsOwnIndependentStressedFrames) {
    std::vector<fincore::stress_scenario_result> results;
    {
        auto returns = fixture();
        results = fincore::stress_test(returns, "strategy");
        returns.get_column<double>("strategy")[0] = 0.99;
    }
    ASSERT_EQ(results.size(), 4U);
    EXPECT_NEAR(results[0].stressed_returns.get_column<double>("strategy")[0], 0.01,
                tolerance);
    results[0].stressed_returns.get_column<double>("strategy")[0] = -0.75;
    EXPECT_NEAR(results[1].stressed_returns.get_column<double>("strategy")[0], 0.01,
                tolerance);
}

}  // namespace
