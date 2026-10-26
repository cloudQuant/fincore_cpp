#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <vector>

namespace {

TEST(SimulationGbmFromReturnsUmbrella,
     ExportsCalibratedPathsWithSourceOracleSchema) {
    const auto returns = fincore::make_series(
        {19, -3, 19, 7}, "returns", {0.01, 0.01, 0.01, 0.01});
    const auto paths = fincore::gbm_from_returns(returns, 4U, 2U, 4U, 42U);
    const std::array<double, 4U> expected{0.0025031276057951857,
                                            0.005012520859401182,
                                            0.0075281954445340915,
                                            0.010050167084168393};

    EXPECT_EQ(paths.get_index(),
              (std::vector<fincore::timestamp>{1, 2, 3, 4}));
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
            EXPECT_NEAR(values[row], expected[row], 1.0e-12)
                << name << " row " << row;
        }
    }
}

}  // namespace
