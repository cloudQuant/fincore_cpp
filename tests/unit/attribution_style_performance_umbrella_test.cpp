#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using one_argument_function =
    fincore::dataframe (*)(const fincore::dataframe &);
using two_argument_function = fincore::dataframe (*)(
    const fincore::dataframe &, const fincore::dataframe &);

static_assert(std::is_same_v<decltype(static_cast<one_argument_function>(
                                 &fincore::analyze_performance_by_style)),
                             one_argument_function>);
static_assert(std::is_same_v<decltype(static_cast<two_argument_function>(
                                 &fincore::analyze_performance_by_style)),
                             two_argument_function>);

TEST(AttributionStylePerformanceUmbrella,
     ExportsExplicitAndDefaultCallsThroughAttributionFacade) {
  fincore::dataframe returns;
  returns.load_index({42, -7});
  returns.load_column<double>("ALPHA", {.01, -.02},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("BETA", {.02, .01},
                              hmdf::nan_policy::dont_pad_with_nans);

  fincore::dataframe exposures;
  // The rows list BETA before ALPHA, opposite to the return column order, so
  // .0125 only comes from matching assets by name; pairing the two frames by
  // position would blend different returns and give a different blend.
  exposures.load_index({0, 1});
  exposures.load_column<std::string>(fincore::style_asset_column.data(),
                                     {"BETA", "ALPHA"},
                                     hmdf::nan_policy::dont_pad_with_nans);
  exposures.load_column<double>("blend", {1.0, 3.0},
                                hmdf::nan_policy::dont_pad_with_nans);

  const auto explicit_result = static_cast<two_argument_function>(
      &fincore::analyze_performance_by_style)(returns, exposures);
  const auto default_result = static_cast<one_argument_function>(
      &fincore::analyze_performance_by_style)(returns);

  EXPECT_EQ(explicit_result.shape(),
            (std::pair<std::size_t, std::size_t>{2U, 1U}));
  EXPECT_EQ(explicit_result.get_index(),
            (std::vector<fincore::timestamp>{0, 1}));
  const auto explicit_columns = explicit_result.get_columns_info<double>();
  ASSERT_EQ(explicit_columns.size(), 1U);
  EXPECT_EQ(std::string{std::get<0>(explicit_columns[0]).c_str()},
            "blend_return");
  const auto &explicit_values =
      explicit_result.get_column<double>("blend_return");
  ASSERT_EQ(explicit_values.size(), 2U);
  EXPECT_NEAR(explicit_values[0], .0125, 1.0e-12);
  EXPECT_NEAR(explicit_values[1], -.0125, 1.0e-12);

  EXPECT_EQ(default_result.shape(),
            (std::pair<std::size_t, std::size_t>{2U, 7U}));
  EXPECT_EQ(default_result.get_index(),
            (std::vector<fincore::timestamp>{0, 1}));
  const auto default_columns = default_result.get_columns_info<double>();
  ASSERT_EQ(default_columns.size(), 7U);
  EXPECT_EQ(std::string{std::get<0>(default_columns.front()).c_str()},
            "equal_weight_return");
  EXPECT_EQ(std::string{std::get<0>(default_columns.back()).c_str()},
            "low_return");
}

} // namespace
