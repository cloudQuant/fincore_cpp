#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <tuple>
#include <typeindex>
#include <vector>

namespace {

TEST(SimulationBootstrapUmbrella, ExportsExactTypedOrdinalConstantDistribution) {
    const auto returns =
        fincore::make_series({9, -3, 9}, "returns", {0.01, 0.01, 0.01});
    const auto distribution = fincore::bootstrap_distribution(
        returns, 4U, fincore::bootstrap_statistic::mean, 42U);

    EXPECT_EQ(distribution.get_index(),
              (std::vector<fincore::timestamp>{0, 1, 2, 3}));
    ASSERT_EQ(distribution.shape().first, 4U);
    ASSERT_EQ(distribution.shape().second, 1U);

    const auto columns = distribution.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_EQ(std::get<0>(columns[0]), "bootstrap");
    EXPECT_EQ(std::get<1>(columns[0]), 4U);
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));

    const auto& values = distribution.get_column<double>("bootstrap");
    ASSERT_EQ(values.size(), 4U);
    // Constant returns make every resample identical, so this can only catch wiring,
    // naming or indexing faults; sampling quality is not what is being pinned here.
    for (const double value : values) {
        EXPECT_DOUBLE_EQ(value, 0.01);
    }
}

}  // namespace
