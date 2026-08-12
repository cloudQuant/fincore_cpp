#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
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

TEST(MonteCarloSimulationUmbrella,
     ExportsTheValueOwningSimulationComposerWithoutExtraColumns) {
  const auto returns = fincore::make_series(
      {19, -3, 19, 7}, "returns", {0.01, 0.01, 0.01, 0.01});
  const auto result = fincore::simulate_monte_carlo(
      returns, 2U, 4U, true, 42U);
  const auto& paths = result.paths();

  EXPECT_EQ(paths.get_index(), (std::vector<fincore::timestamp>{1, 2, 3, 4}));
  EXPECT_EQ(paths.shape(), (std::pair<std::size_t, std::size_t>{4U, 4U}));
  const auto columns = paths.get_columns_info<double>();
  ASSERT_EQ(columns.size(), 4U);
  EXPECT_EQ(paths.shape().second, columns.size());
  const std::array<std::string, 4U> names{
      "path_0", "path_1", "antithetic_path_0", "antithetic_path_1"};
  for (std::size_t column = 0U; column < names.size(); ++column) {
    EXPECT_EQ(std::get<0>(columns[column]), names[column].c_str());
    EXPECT_EQ(std::get<1>(columns[column]), 4U);
    EXPECT_EQ(std::get<2>(columns[column]), typeid(double));
  }

  EXPECT_EQ(result.path_count(), 4U);
  EXPECT_EQ(result.observation_count(), 4U);
  const auto& statistics = result.statistics();
  EXPECT_NEAR(statistics.mean, 3.968332704484823e-05, tolerance);
  EXPECT_NEAR(statistics.sample_standard_deviation, 0.00013747798790091163,
              tolerance);
  EXPECT_NEAR(statistics.median, 3.968332704484823e-05, tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_1, -7.937610293851094e-05,
              tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_5, -7.937610293851094e-05,
              tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_25, -7.937610293851094e-05,
              tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_50, 3.968332704484823e-05,
              tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_75, 0.0001587427570282074,
              tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_95, 0.0001587427570282074,
              tolerance);
  EXPECT_NEAR(statistics.percentiles.percentile_99, 0.0001587427570282074,
              tolerance);
  EXPECT_NEAR(result.terminal_value_at_risk(), -7.937610293851094e-05,
              tolerance);
  EXPECT_NEAR(result.terminal_conditional_value_at_risk(),
              -7.937610293851094e-05, tolerance);
}

}  // namespace
