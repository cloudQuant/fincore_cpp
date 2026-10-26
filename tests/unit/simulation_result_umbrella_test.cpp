#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace {

using analyze_simulation_paths_function = fincore::simulation_result (*) (
    const fincore::dataframe&);

static_assert(std::is_same_v<decltype(&fincore::analyze_simulation_paths),
                             analyze_simulation_paths_function>);

TEST(SimulationResultUmbrella, ExportsValueOwningTerminalPathAnalysis) {
    fincore::dataframe paths;
    paths.load_index(std::vector<fincore::timestamp>{7, 3, 7});
    paths.load_column<double>("left", std::vector<double>{100.0, 95.0, 80.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    paths.load_column<double>("right", std::vector<double>{100.0, 105.0, 110.0},
                              hmdf::nan_policy::dont_pad_with_nans);

    const auto result = fincore::analyze_simulation_paths(paths);

    // Every statistic below is over the terminal value of each path — the last
    // row, so 80 and 110 — not over all six observations: the mean is 95 and the
    // sample deviation 21.213..., and the repeated 7 shows that "terminal" means
    // the final row by position rather than the largest timestamp.
    EXPECT_EQ(result.path_count(), 2U);
    EXPECT_EQ(result.observation_count(), 3U);
    EXPECT_EQ(result.paths().get_index(),
              (std::vector<fincore::timestamp>{7, 3, 7}));
    ASSERT_EQ(result.paths().shape().first, 3U);
    ASSERT_EQ(result.paths().shape().second, 2U);

    const auto double_columns = result.paths().get_columns_info<double>();
    ASSERT_EQ(double_columns.size(), 2U);
    EXPECT_EQ(std::get<0>(double_columns[0]), "left");
    EXPECT_EQ(std::get<0>(double_columns[1]), "right");
    EXPECT_EQ(std::get<1>(double_columns[0]), 3U);
    EXPECT_EQ(std::get<1>(double_columns[1]), 3U);
    EXPECT_EQ(std::get<2>(double_columns[0]), typeid(double));
    EXPECT_EQ(std::get<2>(double_columns[1]), typeid(double));
    EXPECT_NEAR(result.statistics().mean, 95.0, 1e-12);
    EXPECT_NEAR(result.statistics().sample_standard_deviation,
                21.213203435596427, 1e-12);
    EXPECT_NEAR(result.statistics().median, 95.0, 1e-12);
    EXPECT_NEAR(result.statistics().percentiles.percentile_1, 80.3, 1e-12);
    EXPECT_NEAR(result.statistics().percentiles.percentile_5, 81.5, 1e-12);
    EXPECT_NEAR(result.statistics().percentiles.percentile_25, 87.5, 1e-12);
    EXPECT_NEAR(result.statistics().percentiles.percentile_50, 95.0, 1e-12);
    EXPECT_NEAR(result.statistics().percentiles.percentile_75, 102.5, 1e-12);
    EXPECT_NEAR(result.statistics().percentiles.percentile_95, 108.5, 1e-12);
    EXPECT_NEAR(result.statistics().percentiles.percentile_99, 109.7, 1e-12);
    EXPECT_NEAR(result.terminal_value_at_risk(0.05), 81.5, 1e-12);
    EXPECT_EQ(result.terminal_conditional_value_at_risk(0.05), 80.0);
}

}  // namespace
