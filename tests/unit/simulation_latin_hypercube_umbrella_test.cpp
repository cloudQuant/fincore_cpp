#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace {

using latin_hypercube_sampling_function = fincore::dataframe (*) (
    std::size_t, std::size_t, std::optional<std::uint64_t>);

static_assert(std::is_same_v<decltype(&fincore::latin_hypercube_sampling),
                             latin_hypercube_sampling_function>);

TEST(SimulationLatinHypercubeUmbrella, ExportsNamedStratifiedSamples) {
    const auto samples = fincore::latin_hypercube_sampling(4U, 1U, 42U);

    EXPECT_EQ(fincore::latin_hypercube_dimension_column_prefix, "lhs_dimension_");
    EXPECT_EQ(samples.get_index(), (std::vector<fincore::timestamp>{0, 1, 2, 3}));
    ASSERT_EQ(samples.shape().first, 4U);
    ASSERT_EQ(samples.shape().second, 1U);

    const auto columns = samples.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_EQ(std::get<0>(columns[0]), "lhs_dimension_0");
    EXPECT_EQ(std::get<1>(columns[0]), 4U);
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));

    std::vector<bool> seen_strata(4U, false);
    // Each of the four strata has to be hit exactly once, which is the defining
    // property of the sampler; the strict upper bound is what keeps a value of
    // exactly 1.0 from naming a fifth stratum.
    for (const double value : samples.get_column<double>("lhs_dimension_0")) {
        ASSERT_TRUE(std::isfinite(value));
        ASSERT_GE(value, 0.0);
        ASSERT_LT(value, 1.0);
        const auto stratum = static_cast<std::size_t>(std::floor(value * 4.0));
        ASSERT_LT(stratum, seen_strata.size());
        EXPECT_FALSE(seen_strata[stratum]);
        seen_strata[stratum] = true;
    }
    EXPECT_EQ(seen_strata, (std::vector<bool>{true, true, true, true}));
}

}  // namespace
