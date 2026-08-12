#include <fincore/simulation/result.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace {

using analyze_simulation_paths_function =
    fincore::simulation_result (*)(const fincore::dataframe &);

static_assert(std::is_same_v<decltype(&fincore::analyze_simulation_paths),
                             analyze_simulation_paths_function>);

fincore::dataframe oracle_paths() {
  fincore::dataframe paths;
  paths.load_index(std::vector<fincore::timestamp>{9, 3, 9});
  paths.load_column<double>("path_0", std::vector<double>{100.0, 95.0, 80.0},
                            hmdf::nan_policy::dont_pad_with_nans);
  paths.load_column<double>("path_1", std::vector<double>{100.0, 105.0, 110.0},
                            hmdf::nan_policy::dont_pad_with_nans);
  paths.load_column<double>("path_2", std::vector<double>{100.0, 110.0, 130.0},
                            hmdf::nan_policy::dont_pad_with_nans);
  paths.load_column<double>("path_3", std::vector<double>{100.0, 120.0, 190.0},
                            hmdf::nan_policy::dont_pad_with_nans);
  return paths;
}

void expect_exact_path_schema(const fincore::dataframe &paths,
                              const std::size_t expected_rows,
                              const std::vector<std::string> &expected_names) {
  ASSERT_EQ(paths.shape().first, expected_rows);
  ASSERT_EQ(paths.shape().second, expected_names.size());

  const auto columns = paths.get_columns_info<double>();
  ASSERT_EQ(columns.size(), expected_names.size());
  for (std::size_t column = 0U; column < expected_names.size(); ++column) {
    EXPECT_EQ(std::get<0>(columns[column]), expected_names[column].c_str());
    EXPECT_EQ(std::get<1>(columns[column]), expected_rows);
    EXPECT_EQ(std::get<2>(columns[column]), typeid(double));
  }
}

void expect_statistics(
    const fincore::simulation_terminal_statistics &statistics) {
  EXPECT_NEAR(statistics.mean, 127.5, 1e-12);
  EXPECT_NEAR(statistics.sample_standard_deviation, 46.45786621588785, 1e-12);
  EXPECT_NEAR(statistics.median, 120.0, 1e-12);
  EXPECT_NEAR(statistics.percentiles.percentile_1, 80.9, 1e-12);
  EXPECT_NEAR(statistics.percentiles.percentile_5, 84.5, 1e-12);
  EXPECT_NEAR(statistics.percentiles.percentile_25, 102.5, 1e-12);
  EXPECT_NEAR(statistics.percentiles.percentile_50, 120.0, 1e-12);
  EXPECT_NEAR(statistics.percentiles.percentile_75, 145.0, 1e-12);
  EXPECT_NEAR(statistics.percentiles.percentile_95, 181.0, 1e-12);
  EXPECT_NEAR(statistics.percentiles.percentile_99, 188.2, 1e-12);
}

TEST(SimulationResult, AnalyzesNativePathsWithSourceTerminalStatistics) {
  const auto result = fincore::analyze_simulation_paths(oracle_paths());

  EXPECT_EQ(result.path_count(), 4U);
  EXPECT_EQ(result.observation_count(), 3U);
  EXPECT_EQ(result.paths().get_index(),
            (std::vector<fincore::timestamp>{9, 3, 9}));
  expect_exact_path_schema(result.paths(), 3U,
                           {"path_0", "path_1", "path_2", "path_3"});
  EXPECT_EQ(result.paths().get_column<double>("path_0"),
            (std::vector<double>{100.0, 95.0, 80.0}));
  EXPECT_EQ(result.paths().get_column<double>("path_3"),
            (std::vector<double>{100.0, 120.0, 190.0}));
  expect_statistics(result.statistics());

  EXPECT_NEAR(result.terminal_value_at_risk(0.01), 80.9, 1e-12);
  EXPECT_NEAR(result.terminal_value_at_risk(0.05), 84.5, 1e-12);
  EXPECT_NEAR(result.terminal_value_at_risk(0.5), 120.0, 1e-12);
  EXPECT_NEAR(result.terminal_value_at_risk(0.95), 181.0, 1e-12);
  EXPECT_NEAR(result.terminal_value_at_risk(0.99), 188.2, 1e-12);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(0.01), 80.0, 1e-12);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(0.05), 80.0, 1e-12);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(0.5), 95.0, 1e-12);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(0.95),
              106.66666666666667, 1e-12);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(0.99),
              106.66666666666667, 1e-12);
}

TEST(SimulationResult, PreservesSingletonAndInvalidAlphaSourceBoundaries) {
  fincore::dataframe paths;
  paths.load_index(std::vector<fincore::timestamp>{42});
  paths.load_column<double>("only_path", std::vector<double>{1.0},
                            hmdf::nan_policy::dont_pad_with_nans);

  const auto result = fincore::analyze_simulation_paths(paths);

  EXPECT_EQ(result.path_count(), 1U);
  EXPECT_EQ(result.observation_count(), 1U);
  EXPECT_TRUE(std::isnan(result.statistics().sample_standard_deviation));
  EXPECT_EQ(result.statistics().mean, 1.0);
  EXPECT_EQ(result.statistics().median, 1.0);
  EXPECT_EQ(result.statistics().percentiles.percentile_1, 1.0);
  EXPECT_EQ(result.statistics().percentiles.percentile_99, 1.0);
  EXPECT_EQ(result.terminal_value_at_risk(), 1.0);
  EXPECT_EQ(result.terminal_conditional_value_at_risk(), 1.0);

  for (const double alpha : {0.0, 1.0, std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity()}) {
    EXPECT_TRUE(std::isnan(result.terminal_value_at_risk(alpha)));
    EXPECT_TRUE(std::isnan(result.terminal_conditional_value_at_risk(alpha)));
  }
}

TEST(SimulationResult,
     RetainsNonterminalIeeeValuesButRejectsNonfiniteTerminals) {
  fincore::dataframe valid;
  valid.load_index(std::vector<fincore::timestamp>{1, 2});
  valid.load_column<double>(
      "path_0",
      std::vector<double>{std::numeric_limits<double>::quiet_NaN(), 80.0},
      hmdf::nan_policy::dont_pad_with_nans);
  valid.load_column<double>(
      "path_1",
      std::vector<double>{std::numeric_limits<double>::infinity(), 110.0},
      hmdf::nan_policy::dont_pad_with_nans);
  const auto result = fincore::analyze_simulation_paths(valid);
  EXPECT_TRUE(std::isnan(result.paths().get_column<double>("path_0")[0]));
  EXPECT_EQ(result.paths().get_column<double>("path_1")[0],
            std::numeric_limits<double>::infinity());
  EXPECT_NEAR(result.statistics().mean, 95.0, 1e-12);

  auto terminal_nan = valid;
  terminal_nan.get_column<double>("path_0")[1] =
      std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW(
      static_cast<void>(fincore::analyze_simulation_paths(terminal_nan)),
      fincore::input_error);

  auto terminal_infinity = valid;
  terminal_infinity.get_column<double>("path_1")[1] =
      -std::numeric_limits<double>::infinity();
  EXPECT_THROW(
      static_cast<void>(fincore::analyze_simulation_paths(terminal_infinity)),
      fincore::input_error);
}

TEST(SimulationResult, RejectsStructuralAndColumnBoundaries) {
  fincore::dataframe empty_rows;
  empty_rows.load_index(std::vector<fincore::timestamp>{});
  empty_rows.load_column<double>("path", std::vector<double>{},
                                 hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(fincore::analyze_simulation_paths(empty_rows)),
               fincore::input_error);

  fincore::dataframe no_columns;
  no_columns.load_index(std::vector<fincore::timestamp>{1});
  EXPECT_THROW(static_cast<void>(fincore::analyze_simulation_paths(no_columns)),
               fincore::input_error);

  fincore::dataframe only_integer;
  only_integer.load_index(std::vector<fincore::timestamp>{1});
  only_integer.load_column<int>("path", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(
      static_cast<void>(fincore::analyze_simulation_paths(only_integer)),
      fincore::input_error);

  auto mixed = oracle_paths();
  mixed.load_column<int>("metadata", std::vector<int>{1, 2, 3},
                         hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(fincore::analyze_simulation_paths(mixed)),
               fincore::input_error);

  fincore::dataframe malformed;
  malformed.load_index(std::vector<fincore::timestamp>{1, 2});
  malformed.load_column<double>("path", std::vector<double>{1.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(fincore::analyze_simulation_paths(malformed)),
               fincore::input_error);
}

TEST(SimulationResult, OwnsPathsStatisticsAndTerminalDistribution) {
  const auto result = [] {
    auto paths = oracle_paths();
    const auto analyzed = fincore::analyze_simulation_paths(paths);
    paths.get_column<double>("path_0")[2] = -999.0;
    paths.get_column<double>("path_3")[2] = -888.0;
    paths.load_index(std::vector<fincore::timestamp>{1, 2, 3});
    return analyzed;
  }();

  EXPECT_EQ(result.paths().get_index(),
            (std::vector<fincore::timestamp>{9, 3, 9}));
  EXPECT_EQ(result.paths().get_column<double>("path_0")[2], 80.0);
  EXPECT_EQ(result.paths().get_column<double>("path_3")[2], 190.0);
  expect_statistics(result.statistics());
  EXPECT_NEAR(result.terminal_value_at_risk(0.95), 181.0, 1e-12);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(0.95),
              106.66666666666667, 1e-12);
}

} // namespace
