#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

TEST(AttributionFactorExposuresUmbrella, ExportsRollingFactorExposuresSurface) {
    fincore::dataframe returns;
    returns.load_index({1, 2, 3});
    returns.load_column<double>("returns", {3.0, 5.0, 7.0},
                                hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe factors;
    factors.load_index({1, 2, 3});
    factors.load_column<double>("MKT", {1.0, 2.0, 3.0},
                                hmdf::nan_policy::dont_pad_with_nans);

    constexpr std::array<std::string_view, 1U> factor_names{"MKT"};
    const auto exposures =
        fincore::rolling_factor_exposures(returns, factors, factor_names);

    ASSERT_EQ(exposures.shape().second, 2U);
    EXPECT_EQ(exposures.get_index(), (std::vector<fincore::timestamp>{1, 2, 3}));
    const auto columns = exposures.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 2U);
    EXPECT_STREQ(std::get<0>(columns[0]).c_str(),
                 std::string{fincore::factor_regression_alpha_name}.c_str());
    EXPECT_STREQ(std::get<0>(columns[1]).c_str(), "MKT");

    const auto& alpha = exposures.get_column<double>(
        fincore::factor_regression_alpha_name.data());
    const auto& market = exposures.get_column<double>("MKT");
    ASSERT_EQ(alpha.size(), 3U);
    ASSERT_EQ(market.size(), 3U);
    EXPECT_TRUE(std::isnan(alpha[0]));
    EXPECT_TRUE(std::isnan(market[0]));
    EXPECT_NEAR(alpha[2], 1.0, 1.0e-12);
    EXPECT_NEAR(market[2], 2.0, 1.0e-12);
}

}  // namespace
