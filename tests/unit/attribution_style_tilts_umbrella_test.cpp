#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using style_tilts_function = fincore::dataframe (*)(
    const fincore::dataframe &, std::size_t);

static_assert(std::is_same_v<decltype(static_cast<style_tilts_function>(
                                 &fincore::calculate_style_tilts)),
                             style_tilts_function>);
static_assert(std::is_same_v<decltype(fincore::calculate_style_tilts(
                                 std::declval<const fincore::dataframe &>())),
                             fincore::dataframe>);

TEST(AttributionStyleTiltsUmbrella,
     ExportsDefaultCallThroughAttributionFacade) {
  // 253 rows is one more than the default 252-observation window, so exactly
  // one output row remains; all-zero returns leave every tilt test on a tie,
  // which is why this umbrella asserts shape, index tail and names only.
  fincore::dataframe returns;
  std::vector<fincore::timestamp> index(253U);
  for (std::size_t row{0U}; row < index.size(); ++row) {
    index[row] = static_cast<fincore::timestamp>(1000U + row);
  }
  returns.load_index(std::move(index));
  returns.load_column<double>("A", std::vector<double>(253U, 0.0),
                              hmdf::nan_policy::dont_pad_with_nans);

  const auto result = fincore::calculate_style_tilts(returns);
  EXPECT_EQ(result.shape(),
            (std::pair<std::size_t, std::size_t>{1U, 8U}));
  EXPECT_EQ(result.get_index(),
            (std::vector<fincore::timestamp>{1252}));
  const auto columns = result.get_columns_info<double>();
  ASSERT_EQ(columns.size(), 8U);
  EXPECT_EQ(std::string{std::get<0>(columns.front()).c_str()}, "A_large");
  EXPECT_EQ(std::string{std::get<0>(columns.back()).c_str()}, "A_low_vol");
}

} // namespace
