#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using automatic_function =
    fincore::dataframe (*)(const fincore::dataframe&);
using explicit_function = fincore::dataframe (*)(
    const fincore::dataframe&,
    const fincore::dataframe&,
    std::span<const std::string_view>);

static_assert(std::is_same_v<
              decltype(static_cast<automatic_function>(
                  &fincore::calculate_regression_attribution)),
              automatic_function>);
static_assert(std::is_same_v<
              decltype(static_cast<explicit_function>(
                  &fincore::calculate_regression_attribution)),
              explicit_function>);

TEST(AttributionStyleRegressionAttributionUmbrella,
     ExposesBothPublicCallsAndExactSchema) {
  fincore::dataframe portfolio;
  portfolio.load_index({1, 2, 3});
  portfolio.load_column<double>("p", {.01, .02, .03},
                                hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe styles;
  styles.load_index({1, 2, 3});
  styles.load_column<double>("selected", {.02, .04, .06},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"selected"};

  // No contribution value is asserted on either path, and the two frames above
  // share one index in one order, so aligned pairing and positional pairing
  // coincide: what is pinned here is the arity, labels and column schema.
  const auto explicit_result =
      static_cast<explicit_function>(&fincore::calculate_regression_attribution)(
          portfolio, styles, selected);
  EXPECT_EQ(explicit_result.shape(),
            (std::pair<std::size_t, std::size_t>{2U, 2U}));
  ASSERT_EQ(explicit_result.get_index().size(), 2U);
  EXPECT_EQ(explicit_result.get_index()[0], 0);
  EXPECT_EQ(explicit_result.get_index()[1], 1);
  EXPECT_EQ(explicit_result.get_columns_info<std::string>().size(), 1U);
  EXPECT_EQ(explicit_result.get_columns_info<double>().size(), 1U);
  EXPECT_EQ(explicit_result.get_column<std::string>(
                fincore::style_name_column.data()),
            (std::vector<std::string>{"selected", "residual"}));

  fincore::dataframe assets;
  assets.load_index({1, 2, 3});
  assets.load_column<double>("A", {.01, .02, .03},
                             hmdf::nan_policy::dont_pad_with_nans);
  assets.load_column<double>("B", {.03, .02, .01},
                             hmdf::nan_policy::dont_pad_with_nans);
  const auto automatic_result =
      static_cast<automatic_function>(&fincore::calculate_regression_attribution)(
          assets);
  EXPECT_EQ(automatic_result.get_columns_info<std::string>().size(), 1U);
  EXPECT_EQ(automatic_result.get_columns_info<double>().size(), 1U);
  EXPECT_EQ(fincore::style_attribution_contribution_column, "contribution");
  EXPECT_EQ(fincore::style_attribution_residual_name, "residual");
}

}  // namespace
