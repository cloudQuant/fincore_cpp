#include <fincore/simulation/parameters.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

constexpr double tolerance = 1.0e-12;

using parameter_estimation_function = fincore::simulation_parameters (*) (
    const fincore::dataframe&, std::size_t, std::string_view);

static_assert(std::is_same_v<decltype(&fincore::estimate_simulation_parameters),
                             parameter_estimation_function>);
static_assert(std::is_same_v<decltype(fincore::simulation_parameters::drift),
                             double>);
static_assert(
    std::is_same_v<decltype(fincore::simulation_parameters::volatility), double>);

fincore::dataframe oracle_returns() {
    return fincore::make_series(
        {42, -5, 42, 0, 8}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), -0.02, 0.03, 0.0});
}

fincore::dataframe finite_oracle_returns() {
    return fincore::make_series({9, 7, 7, -3}, "returns",
                                {0.01, -0.02, 0.03, 0.0});
}

TEST(SimulationParameters, MatchesPythonSourceOracle) {
    const auto parameters = fincore::estimate_simulation_parameters(oracle_returns());

    EXPECT_NEAR(parameters.drift, 1.2599999999999998, tolerance);
    EXPECT_NEAR(parameters.volatility, 0.3304542328371662, tolerance);
}

TEST(SimulationParameters, RemovesOnlyNansAndRetainsFiniteValuesInOrder) {
    const auto nan_mixed = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7}, "returns",
        {std::numeric_limits<double>::quiet_NaN(), 0.01,
         std::numeric_limits<double>::quiet_NaN(), -0.02, 0.03, 0.0,
         std::numeric_limits<double>::quiet_NaN()});

    const auto actual = fincore::estimate_simulation_parameters(nan_mixed);
    const auto expected = fincore::estimate_simulation_parameters(finite_oracle_returns());

    EXPECT_NEAR(actual.drift, expected.drift, tolerance);
    EXPECT_NEAR(actual.volatility, expected.volatility, tolerance);
}

TEST(SimulationParameters, SupportsCustomBoundedColumnAndNonDefaultPeriods) {
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{8, 0, 8, -5, 42});
    returns.load_column<int>("ignored", {1, 2, 3, 4, 5},
                             hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("strategy", {0.01,
                                               std::numeric_limits<double>::quiet_NaN(),
                                               -0.02, 0.03, 0.0},
                                hmdf::nan_policy::dont_pad_with_nans);
    constexpr std::array<char, 8U> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view column{bounded_name.data(), bounded_name.size()};

    const auto parameters =
        fincore::estimate_simulation_parameters(returns, 12U, column);

    EXPECT_NEAR(parameters.drift, 0.06, tolerance);
    EXPECT_NEAR(parameters.volatility, 0.07211102550927978, tolerance);
}

TEST(SimulationParameters, IgnoresRawTimestampOrder) {
    const auto ordered = fincore::make_series({1, 2, 3, 4}, "returns",
                                              {0.01, -0.02, 0.03, 0.0});
    const auto irregular_index = fincore::make_series(
        {42, -100, 42, 0}, "returns", {0.01, -0.02, 0.03, 0.0});

    const auto expected = fincore::estimate_simulation_parameters(ordered);
    const auto actual = fincore::estimate_simulation_parameters(irregular_index);

    EXPECT_NEAR(actual.drift, expected.drift, tolerance);
    EXPECT_NEAR(actual.volatility, expected.volatility, tolerance);
}

TEST(SimulationParameters, UsesSampleRatherThanPopulationDeviation) {
    const auto returns = fincore::make_series({1, 2}, "returns", {0.0, 2.0});

    const auto parameters = fincore::estimate_simulation_parameters(returns, 1U);

    EXPECT_DOUBLE_EQ(parameters.drift, 1.0);
    EXPECT_NEAR(parameters.volatility, std::sqrt(2.0), tolerance);
    // This last check is what separates the sample denominator from the population
    // one: the excluded value is the one the population denominator would give.
    EXPECT_NE(parameters.volatility, 1.0);
}

TEST(SimulationParameters, RejectsInvalidSelectedColumnsAndInsufficientData) {
    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{1, 2});
    EXPECT_THROW(fincore::estimate_simulation_parameters(missing), fincore::input_error);

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1, 2});
    wrong_type.load_column<int>("returns", {1, 2},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::estimate_simulation_parameters(wrong_type),
                 fincore::input_error);

    fincore::dataframe short_column;
    short_column.load_index(std::vector<fincore::timestamp>{1, 2});
    short_column.load_column<double>("returns", {0.01},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::estimate_simulation_parameters(short_column),
                 fincore::input_error);

    const auto empty = fincore::make_series({}, "returns", {});
    EXPECT_THROW(fincore::estimate_simulation_parameters(empty), fincore::input_error);

    const auto all_nan = fincore::make_series(
        {1, 2}, "returns", {std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(fincore::estimate_simulation_parameters(all_nan),
                 fincore::input_error);

    const auto single_retained = fincore::make_series(
        {1, 2}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(fincore::estimate_simulation_parameters(single_retained),
                 fincore::input_error);
}

TEST(SimulationParameters, RejectsInfinitiesInvalidPeriodsAndNonfiniteArithmetic) {
    for (const double infinite : {std::numeric_limits<double>::infinity(),
                                  -std::numeric_limits<double>::infinity()}) {
        const auto returns = fincore::make_series({1, 2}, "returns", {0.01, infinite});
        EXPECT_THROW(fincore::estimate_simulation_parameters(returns),
                     fincore::input_error);
    }

    EXPECT_THROW(fincore::estimate_simulation_parameters(finite_oracle_returns(), 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::estimate_simulation_parameters(finite_oracle_returns(), 252U,
                                                          ""),
                 fincore::input_error);
    constexpr std::array<char, 12U> embedded_nul_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l'};
    const std::string_view embedded_nul_column{embedded_nul_storage.data(),
                                               embedded_nul_storage.size()};
    EXPECT_THROW(fincore::estimate_simulation_parameters(finite_oracle_returns(), 252U,
                                                          embedded_nul_column),
                 fincore::input_error);

    const double maximum = std::numeric_limits<double>::max();
    const auto annualization_overflow =
        fincore::make_series({1, 2}, "returns", {maximum / 2.0, maximum / 2.0});
    EXPECT_THROW(fincore::estimate_simulation_parameters(annualization_overflow, 4U),
                 fincore::input_error);

    const auto variance_overflow =
        fincore::make_series({1, 2}, "returns", {maximum, -maximum});
    EXPECT_THROW(fincore::estimate_simulation_parameters(variance_overflow, 1U),
                 fincore::input_error);
}

}  // namespace

namespace {

TEST(SimulationParameters, AnnualizesWithSourcePeriodFactors) {
    EXPECT_DOUBLE_EQ(fincore::annualize(0.01), 0.01 * std::sqrt(252.0));
    EXPECT_DOUBLE_EQ(fincore::annualize(0.02, "weekly"),
                     0.02 * std::sqrt(52.0));
    EXPECT_DOUBLE_EQ(fincore::annualize(0.03, "MONTHLY"),
                     0.03 * std::sqrt(12.0));
    EXPECT_DOUBLE_EQ(fincore::annualize(0.04, "yearly"), 0.04);
    // "hourly" is not a recognised period, so it silently takes the default daily
    // factor instead of being rejected as a typo would be.
    EXPECT_DOUBLE_EQ(fincore::annualize(0.01, "hourly"),
                     0.01 * std::sqrt(252.0));
}

}  // namespace
