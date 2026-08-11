#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <vector>

namespace {

TEST(SimulationCorrelationUmbrella, ExportsNamedCorrelationBreakdownMatrix) {
    constexpr std::array<std::string_view, 2U> asset_names{"equity", "credit"};

    const auto matrix = fincore::generate_correlation_breakdown(asset_names, 0.25);

    EXPECT_EQ(matrix.get_index(),
              (std::vector<fincore::timestamp>{
                  static_cast<fincore::timestamp>(0),
                  static_cast<fincore::timestamp>(1)}));
    ASSERT_EQ(matrix.shape().first, 2U);
    ASSERT_EQ(matrix.shape().second, 2U);

    const auto columns = matrix.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 2U);
    EXPECT_EQ(std::get<0>(columns[0]), "equity");
    EXPECT_EQ(std::get<1>(columns[0]), 2U);
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));
    EXPECT_EQ(std::get<0>(columns[1]), "credit");
    EXPECT_EQ(std::get<1>(columns[1]), 2U);
    EXPECT_EQ(std::get<2>(columns[1]), typeid(double));

    const auto& equity = matrix.get_column<double>("equity");
    const auto& credit = matrix.get_column<double>("credit");
    EXPECT_DOUBLE_EQ(equity[0], 1.0);
    EXPECT_DOUBLE_EQ(equity[1], 0.25);
    EXPECT_DOUBLE_EQ(credit[0], 0.25);
    EXPECT_DOUBLE_EQ(credit[1], 1.0);
}

}  // namespace
