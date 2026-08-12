#include <fincore/simulation/monte_carlo.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using monte_carlo_function = fincore::simulation_result (*) (
    const fincore::dataframe&, std::size_t, std::size_t, bool,
    std::optional<std::uint64_t>, std::string_view);

static_assert(std::is_same_v<decltype(&fincore::simulate_monte_carlo),
                             monte_carlo_function>);
static_assert(std::is_same_v<
              decltype(fincore::simulate_monte_carlo(
                  std::declval<const fincore::dataframe&>())),
              fincore::simulation_result>);

constexpr double tolerance = 1.0e-12;

fincore::dataframe constant_returns() {
  auto returns = fincore::make_series({19, -3, 19, 7}, "returns",
                                      {0.01, 0.01, 0.01, 0.01});
  return returns;
}

fincore::dataframe nonconstant_returns() {
  return fincore::make_series({42, -1, 42, 7, 0}, "strategy",
                              {0.01, -0.02, 0.03, 0.0, 0.015});
}

void expect_values(const fincore::dataframe& paths, const std::string_view name,
                   const std::vector<double>& expected) {
  const auto& actual = paths.get_column<double>(name.data());
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t row = 0U; row < expected.size(); ++row) {
    EXPECT_NEAR(actual[row], expected[row], tolerance) << name << " row " << row;
  }
}

void expect_statistics(const fincore::simulation_result& result,
                       const std::array<double, 10U>& expected) {
  const auto& statistics = result.statistics();
  EXPECT_NEAR(statistics.mean, expected[0], tolerance);
  EXPECT_NEAR(statistics.sample_standard_deviation, expected[1], tolerance);
  EXPECT_NEAR(statistics.median, expected[2], tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_1, expected[3], tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_5, expected[4], tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_25, expected[5], tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_50, expected[6], tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_75, expected[7], tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_95, expected[8], tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_99, expected[9], tolerance);
}

void expect_same_result(const fincore::simulation_result& actual,
                        const fincore::simulation_result& expected) {
  EXPECT_EQ(actual.paths().get_index(), expected.paths().get_index());
  EXPECT_EQ(actual.paths().shape(), expected.paths().shape());
  const auto actual_columns = actual.paths().get_columns_info<double>();
  const auto expected_columns = expected.paths().get_columns_info<double>();
  ASSERT_EQ(actual_columns.size(), expected_columns.size());
  for (std::size_t column = 0U; column < actual_columns.size(); ++column) {
    const auto& name = std::get<0>(expected_columns[column]);
    EXPECT_EQ(std::get<0>(actual_columns[column]), name);
    EXPECT_EQ(actual.paths().get_column<double>(name.c_str()),
              expected.paths().get_column<double>(name.c_str()));
  }
  EXPECT_EQ(actual.path_count(), expected.path_count());
  EXPECT_EQ(actual.observation_count(), expected.observation_count());
  const auto& actual_stats = actual.statistics();
  const auto& expected_stats = expected.statistics();
  EXPECT_NEAR(actual_stats.mean, expected_stats.mean, tolerance);
  EXPECT_NEAR(actual_stats.sample_standard_deviation,
              expected_stats.sample_standard_deviation, tolerance);
  EXPECT_NEAR(actual_stats.median, expected_stats.median, tolerance);
  EXPECT_NEAR(actual.terminal_value_at_risk(),
              expected.terminal_value_at_risk(), tolerance);
  EXPECT_NEAR(actual.terminal_conditional_value_at_risk(),
              expected.terminal_conditional_value_at_risk(), tolerance);
}

TEST(MonteCarloSimulation, MatchesConstantPythonOracleWithNativeTranspose) {
  const auto result = fincore::simulate_monte_carlo(
      constant_returns(), 2U, 4U, false, 42U);
  const std::vector<double> expected{3.968332704484823e-05,
                                     7.936822885623585e-05,
                                     0.00011905470549655739,
                                     0.0001587427570282074};

  EXPECT_EQ(result.paths().get_index(),
            (std::vector<fincore::timestamp>{1, 2, 3, 4}));
  EXPECT_EQ(result.path_count(), 2U);
  EXPECT_EQ(result.observation_count(), 4U);
  EXPECT_EQ(result.paths().shape(), (std::pair<std::size_t, std::size_t>{4U, 2U}));
  expect_values(result.paths(), "path_0", expected);
  expect_values(result.paths(), "path_1", expected);
  expect_statistics(result, {0.0001587427570282074, 0.0,
                             0.0001587427570282074, 0.0001587427570282074,
                             0.0001587427570282074, 0.0001587427570282074,
                             0.0001587427570282074, 0.0001587427570282074,
                             0.0001587427570282074, 0.0001587427570282074});
  EXPECT_NEAR(result.terminal_value_at_risk(), 0.0001587427570282074, tolerance);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(),
              0.0001587427570282074, tolerance);
}

TEST(MonteCarloSimulation, AppendsAntitheticPathsAndMatchesConstantOracle) {
  const auto result = fincore::simulate_monte_carlo(
      constant_returns(), 2U, 4U, true, 42U);
  const std::vector<double> positive{3.968332704484823e-05,
                                     7.936822885623585e-05,
                                     0.00011905470549655739,
                                     0.0001587427570282074};
  const std::vector<double> negative{3.968332704484823e-05,
                                     -1.5747665393917032e-09,
                                     -3.9688051406860936e-05,
                                     -7.937610293851094e-05};

  EXPECT_EQ(result.paths().get_index(),
            (std::vector<fincore::timestamp>{1, 2, 3, 4}));
  EXPECT_EQ(result.path_count(), 4U);
  EXPECT_EQ(result.observation_count(), 4U);
  expect_values(result.paths(), "path_0", positive);
  expect_values(result.paths(), "path_1", positive);
  expect_values(result.paths(), "antithetic_path_0", negative);
  expect_values(result.paths(), "antithetic_path_1", negative);
  expect_statistics(result, {3.968332704484823e-05, 0.00013747798790091163,
                             3.968332704484823e-05, -7.937610293851094e-05,
                             -7.937610293851094e-05, -7.937610293851094e-05,
                             3.968332704484823e-05, 0.0001587427570282074,
                             0.0001587427570282074, 0.0001587427570282074});
  EXPECT_NEAR(result.terminal_value_at_risk(), -7.937610293851094e-05, tolerance);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(),
              -7.937610293851094e-05, tolerance);
}

TEST(MonteCarloSimulation, ComposesTheAcceptedPublicLeavesAndSeedSemantics) {
  const auto returns = nonconstant_returns();
  const auto direct = fincore::analyze_simulation_paths(
      fincore::gbm_from_returns(returns, 6U, 3U, 252U, 918U, "strategy"));
  const auto direct_antithetic = fincore::analyze_simulation_paths(
      fincore::antithetic_variates(
          fincore::gbm_from_returns(returns, 6U, 3U, 252U, 918U, "strategy")));
  const auto actual = fincore::simulate_monte_carlo(
      returns, 3U, 6U, false, 918U, "strategy");
  const auto actual_antithetic = fincore::simulate_monte_carlo(
      returns, 3U, 6U, true, 918U, "strategy");

  expect_same_result(actual, direct);
  expect_same_result(actual_antithetic, direct_antithetic);

  const auto repeated = fincore::simulate_monte_carlo(
      returns, 3U, 6U, false, 918U, "strategy");
  const auto distinct = fincore::simulate_monte_carlo(
      returns, 3U, 6U, false, 919U, "strategy");
  EXPECT_EQ(actual.paths().get_column<double>("path_0"),
            repeated.paths().get_column<double>("path_0"));
  EXPECT_NE(actual.paths().get_column<double>("path_0"),
            distinct.paths().get_column<double>("path_0"));
}

TEST(MonteCarloSimulation, PropagatesAcceptedLeafInputErrors) {
  fincore::dataframe missing;
  missing.load_index(std::vector<fincore::timestamp>{1, 2});
  EXPECT_THROW(static_cast<void>(fincore::simulate_monte_carlo(missing)),
               fincore::input_error);

  fincore::dataframe wrong;
  wrong.load_index(std::vector<fincore::timestamp>{1, 2});
  wrong.load_column<int>("returns", {1, 2}, hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(fincore::simulate_monte_carlo(wrong)),
               fincore::input_error);

  fincore::dataframe malformed;
  malformed.load_index(std::vector<fincore::timestamp>{1, 2});
  malformed.load_column<double>("returns", {0.01},
                                hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(fincore::simulate_monte_carlo(malformed)),
               fincore::input_error);

  constexpr std::array<char, 12U> nul_name{
      'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l'};
  EXPECT_THROW(static_cast<void>(fincore::simulate_monte_carlo(
                   constant_returns(), 2U, 4U, false, 42U,
                   std::string_view{nul_name.data(), nul_name.size()})),
               fincore::input_error);

  const auto all_nan = fincore::make_series(
      {1, 2}, "returns", {std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::quiet_NaN()});
  const auto one_retained = fincore::make_series(
      {1, 2}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN()});
  const auto infinity = fincore::make_series(
      {1, 2}, "returns", {0.01, std::numeric_limits<double>::infinity()});
  EXPECT_THROW(static_cast<void>(fincore::simulate_monte_carlo(all_nan)),
               fincore::input_error);
  EXPECT_THROW(static_cast<void>(fincore::simulate_monte_carlo(one_retained)),
               fincore::input_error);
  EXPECT_THROW(static_cast<void>(fincore::simulate_monte_carlo(infinity)),
               fincore::input_error);
  EXPECT_THROW(static_cast<void>(
                   fincore::simulate_monte_carlo(constant_returns(), 0U)),
               fincore::input_error);
  EXPECT_THROW(static_cast<void>(fincore::simulate_monte_carlo(
                   constant_returns(), std::numeric_limits<std::size_t>::max(),
                   4U)),
               fincore::input_error);
  EXPECT_THROW(static_cast<void>(
                   fincore::simulate_monte_carlo(constant_returns(), 2U, 0U)),
               fincore::input_error);
}

TEST(MonteCarloSimulation, ResultOwnsPathsStatisticsAndRiskAfterSourceMutation) {
  const auto result = [] {
    auto returns = constant_returns();
    const auto simulated = fincore::simulate_monte_carlo(
        returns, 2U, 4U, true, 42U);
    returns.get_column<double>("returns")[0] = -0.75;
    returns.load_index(std::vector<fincore::timestamp>{99, 98, 97, 96});
    return simulated;
  }();

  EXPECT_EQ(result.paths().get_index(),
            (std::vector<fincore::timestamp>{1, 2, 3, 4}));
  EXPECT_EQ(result.paths().get_column<double>("path_0")[3],
            0.0001587427570282074);
  EXPECT_NEAR(result.statistics().mean, 3.968332704484823e-05, tolerance);
  EXPECT_NEAR(result.terminal_value_at_risk(), -7.937610293851094e-05, tolerance);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(),
              -7.937610293851094e-05, tolerance);
}

}  // namespace
