#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

TEST(AttributionFactorPredictionUmbrella,
     ExportsFactorRegressionPredictionSurface) {
    fincore::dataframe returns;
    returns.load_index({1, 2});
    returns.load_column<double>("returns", {3.0, 5.0},
                                hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe training_factors;
    training_factors.load_index({1, 2});
    training_factors.load_column<double>("MKT", {1.0, 2.0},
                                         hmdf::nan_policy::dont_pad_with_nans);

    constexpr std::array<std::string_view, 1U> factor_names{"MKT"};
    const auto regression =
        fincore::fit_factor_regression(returns, training_factors, factor_names);

    fincore::dataframe prediction_factors;
    // The index repeats 42 and is out of order: predictions are formed row by
    // row and the input index is copied over as it stands, so neither sorting nor
    // deduplication may happen even though a timestamp index is usually unique.
    prediction_factors.load_index({42, 7, 42});
    prediction_factors.load_column<double>("MKT", {3.0, -1.0, 1.5},
                                           hmdf::nan_policy::dont_pad_with_nans);

    const auto prediction = fincore::factor_regression_predictions(
        regression, prediction_factors, factor_names);

    ASSERT_EQ(prediction.shape().second, 1U);
    EXPECT_EQ(prediction.get_index(),
              (std::vector<fincore::timestamp>{42, 7, 42}));

    const auto double_columns = prediction.get_columns_info<double>();
    ASSERT_EQ(double_columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(double_columns[0]).c_str(),
                 std::string{fincore::factor_prediction_column}.c_str());
    EXPECT_EQ(std::get<1>(double_columns[0]), 3U);
    EXPECT_EQ(std::get<2>(double_columns[0]), typeid(double));

    const auto& values = prediction.get_column<double>(
        fincore::factor_prediction_column.data());
    ASSERT_EQ(values.size(), 3U);
    EXPECT_DOUBLE_EQ(values[0], 7.0);
    EXPECT_DOUBLE_EQ(values[1], -1.0);
    EXPECT_DOUBLE_EQ(values[2], 4.0);
}

}  // namespace
