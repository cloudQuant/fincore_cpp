#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <vector>

namespace {

TEST(SimulationGbmPathsUmbrella, ExportsNamedZeroVolatilityPaths) {
    // Zero volatility closes the form: each point is 100 * exp(0.1 * t) with
    // t = 0, 0.25, ..., 1, so the two paths coincide and the seed cannot matter;
    // the 1e-12 tolerance only has to cover the exponential.
    const auto paths = fincore::geometric_brownian_motion(
        100.0, 0.1, 0.0, 1.0, 0.25, 2U, 42U);
    const std::array<double, 5U> expected{100.0,
                                           102.53151205244289,
                                           105.12710963760242,
                                           107.78841508846317,
                                           110.51709180756478};

    EXPECT_EQ(paths.get_index(),
              (std::vector<fincore::timestamp>{0, 1, 2, 3, 4}));
    ASSERT_EQ(paths.shape().first, expected.size());
    ASSERT_EQ(paths.shape().second, 2U);

    const auto columns = paths.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 2U);
    EXPECT_EQ(std::get<0>(columns[0]), "path_0");
    EXPECT_EQ(std::get<1>(columns[0]), expected.size());
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));
    EXPECT_EQ(std::get<0>(columns[1]), "path_1");
    EXPECT_EQ(std::get<1>(columns[1]), expected.size());
    EXPECT_EQ(std::get<2>(columns[1]), typeid(double));

    for (const std::string_view name : {"path_0", "path_1"}) {
        const auto& values = paths.get_column<double>(name.data());
        ASSERT_EQ(values.size(), expected.size());
        for (std::size_t row = 0U; row < expected.size(); ++row) {
            EXPECT_NEAR(values[row], expected[row], 1.0e-12) << name << " row "
                                                               << row;
        }
    }
}

}  // namespace
