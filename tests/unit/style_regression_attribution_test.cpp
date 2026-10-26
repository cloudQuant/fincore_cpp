#include <fincore/attribution/style.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
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
static_assert(std::is_same_v<
              decltype(fincore::calculate_regression_attribution(
                  std::declval<const fincore::dataframe&>())),
              fincore::dataframe>);

template <typename Selector>
concept accepts_exposure_frame = requires(
    const fincore::dataframe& portfolio,
    const fincore::dataframe& styles,
    const Selector& selector) {
  fincore::calculate_regression_attribution(portfolio, styles, selector);
};

static_assert(!accepts_exposure_frame<fincore::dataframe>);

constexpr double tolerance{1.0e-12};

fincore::dataframe make_portfolio_series() {
  fincore::dataframe result;
  result.load_index({30, 10, 50, 20, 40});
  result.load_column<double>("portfolio", {.03, .01, .05, .02, .04},
                             hmdf::nan_policy::dont_pad_with_nans);
  return result;
}

fincore::dataframe make_portfolio_frame() {
  fincore::dataframe result;
  result.load_index({30, 10, 50, 20, 40});
  result.load_column<double>("left", {.02, .04, .06, .08, .10},
                             hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("right", {.04, .06, .08, .10, .12},
                             hmdf::nan_policy::dont_pad_with_nans);
  return result;
}

fincore::dataframe make_style_returns() {
  fincore::dataframe result;
  result.load_index({40, 20, 50, 10, 30});
  result.load_column<double>("growth", {.08, .04, .10, .02, .06},
                             hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("value", {.02, .04, .01, .05, .03},
                             hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("unselected", {9., 8., 7., 6., 5.},
                             hmdf::nan_policy::dont_pad_with_nans);
  return result;
}

fincore::dataframe make_asset_returns() {
  fincore::dataframe result;
  result.load_index({42, -7, 42, 100, 3});
  result.load_column<double>("ALPHA", {.01, -.02, .03, 0., -.01},
                             hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("BETA", {.02, .01, -.01, .04, 0.},
                             hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("GAMMA", {-.01, .03, .02, -.02, .05},
                             hmdf::nan_policy::dont_pad_with_nans);
  return result;
}

void expect_empty(const fincore::dataframe& result) {
  EXPECT_EQ(result.shape(),
            (std::pair<std::size_t, std::size_t>{0U, 0U}));
  EXPECT_TRUE(result.get_index().empty());
  EXPECT_TRUE(result.get_columns_info<double>().empty());
  EXPECT_TRUE(result.get_columns_info<std::string>().empty());
}

void expect_result(const fincore::dataframe& result,
                   const std::vector<std::string>& styles,
                   const std::vector<double>& expected) {
  ASSERT_EQ(styles.size(), expected.size());
  EXPECT_EQ(result.shape(),
            (std::pair<std::size_t, std::size_t>{styles.size(), 2U}));
  ASSERT_EQ(result.get_index().size(), styles.size());
  for (std::size_t row{0U}; row < styles.size(); ++row) {
    EXPECT_EQ(result.get_index()[row],
              static_cast<fincore::timestamp>(row));
  }

  const auto string_columns = result.get_columns_info<std::string>();
  const auto double_columns = result.get_columns_info<double>();
  ASSERT_EQ(string_columns.size(), 1U);
  ASSERT_EQ(double_columns.size(), 1U);
  EXPECT_EQ(std::string{std::get<0>(string_columns[0]).c_str()},
            fincore::style_name_column);
  EXPECT_EQ(std::string{std::get<0>(double_columns[0]).c_str()},
            fincore::style_attribution_contribution_column);
  EXPECT_EQ(std::get<1>(string_columns[0]), styles.size());
  EXPECT_EQ(std::get<1>(double_columns[0]), styles.size());
  EXPECT_EQ(std::get<2>(string_columns[0]), typeid(std::string));
  EXPECT_EQ(std::get<2>(double_columns[0]), typeid(double));

  const auto& actual_styles =
      result.get_column<std::string>(fincore::style_name_column.data());
  const auto& actual = result.get_column<double>(
      fincore::style_attribution_contribution_column.data());
  ASSERT_EQ(actual_styles.size(), styles.size());
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t row{0U}; row < styles.size(); ++row) {
    EXPECT_EQ(actual_styles[row], styles[row]);
    EXPECT_NEAR(actual[row], expected[row], tolerance);
  }
}

template <std::size_t N>
fincore::dataframe invoke_explicit(
    const fincore::dataframe& portfolio,
    const fincore::dataframe& styles,
    const std::array<std::string_view, N>& selected) {
  return fincore::calculate_regression_attribution(portfolio, styles,
                                                    selected);
}

TEST(StyleRegressionAttribution,
     MatchesExplicitSourceOracleAndStyleReturnOrder) {
  const std::array<std::string_view, 2U> selected{"value", "growth"};
  // Selector order only decides which columns take part; the result rows follow
  // the style frame's own column order, which is why growth precedes value here,
  // and the residual row is always appended last.
  const auto result = invoke_explicit(make_portfolio_series(),
                                      make_style_returns(), selected);
  expect_result(result, {"growth", "value", "residual"},
                {.06, -.03, 0.0});
}

TEST(StyleRegressionAttribution,
     UsesCenteredPearsonCorrelationRatherThanRegressionSlope) {
  fincore::dataframe portfolio;
  portfolio.load_index({1, 2, 3});
  portfolio.load_column<double>("p", {1., 2., 4.},
                                hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe styles;
  styles.load_index({1, 2, 3});
  styles.load_column<double>("selected", {2., 1., 3.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"selected"};

  const double contribution{6.0 / std::sqrt(21.0)};
  expect_result(invoke_explicit(portfolio, styles, selected),
                {"selected", "residual"},
                {contribution, 7.0 / 3.0 - contribution});
}

TEST(StyleRegressionAttribution,
     RowAveragesMultiColumnPortfolioLikePythonDataFrame) {
  const std::array<std::string_view, 2U> selected{"value", "growth"};
  const auto result = invoke_explicit(make_portfolio_frame(),
                                      make_style_returns(), selected);
  expect_result(result, {"growth", "value", "residual"},
                {.018000000000000006, -.009000000000000003,
                 .061000000000000006});
}

TEST(StyleRegressionAttribution,
     AveragesEveryPortfolioColumnBeforeComputingCorrelation) {
  fincore::dataframe portfolio;
  portfolio.load_index({1, 2, 3, 4});
  portfolio.load_column<double>("left", {0., 10., 0., 10.},
                                hmdf::nan_policy::dont_pad_with_nans);
  portfolio.load_column<double>("right", {2., -6., 8., 6.},
                                hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe styles;
  styles.load_index({1, 2, 3, 4});
  styles.load_column<double>("selected", {1., 2., 4., 8.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"selected"};

  expect_result(invoke_explicit(portfolio, styles, selected),
                {"selected", "residual"}, {3.75, 0.0});
}

TEST(StyleRegressionAttribution,
     UsesCommonStyleMeanAndFullPortfolioMeanForResidual) {
  fincore::dataframe portfolio;
  portfolio.load_index({1, 2, 3, 4});
  portfolio.load_column<double>("p", {1., 2., 3., 4.},
                                hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe styles;
  styles.load_index({1, 2, 3, 99});
  styles.load_column<double>("selected", {2., 4., 6., 100.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"selected"};

  // The two frames share only three timestamps: the contribution pairs those
  // rows alone, while the residual is taken against the mean of every portfolio
  // row, so the unmatched portfolio return stays unattributed instead of being
  // pushed into the style.
  const auto result = invoke_explicit(portfolio, styles, selected);
  expect_result(result, {"selected", "residual"}, {4.0, -1.5});
}

TEST(StyleRegressionAttribution,
     UsesZeroForShortAndConstantAlignedSamples) {
  const auto portfolio = make_portfolio_series();
  fincore::dataframe styles;
  styles.load_index({30, 10});
  styles.load_column<double>("short", {.5, .7},
                             hmdf::nan_policy::dont_pad_with_nans);
  styles.load_column<double>("flat", {1., 1.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 2U> selected{"short", "flat"};

  const auto result = invoke_explicit(portfolio, styles, selected);
  expect_result(result, {"short", "flat", "residual"},
                {0.0, 0.0, .03});

  fincore::dataframe full_flat;
  full_flat.load_index({30, 10, 50, 20, 40});
  full_flat.load_column<double>("flat", {2., 2., 2., 2., 2.},
                                hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> flat{"flat"};
  expect_result(invoke_explicit(portfolio, full_flat, flat),
                {"flat", "residual"}, {0.0, .03});

  fincore::dataframe flat_portfolio;
  flat_portfolio.load_index({1, 2, 3});
  flat_portfolio.load_column<double>("p", {2., 2., 2.},
                                     hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe variable_style;
  variable_style.load_index({1, 2, 3});
  variable_style.load_column<double>("selected", {1., 2., 4.},
                                     hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> variable{"selected"};
  expect_result(invoke_explicit(flat_portfolio, variable_style, variable),
                {"selected", "residual"}, {0.0, 2.0});
}

TEST(StyleRegressionAttribution,
     PreservesTheStrictPopulationStandardDeviationThreshold) {
  fincore::dataframe portfolio;
  portfolio.load_index({1, 2, 3, 4});
  portfolio.load_column<double>("p",
                                {-1.0e-15, -1.0e-15, 1.0e-15, 1.0e-15},
                                hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe styles;
  styles.load_index({1, 2, 3, 4});
  styles.load_column<double>("edge", {1., 1., 3., 3.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"edge"};

  const auto result = invoke_explicit(portfolio, styles, selected);
  const auto& contributions = result.get_column<double>(
      fincore::style_attribution_contribution_column.data());
  ASSERT_EQ(contributions.size(), 2U);
  EXPECT_GT(contributions[0], 0.0);
  EXPECT_NEAR(contributions[0], 2.0, 1.0e-12);
  EXPECT_NEAR(contributions[1], -2.0, 1.0e-12);

  fincore::dataframe below_portfolio;
  below_portfolio.load_index({1, 2, 3, 4});
  below_portfolio.load_column<double>(
      "p", {-5.0e-16, -5.0e-16, 5.0e-16, 5.0e-16},
      hmdf::nan_policy::dont_pad_with_nans);
  expect_result(invoke_explicit(below_portfolio, styles, selected),
                {"edge", "residual"}, {0.0, 0.0});

  fincore::dataframe regular_portfolio;
  regular_portfolio.load_index({1, 2, 3, 4});
  regular_portfolio.load_column<double>("p", {-1., -1., 1., 1.},
                                        hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe below_style;
  below_style.load_index({1, 2, 3, 4});
  below_style.load_column<double>(
      "edge", {1.0 - 4.0e-16, 1.0 - 4.0e-16, 1.0 + 4.0e-16,
               1.0 + 4.0e-16},
      hmdf::nan_policy::dont_pad_with_nans);
  expect_result(invoke_explicit(regular_portfolio, below_style, selected),
                {"edge", "residual"}, {0.0, 0.0});
}

TEST(StyleRegressionAttribution,
     ReturnsCanonicalEmptyAndResidualOnlySurfaces) {
  fincore::dataframe empty;
  expect_empty(fincore::calculate_regression_attribution(empty));
  expect_empty(fincore::calculate_regression_attribution(
      empty, make_style_returns(), std::span<const std::string_view>{}));

  fincore::dataframe zero_columns;
  zero_columns.load_index({1});
  expect_empty(fincore::calculate_regression_attribution(zero_columns));

  fincore::dataframe no_styles;
  no_styles.load_index({30, 10, 50, 20, 40});
  expect_result(fincore::calculate_regression_attribution(
                    make_portfolio_series(), no_styles,
                    std::span<const std::string_view>{}),
                {"residual"}, {.03});

  const std::array<std::string_view, 1U> missing{"missing"};
  expect_result(invoke_explicit(make_portfolio_series(), no_styles, missing),
                {"residual"}, {.03});

  fincore::dataframe typed_empty_style;
  typed_empty_style.load_column<double>(
      "empty", {}, hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> empty_style{"empty"};
  expect_result(invoke_explicit(make_portfolio_series(), typed_empty_style,
                                empty_style),
                {"empty", "residual"}, {0.0, .03});
}

TEST(StyleRegressionAttribution,
     IgnoresUnselectedMalformedColumnsButRejectsSelectedOnes) {
  fincore::dataframe styles;
  styles.load_index({30, 10, 50, 20, 40});
  styles.load_column<double>("selected", {.03, .01, .05, .02, .04},
                             hmdf::nan_policy::dont_pad_with_nans);
  styles.load_column<double>("unselected_nonfinite",
                             {std::numeric_limits<double>::infinity(), 0., 0.,
                              0., 0.},
                             hmdf::nan_policy::dont_pad_with_nans);
  styles.load_column<int>("metadata", {1, 2, 3, 4, 5},
                          hmdf::nan_policy::dont_pad_with_nans);
  styles.load_column<double>("", {std::numeric_limits<double>::infinity()},
                             hmdf::nan_policy::dont_pad_with_nans);
  styles.load_column<double>("unselected_short", {1.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"selected"};
  expect_result(invoke_explicit(make_portfolio_series(), styles, selected),
                {"selected", "residual"}, {.03, 0.0});

  const std::array<std::string_view, 1U> wrong_type{"metadata"};
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   make_portfolio_series(), styles, wrong_type)),
               fincore::input_error);
  const std::array<std::string_view, 1U> nonfinite{
      "unselected_nonfinite"};
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   make_portfolio_series(), styles, nonfinite)),
               fincore::input_error);
}

TEST(StyleRegressionAttribution,
     RejectsMalformedPortfolioAndSelectedStyleStructures) {
  fincore::dataframe mixed;
  mixed.load_index({1, 2, 3});
  mixed.load_column<double>("p", {1., 2., 3.},
                            hmdf::nan_policy::dont_pad_with_nans);
  mixed.load_column<int>("metadata", {1, 2, 3},
                         hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe styles;
  styles.load_index({1, 2, 3});
  styles.load_column<double>("s", {1., 2., 3.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"s"};
  EXPECT_THROW(static_cast<void>(invoke_explicit(mixed, styles, selected)),
               fincore::input_error);

  fincore::dataframe empty_name;
  empty_name.load_index({1, 2, 3});
  empty_name.load_column<double>("", {1., 2., 3.},
                                 hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   empty_name, styles, selected)),
               fincore::input_error);

  fincore::dataframe short_portfolio;
  short_portfolio.load_index({1, 2, 3});
  short_portfolio.load_column<double>("p", {1., 2.},
                                      hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   short_portfolio, styles, selected)),
               fincore::input_error);

  fincore::dataframe short_style;
  short_style.load_index({1, 2, 3});
  short_style.load_column<double>("s", {1., 2.},
                                  hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   make_portfolio_series(), short_style, selected)),
               fincore::input_error);

  for (const double invalid : {
           std::numeric_limits<double>::quiet_NaN(),
           std::numeric_limits<double>::infinity(),
           -std::numeric_limits<double>::infinity()}) {
    fincore::dataframe invalid_portfolio;
    invalid_portfolio.load_index({1, 2, 3});
    invalid_portfolio.load_column<double>("p", {1., invalid, 3.},
                                          hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(invoke_explicit(
                     invalid_portfolio, styles, selected)),
                 fincore::input_error);
  }
}

TEST(StyleRegressionAttribution, RejectsFiniteIntermediateOverflow) {
  const double largest{std::numeric_limits<double>::max()};
  fincore::dataframe styles;
  styles.load_index({1, 2, 3});
  styles.load_column<double>("selected", {1., 2., 3.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"selected"};

  fincore::dataframe row_sum_overflow;
  row_sum_overflow.load_index({1, 2, 3});
  row_sum_overflow.load_column<double>("left", {largest, 1., 2.},
                                       hmdf::nan_policy::dont_pad_with_nans);
  row_sum_overflow.load_column<double>("right", {largest, 1., 2.},
                                       hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   row_sum_overflow, styles, selected)),
               fincore::input_error);

  fincore::dataframe portfolio;
  portfolio.load_index({1, 2, 3});
  portfolio.load_column<double>("p", {1., 2., 3.},
                                hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe centered_overflow;
  centered_overflow.load_index({1, 2, 3});
  centered_overflow.load_column<double>("selected",
                                        {largest, -largest, largest},
                                        hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   portfolio, centered_overflow, selected)),
               fincore::input_error);
}

TEST(StyleRegressionAttribution,
     RejectsDuplicateExplicitIndexesAndInvalidSelectors) {
  fincore::dataframe duplicate_portfolio;
  duplicate_portfolio.load_index({1, 1, 2});
  duplicate_portfolio.load_column<double>("p", {1., 2., 3.},
                                          hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe styles;
  styles.load_index({1, 2, 3});
  styles.load_column<double>("s", {1., 2., 3.},
                             hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 1U> selected{"s"};
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   duplicate_portfolio, styles, selected)),
               fincore::input_error);

  fincore::dataframe duplicate_styles;
  duplicate_styles.load_index({1, 1, 2});
  duplicate_styles.load_column<double>("s", {1., 2., 3.},
                                       hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(invoke_explicit(
                   styles, duplicate_styles, selected)),
               fincore::input_error);

  const std::array<std::string_view, 1U> empty_name{""};
  const std::array<std::string_view, 1U> residual{"residual"};
  const std::array<std::string_view, 2U> duplicate{"s", "s"};
  const std::string embedded{"s\0evil", 6U};
  const std::array<std::string_view, 1U> embedded_name{embedded};
  const std::string too_long(fincore::dataframe::ColNameType::capacity(), 'x');
  const std::array<std::string_view, 1U> long_name{too_long};
  for (const auto selector : {
           std::span<const std::string_view>{empty_name},
           std::span<const std::string_view>{residual},
           std::span<const std::string_view>{duplicate},
           std::span<const std::string_view>{embedded_name},
           std::span<const std::string_view>{long_name}}) {
    EXPECT_THROW(static_cast<void>(fincore::calculate_regression_attribution(
                     styles, styles, selector)),
                 fincore::input_error);
  }
}

TEST(StyleRegressionAttribution,
     MatchesAutomaticSourceOracleWithDuplicateRawLabels) {
  const auto result =
      fincore::calculate_regression_attribution(make_asset_returns());
  expect_result(
      result,
      {"equal_weight", "winner", "loser", "value", "growth", "high",
       "low", "residual"},
      {.009333333333333332, .002613098319673955,
       .0007593263966019986, .009333333333333332,
       .009333333333333332, 0.0, 0.0, -.02203909138294262});
}

TEST(StyleRegressionAttribution, OwnsInputsSelectorsNamesAndValues) {
  auto portfolio = make_portfolio_series();
  auto styles = make_style_returns();
  std::array<std::string, 2U> storage{"growth", "value"};
  const std::array<std::string_view, 2U> selected{storage[0], storage[1]};
  auto result = invoke_explicit(portfolio, styles, selected);

  portfolio.get_column<double>("portfolio")[0] = 999.;
  styles.get_column<double>("growth")[0] = 999.;
  storage[0] = "changed";
  expect_result(result, {"growth", "value", "residual"},
                {.06, -.03, 0.0});
}

}  // namespace
