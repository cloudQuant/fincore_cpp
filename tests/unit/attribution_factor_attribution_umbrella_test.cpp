#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

TEST(AttributionFactorAttributionUmbrella,
     ExportsFactorAttributionDecompositionSurface) {
    fincore::dataframe returns;
    returns.load_index({1, 2, 3});
    returns.load_column<double>("returns", {3.0, 5.0, 7.0},
                                hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe factors;
    factors.load_index({1, 2, 3});
    factors.load_column<double>("MKT", {1.0, 2.0, 3.0},
                                hmdf::nan_policy::dont_pad_with_nans);

    constexpr std::array<std::string_view, 1U> factor_names{"MKT"};
    // The risk-free rate is deliberately not the default 0: at 0 the mean return
    // is exactly alpha plus the specific return, so unexplained would be 0 and
    // even a decomposition that dropped the subtraction would satisfy the checks.
    const auto summary = fincore::factor_attribution_decomposition(
        returns, factors, factor_names, .5);

    ASSERT_EQ(summary.shape().second, 2U);
    EXPECT_EQ(summary.get_index(),
              (std::vector<fincore::timestamp>{0, 1, 2, 3, 4}));

    const auto string_columns = summary.get_columns_info<std::string>();
    ASSERT_EQ(string_columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(string_columns[0]).c_str(),
                 std::string{fincore::factor_attribution_component_column}.c_str());
    EXPECT_EQ(std::get<2>(string_columns[0]), typeid(std::string));

    const auto double_columns = summary.get_columns_info<double>();
    ASSERT_EQ(double_columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(double_columns[0]).c_str(),
                 std::string{fincore::factor_attribution_contribution_column}.c_str());
    EXPECT_EQ(std::get<2>(double_columns[0]), typeid(double));

    EXPECT_EQ(summary.get_column<std::string>(
                  fincore::factor_attribution_component_column.data()),
              (std::vector<std::string>{"alpha", "MKT_attribution",
                                        "specific_return", "common_return",
                                        "unexplained"}));
    const auto& values = summary.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    ASSERT_EQ(values.size(), 5U);
    EXPECT_NEAR(values[0], 1.0, 1.0e-12);
    EXPECT_NEAR(values[1], 4.0, 1.0e-12);
    // specific_return sums one contribution per requested factor, so with the
    // single MKT column it is forced to equal values[1]; the equality shows the
    // row order, not an accumulation over several factors.
    EXPECT_NEAR(values[2], values[1], 1.0e-12);
    EXPECT_NEAR(values[3], 4.5, 1.0e-12);
    EXPECT_NEAR(values[4], values[3] - values[0] - values[2], 1.0e-12);
    EXPECT_NEAR(values[4], -.5, 1.0e-12);
}

}  // namespace
