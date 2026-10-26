#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace {

using antithetic_variates_function = fincore::dataframe (*) (const fincore::dataframe&);

static_assert(std::is_same_v<decltype(&fincore::antithetic_variates),
                             antithetic_variates_function>);

TEST(SimulationAntitheticVariatesUmbrella, ExportsNamedMirroredPaths) {
    fincore::dataframe paths;
    paths.load_index(std::vector<fincore::timestamp>{7, 8});
    paths.load_column<double>("strategy", std::vector<double>{100.0, 110.0},
                              hmdf::nan_policy::dont_pad_with_nans);

    const auto result = fincore::antithetic_variates(paths);

    EXPECT_EQ(fincore::antithetic_path_column_prefix, "antithetic_");
    EXPECT_EQ(result.get_index(), (std::vector<fincore::timestamp>{7, 8}));
    ASSERT_EQ(result.shape().first, 2U);
    ASSERT_EQ(result.shape().second, 2U);

    const auto columns = result.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 2U);
    EXPECT_EQ(std::get<0>(columns[0]), "strategy");
    EXPECT_EQ(std::get<1>(columns[0]), 2U);
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));
    EXPECT_EQ(std::get<0>(columns[1]), "antithetic_strategy");
    EXPECT_EQ(std::get<1>(columns[1]), 2U);
    EXPECT_EQ(std::get<2>(columns[1]), typeid(double));
    EXPECT_EQ(result.get_column<double>("strategy"),
              (std::vector<double>{100.0, 110.0}));
    // The mirror reflects the return path rather than inverting the wealth level:
    // +10% becomes -10%, which gives exactly 90 where 100 / 1.1 would give
    // 90.909..., so the difference is visible in the low digits.
    EXPECT_EQ(result.get_column<double>("antithetic_strategy"),
              (std::vector<double>{100.0, 90.0}));
}

}  // namespace
