#include <fincore/metrics/perf_attrib.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using fincore::timestamp;

const std::vector<timestamp> &dates() {
  static const std::vector<timestamp> value{
      1704153600000000000LL, 1704240000000000000LL, 1704326400000000000LL};
  return value;
}

fincore::dataframe dollar_positions() {
  fincore::dataframe positions;
  positions.load_index(std::vector<timestamp>(dates()));
  positions.load_column<double>("A", {20.0, 30.0, -20.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<int>("metadata", {1, 2, 3},
                             hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("B", {50.0, -10.0, 20.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("cash", {30.0, 10.0, 10.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  return positions;
}

fincore::dataframe factor_loadings() {
  fincore::dataframe loadings;
  loadings.load_index(
      {dates()[0], dates()[0], dates()[1], dates()[1], dates()[2], dates()[2]});
  loadings.load_column<std::string>("symbol", {"A", "B", "A", "B", "A", "B"},
                                    hmdf::nan_policy::dont_pad_with_nans);
  loadings.load_column<double>("market", {.2, .4, .5, .2, .1, .6},
                               hmdf::nan_policy::dont_pad_with_nans);
  loadings.load_column<double>("value", {.4, -.2, .1, .3, .5, -.1},
                               hmdf::nan_policy::dont_pad_with_nans);
  return loadings;
}

void expect_columns(const fincore::dataframe &frame,
                    const std::array<std::string_view, 2U> &expected) {
  const auto columns = frame.get_columns_info<double>();
  ASSERT_EQ(columns.size(), expected.size());
  for (std::size_t position = 0U; position < expected.size(); ++position) {
    EXPECT_STREQ(std::get<0>(columns[position]).c_str(),
                 std::string{expected[position]}.c_str());
    EXPECT_EQ(std::get<2>(columns[position]), typeid(double));
  }
}

TEST(PerfAttrib, NormalizesDollarPositionsAndComputesOrderedExposures) {
  const auto normalized =
      fincore::normalize_attribution_positions(dollar_positions());
  EXPECT_EQ(normalized.get_index(), dates());
  ASSERT_FALSE(normalized.has_column("cash"));
  ASSERT_FALSE(normalized.has_column("metadata"));
  expect_columns(normalized, {"A", "B"});
  EXPECT_EQ(normalized.get_column<double>("A"),
            (std::vector<double>{2.0 / 7.0, 3.0 / 4.0, -1.0 / 2.0}));
  EXPECT_EQ(normalized.get_column<double>("B"),
            (std::vector<double>{5.0 / 7.0, -1.0 / 4.0, 1.0 / 2.0}));

  constexpr std::array<std::string_view, 2U> factors{"market", "value"};
  const auto exposures =
      fincore::compute_exposures(normalized, factor_loadings(), factors);
  EXPECT_EQ(exposures.get_index(), dates());
  expect_columns(exposures, factors);
  const auto &market = exposures.get_column<double>("market");
  const auto &value = exposures.get_column<double>("value");
  ASSERT_EQ(market.size(), 3U);
  ASSERT_EQ(value.size(), 3U);
  EXPECT_NEAR(market[0], .342857142857143, 1.0e-12);
  EXPECT_NEAR(market[1], .325, 1.0e-12);
  EXPECT_NEAR(market[2], .25, 1.0e-12);
  EXPECT_NEAR(value[0], -.028571428571429, 1.0e-12);
  EXPECT_NEAR(value[1], .0, 1.0e-12);
  EXPECT_NEAR(value[2], -.3, 1.0e-12);
}

TEST(PerfAttrib, RejectsInvalidExposureSchemas) {
  constexpr std::array<std::string_view, 0U> no_factors{};
  constexpr std::array<std::string_view, 1U> empty_factor{""};
  constexpr std::array<std::string_view, 2U> duplicate_factors{"market",
                                                               "market"};
  const auto positions =
      fincore::normalize_attribution_positions(dollar_positions());
  const auto loadings = factor_loadings();
  EXPECT_THROW(fincore::compute_exposures(positions, loadings, no_factors),
               fincore::input_error);
  EXPECT_THROW(fincore::compute_exposures(positions, loadings, empty_factor),
               fincore::input_error);
  EXPECT_THROW(
      fincore::compute_exposures(positions, loadings, duplicate_factors),
      fincore::input_error);

  auto absent_symbol = loadings;
  absent_symbol.remove_column<std::string>("symbol");
  constexpr std::array<std::string_view, 1U> market{"market"};
  EXPECT_THROW(fincore::compute_exposures(positions, absent_symbol, market),
               fincore::input_error);

  fincore::dataframe wrong_symbol;
  wrong_symbol.load_index(std::vector<timestamp>(loadings.get_index()));
  wrong_symbol.load_column<int>("symbol", {1, 2, 1, 2, 1, 2},
                                hmdf::nan_policy::dont_pad_with_nans);
  wrong_symbol.load_column<double>("market", {.2, .4, .5, .2, .1, .6},
                                   hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::compute_exposures(positions, wrong_symbol, market),
               fincore::input_error);

  fincore::dataframe wrong_cash;
  wrong_cash.load_index(std::vector<timestamp>(dates()));
  wrong_cash.load_column<double>("A", {1.0, 1.0, 1.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
  wrong_cash.load_column<int>("cash", {0, 0, 0},
                              hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::normalize_attribution_positions(wrong_cash),
               fincore::input_error);

  auto missing_key = loadings;
  missing_key.remove_column<std::string>("symbol");
  missing_key.load_column<std::string>("symbol", {"A", "B", "A", "B", "A", "C"},
                                       hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::compute_exposures(positions, missing_key, market),
               fincore::input_error);

  auto duplicate_key = loadings;
  duplicate_key.remove_column<std::string>("symbol");
  duplicate_key.load_column<std::string>("symbol",
                                         {"A", "A", "A", "B", "A", "B"},
                                         hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::compute_exposures(positions, duplicate_key, market),
               fincore::input_error);

  fincore::dataframe malformed;
  malformed.load_index(std::vector<timestamp>(dates()));
  malformed.load_column<double>("A", {1.0, 1.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::normalize_attribution_positions(malformed),
               fincore::input_error);
}

} // namespace
