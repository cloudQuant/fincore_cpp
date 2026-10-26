#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace {

TEST(AttributionFactorRegressionUmbrella, ExportsFactorRegressionSurface) {
    fincore::dataframe returns;
    returns.load_index({1, 2});
    returns.load_column<double>("returns", {3.0, 5.0},
                                hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe factors;
    factors.load_index({1, 2});
    factors.load_column<double>("MKT", {1.0, 2.0},
                                hmdf::nan_policy::dont_pad_with_nans);

    constexpr std::array<std::string_view, 1U> factor_names{"MKT"};
    // Two rows is the documented floor for a one-factor fit and the two points
    // lie exactly on y = 2x + 1, so the design is exactly determined: the unit
    // R-squared and the zero residual come from the fixture, not from a least fit.
    const auto result =
        fincore::fit_factor_regression(returns, factors, factor_names);

    const double alpha = result.alpha;
    const double r_squared = result.r_squared;
    const auto& labels = result.coefficients.get_column<std::string>(
        fincore::factor_regression_factor_column.data());
    const auto& coefficients = result.coefficients.get_column<double>(
        fincore::factor_regression_coefficient_column.data());
    const auto& fitted = result.fitted_returns.get_column<double>(
        fincore::factor_regression_fitted_column.data());
    const auto& residuals = result.residuals.get_column<double>(
        fincore::factor_regression_residual_column.data());

    ASSERT_EQ(labels, (std::vector<std::string>{"alpha", "MKT"}));
    ASSERT_EQ(coefficients.size(), 2U);
    ASSERT_EQ(fitted.size(), 2U);
    ASSERT_EQ(residuals.size(), 2U);
    EXPECT_DOUBLE_EQ(alpha, 1.0);
    EXPECT_DOUBLE_EQ(coefficients[1], 2.0);
    EXPECT_DOUBLE_EQ(r_squared, 1.0);
    EXPECT_DOUBLE_EQ(fitted[0], 3.0);
    EXPECT_DOUBLE_EQ(residuals[0], 0.0);
}

}  // namespace
