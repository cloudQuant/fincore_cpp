#include <fincore/attribution/style.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <limits>
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

template <typename Frame>
concept has_two_argument_factor_overload =
    requires(const Frame &returns, const Frame &factors) {
      { fincore::calculate_style_tilts(returns, factors) } ->
          std::same_as<fincore::dataframe>;
    };

template <typename Frame>
concept has_three_argument_factor_overload =
    requires(const Frame &returns, const Frame &factors) {
      { fincore::calculate_style_tilts(returns, factors, std::size_t{2U}) } ->
          std::same_as<fincore::dataframe>;
    };

static_assert(!has_two_argument_factor_overload<fincore::dataframe>);
static_assert(!has_three_argument_factor_overload<fincore::dataframe>);

fincore::dataframe make_oracle_returns() {
  fincore::dataframe returns;
  returns.load_index({42, -7, 100, 3, 55});
  returns.load_column<double>("A", {0.0, .25, -.5, 0.0, .75},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("B", {0.0, 0.0, -.5, -.5, .75},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("C", {0.0, -.5, 1.0, .25, .75},
                              hmdf::nan_policy::dont_pad_with_nans);
  return returns;
}

const std::vector<std::string> &oracle_columns() {
  static const std::vector<std::string> columns{
      "A_large",   "A_small",   "B_large",   "B_small",
      "C_large",   "C_small",   "A_winner",  "A_loser",
      "B_winner",  "B_loser",   "C_winner",  "C_loser",
      "A_value",   "A_growth",  "B_value",   "B_growth",
      "C_value",   "C_growth",  "A_high_vol", "A_low_vol",
      "B_high_vol", "B_low_vol", "C_high_vol", "C_low_vol"};
  return columns;
}

const std::vector<std::vector<double>> &oracle_values() {
  static const std::vector<std::vector<double>> values{
      {0.0, 0.0}, {0.0, 0.0}, {0.0, 1.0}, {0.0, 0.0},
      {0.0, 0.0}, {1.0, 1.0}, {0.0, 0.0}, {1.0, 1.0},
      {0.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {1.0, 0.0},
      {0.0, 0.0}, {1.0, 1.0}, {1.0, 1.0}, {0.0, 0.0},
      {0.0, 0.0}, {1.0, 1.0}, {1.0, 1.0}, {0.0, 0.0},
      {0.0, 0.0}, {1.0, 1.0}, {1.0, 1.0}, {0.0, 0.0}};
  return values;
}

void expect_empty(const fincore::dataframe &result) {
  EXPECT_EQ(result.shape(),
            (std::pair<std::size_t, std::size_t>{0U, 0U}));
  EXPECT_TRUE(result.get_index().empty());
  EXPECT_TRUE(result.get_columns_info<double>().empty());
  EXPECT_TRUE(result.get_columns_info<int>().empty());
  EXPECT_TRUE(result.get_columns_info<std::string>().empty());
}

void expect_schema(const fincore::dataframe &result,
                   const std::vector<fincore::timestamp> &index,
                   const std::vector<std::string> &names) {
  EXPECT_EQ(result.shape(),
            (std::pair<std::size_t, std::size_t>{index.size(), names.size()}));
  EXPECT_EQ(result.get_index(), index);
  EXPECT_TRUE(result.get_columns_info<int>().empty());
  EXPECT_TRUE(result.get_columns_info<std::string>().empty());
  const auto columns = result.get_columns_info<double>();
  ASSERT_EQ(columns.size(), names.size());
  for (std::size_t column{0U}; column < names.size(); ++column) {
    EXPECT_EQ(std::string{std::get<0>(columns[column]).c_str()}, names[column]);
    EXPECT_EQ(std::get<1>(columns[column]), index.size());
    EXPECT_EQ(std::get<2>(columns[column]), typeid(double));
  }
}

void expect_column(const fincore::dataframe &result, const std::string &name,
                   const std::vector<double> &expected) {
  EXPECT_EQ(result.get_column<double>(name.c_str()), expected);
}

void invoke_style_tilts(const fincore::dataframe &returns,
                        const std::size_t window) {
  static_cast<void>(fincore::calculate_style_tilts(returns, window));
}

TEST(StyleTilts, MatchesPythonOracleWithPhaseMajorSchema) {
  const auto result = fincore::calculate_style_tilts(make_oracle_returns(), 3U);
  expect_schema(result, {3, 55}, oracle_columns());
  for (std::size_t column{0U}; column < oracle_columns().size(); ++column) {
    expect_column(result, oracle_columns()[column], oracle_values()[column]);
  }
}

TEST(StyleTilts, UsesAverageRanksAndExactCrossSectionalBoundaries) {
  fincore::dataframe returns;
  returns.load_index({8, 4, 9});
  returns.load_column<double>("A", {0.0, 1.0, 99.0},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("B", {0.0, 2.0, 99.0},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("C", {0.0, 3.0, 99.0},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("D", {0.0, 4.0, 99.0},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("E", {0.0, 5.0, 99.0},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("F", {0.0, 6.0, 99.0},
                              hmdf::nan_policy::dont_pad_with_nans);

  const auto result = fincore::calculate_style_tilts(returns, 2U);
  expect_column(result, "A_large", {1.0});
  expect_column(result, "B_large", {1.0});
  expect_column(result, "C_large", {0.0});
  expect_column(result, "D_small", {0.0});
  expect_column(result, "E_small", {1.0});
  expect_column(result, "F_small", {1.0});
  expect_column(result, "C_value", {1.0});
  expect_column(result, "D_value", {0.0});
  expect_column(result, "D_growth", {1.0});
}

TEST(StyleTilts, PreservesAverageRankTiesAndZeroAsLoser) {
  fincore::dataframe returns;
  returns.load_index({0, 1, 2});
  returns.load_column<double>("A", {0.0, 0.0, 8.0},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("B", {0.0, 0.0, 8.0},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("C", {0.0, 1.0, -8.0},
                              hmdf::nan_policy::dont_pad_with_nans);

  const auto result = fincore::calculate_style_tilts(returns, 2U);
  expect_column(result, "A_large", {0.0});
  expect_column(result, "B_large", {0.0});
  expect_column(result, "C_small", {1.0});
  expect_column(result, "A_winner", {0.0});
  expect_column(result, "A_loser", {1.0});
  expect_column(result, "B_value", {1.0});
  expect_column(result, "C_growth", {1.0});
}

TEST(StyleTilts, ExcludesCurrentRowAndUsesOriginalTrailingWindow) {
  fincore::dataframe returns;
  returns.load_index({10, 11, 12, 13, 14});
  returns.load_column<double>("A", {.5, 0.0, 0.0, 0.0, 100.0},
                              hmdf::nan_policy::dont_pad_with_nans);

  const auto result = fincore::calculate_style_tilts(returns, 3U);
  expect_column(result, "A_winner", {1.0, 0.0});
  expect_column(result, "A_loser", {0.0, 1.0});
}

TEST(StyleTilts, UsesLastSixtySampleVolatilityAndMedianEquality) {
  constexpr std::size_t row_count{62U};
  std::vector<fincore::timestamp> index;
  std::vector<double> alpha(row_count, 0.0);
  std::vector<double> beta(row_count, 0.0);
  std::vector<double> gamma(row_count, 0.0);
  index.reserve(row_count);
  for (std::size_t row{0U}; row < row_count; ++row) {
    index.push_back(static_cast<fincore::timestamp>(row));
  }
  alpha[0U] = std::numeric_limits<double>::max() / 4.0;
  for (std::size_t row{1U}; row <= 60U; ++row) {
    beta[row] = row % 2U == 0U ? 1.0 : -1.0;
    gamma[row] = row % 2U == 0U ? 2.0 : -2.0;
  }

  fincore::dataframe returns;
  returns.load_index(std::move(index));
  returns.load_column<double>("A", std::move(alpha),
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("B", std::move(beta),
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("C", std::move(gamma),
                              hmdf::nan_policy::dont_pad_with_nans);

  const auto result = fincore::calculate_style_tilts(returns, 61U);
  expect_column(result, "A_high_vol", {0.0});
  expect_column(result, "A_low_vol", {1.0});
  expect_column(result, "B_high_vol", {1.0});
  expect_column(result, "B_low_vol", {0.0});
  expect_column(result, "C_high_vol", {1.0});
  expect_column(result, "C_low_vol", {0.0});
}

TEST(StyleTilts, ReturnsCanonicalEmptyBeforeNonemptyValidation) {
  fincore::dataframe zero_rows;
  zero_rows.load_index({});
  zero_rows.load_column<int>("malformed", {},
                             hmdf::nan_policy::dont_pad_with_nans);
  expect_empty(fincore::calculate_style_tilts(zero_rows));

  fincore::dataframe zero_columns;
  zero_columns.load_index({1, 2, 3});
  expect_empty(fincore::calculate_style_tilts(zero_columns));

  fincore::dataframe malformed;
  malformed.load_index({1, 2, 3});
  malformed.load_column<int>("malformed", {1, 2, 3},
                             hmdf::nan_policy::dont_pad_with_nans);
  expect_empty(fincore::calculate_style_tilts(malformed, 0U));
  expect_empty(fincore::calculate_style_tilts(malformed, 1U));

  fincore::dataframe short_history;
  short_history.load_index({1, 2});
  short_history.load_column<int>("malformed", {1, 2},
                                 hmdf::nan_policy::dont_pad_with_nans);
  expect_empty(fincore::calculate_style_tilts(short_history));
}

TEST(StyleTilts, PreservesDuplicateAndNonmonotonicIndexesPositionally) {
  auto returns = make_oracle_returns();
  returns.load_index({42, -7, 100, 3, 3});
  const auto result = fincore::calculate_style_tilts(returns, 3U);

  expect_schema(result, {3, 3}, oracle_columns());
  for (std::size_t column{0U}; column < oracle_columns().size(); ++column) {
    expect_column(result, oracle_columns()[column], oracle_values()[column]);
  }
}

TEST(StyleTilts, DefaultWindowMatchesExplicit252) {
  constexpr std::size_t row_count{254U};
  std::vector<fincore::timestamp> index;
  std::vector<double> alpha(row_count, 0.0);
  std::vector<double> beta(row_count, 0.0);
  index.reserve(row_count);
  for (std::size_t row{0U}; row < row_count; ++row) {
    index.push_back(static_cast<fincore::timestamp>(row));
  }
  alpha[0U] = .25;
  beta[1U] = -.25;

  fincore::dataframe returns;
  returns.load_index(std::move(index));
  returns.load_column<double>("A", std::move(alpha),
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("B", std::move(beta),
                              hmdf::nan_policy::dont_pad_with_nans);

  const auto defaults = fincore::calculate_style_tilts(returns);
  const auto explicit_window = fincore::calculate_style_tilts(returns, 252U);
  expect_schema(defaults, {252, 253},
                {"A_large", "A_small", "B_large", "B_small",
                 "A_winner", "A_loser", "B_winner", "B_loser",
                 "A_value", "A_growth", "B_value", "B_growth",
                 "A_high_vol", "A_low_vol", "B_high_vol", "B_low_vol"});
  for (const auto &column : defaults.get_columns_info<double>()) {
    const std::string name{std::get<0>(column).c_str()};
    EXPECT_EQ(defaults.get_column<double>(name.c_str()),
              explicit_window.get_column<double>(name.c_str()));
  }
}

TEST(StyleTilts, RejectsMalformedStructuresNamesAndNonfiniteValues) {
  fincore::dataframe wrong_type;
  wrong_type.load_index({1, 2, 3});
  wrong_type.load_column<int>("A", {1, 2, 3},
                              hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_tilts(wrong_type, 2U), fincore::input_error);

  fincore::dataframe mixed;
  mixed.load_index({1, 2, 3});
  mixed.load_column<double>("A", {0.0, 0.0, 0.0},
                            hmdf::nan_policy::dont_pad_with_nans);
  mixed.load_column<int>("metadata", {1, 2, 3},
                         hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_tilts(mixed, 2U), fincore::input_error);

  fincore::dataframe short_column;
  short_column.load_index({1, 2, 3});
  short_column.load_column<double>("A", {0.0, 0.0},
                                   hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_tilts(short_column, 2U), fincore::input_error);

  fincore::dataframe empty_name;
  empty_name.load_index({1, 2, 3});
  empty_name.load_column<double>("", {0.0, 0.0, 0.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_tilts(empty_name, 2U), fincore::input_error);

  for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
    fincore::dataframe returns;
    returns.load_index({1, 2, 3});
    returns.load_column<double>("A", {0.0, invalid, 0.0},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(invoke_style_tilts(returns, 2U), fincore::input_error);
  }
}

TEST(StyleTilts, RejectsDerivedNameAndArithmeticOverflow) {
  const std::string long_name(
      fincore::dataframe::ColNameType::capacity() -
          std::string{"_high_vol"}.size(),
      'a');
  fincore::dataframe long_named;
  long_named.load_index({1, 2, 3});
  long_named.load_column<double>(long_name.c_str(), {0.0, 0.0, 0.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_tilts(long_named, 2U), fincore::input_error);

  const double maximum = std::numeric_limits<double>::max();
  fincore::dataframe overflowing;
  overflowing.load_index({1, 2, 3});
  overflowing.load_column<double>("A", {maximum, maximum, 0.0},
                                   hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_tilts(overflowing, 2U), fincore::input_error);
}

TEST(StyleTilts, OwnsOutputAfterSourceMutationAndDestruction) {
  const auto retained = [] {
    auto returns = make_oracle_returns();
    auto result = fincore::calculate_style_tilts(returns, 3U);
    returns.load_index({9, 9, 9, 9, 9});
    returns.get_column<double>("A")[0] = 99.0;
    return result;
  }();

  expect_schema(retained, {3, 55}, oracle_columns());
  expect_column(retained, "C_winner", {0.0, 1.0});
}

} // namespace
