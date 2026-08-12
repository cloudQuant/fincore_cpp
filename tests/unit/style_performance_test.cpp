#include <fincore/attribution/style.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
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
static_assert(std::is_same_v<decltype(fincore::analyze_performance_by_style(
                                 std::declval<const fincore::dataframe &>())),
                             fincore::dataframe>);

constexpr double tolerance{1.0e-12};

fincore::dataframe make_oracle_returns() {
  fincore::dataframe returns;
  returns.load_index({42, -7, 42, 100, 3});
  returns.load_column<double>("ALPHA", {.01, -.02, .03, 0.0, -.01},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("BETA", {.02, .01, -.01, .04, 0.0},
                              hmdf::nan_policy::dont_pad_with_nans);
  returns.load_column<double>("GAMMA", {-.01, .03, .02, -.02, .05},
                              hmdf::nan_policy::dont_pad_with_nans);
  return returns;
}

fincore::dataframe make_exposures(
    std::vector<std::string> assets,
    std::vector<std::pair<std::string, std::vector<double>>> styles) {
  std::vector<fincore::timestamp> index;
  index.reserve(assets.size());
  for (std::size_t row{0U}; row < assets.size(); ++row) {
    index.push_back(static_cast<fincore::timestamp>(row));
  }

  fincore::dataframe exposures;
  exposures.load_index(std::move(index));
  exposures.load_column<std::string>(fincore::style_asset_column.data(),
                                     std::move(assets),
                                     hmdf::nan_policy::dont_pad_with_nans);
  for (auto &[name, values] : styles) {
    exposures.load_column<double>(name.c_str(), std::move(values),
                                  hmdf::nan_policy::dont_pad_with_nans);
  }
  return exposures;
}

fincore::dataframe make_oracle_exposures() {
  return make_exposures({"ALPHA", "BETA", "GAMMA"},
                        {{"blend", {2.0, 1.0, 0.0}},
                         {"hedged", {1.0, -.5, .5}},
                         {"zero", {0.0, 0.0, 0.0}},
                         {"negative_total", {-1.0, 0.0, 0.0}}});
}

void expect_empty(const fincore::dataframe &result) {
  EXPECT_EQ(result.shape(), (std::pair<std::size_t, std::size_t>{0U, 0U}));
  EXPECT_TRUE(result.get_index().empty());
  EXPECT_TRUE(result.get_columns_info<double>().empty());
  EXPECT_TRUE(result.get_columns_info<std::string>().empty());
}

void expect_schema(const fincore::dataframe &result,
                   const std::vector<std::string> &names,
                   const std::size_t rows) {
  EXPECT_EQ(result.shape(),
            (std::pair<std::size_t, std::size_t>{rows, names.size()}));
  ASSERT_EQ(result.get_index().size(), rows);
  for (std::size_t row{0U}; row < rows; ++row) {
    EXPECT_EQ(result.get_index()[row], static_cast<fincore::timestamp>(row));
  }
  EXPECT_TRUE(result.get_columns_info<std::string>().empty());
  EXPECT_TRUE(result.get_columns_info<int>().empty());
  const auto columns = result.get_columns_info<double>();
  ASSERT_EQ(columns.size(), names.size());
  for (std::size_t column{0U}; column < names.size(); ++column) {
    EXPECT_EQ(std::string{std::get<0>(columns[column]).c_str()}, names[column]);
    EXPECT_EQ(std::get<1>(columns[column]), rows);
    EXPECT_EQ(std::get<2>(columns[column]), typeid(double));
  }
}

void expect_values(const fincore::dataframe &result, const std::string &name,
                   const std::array<double, 5U> &expected) {
  const auto &values = result.get_column<double>(name.c_str());
  ASSERT_EQ(values.size(), expected.size());
  for (std::size_t row{0U}; row < expected.size(); ++row) {
    EXPECT_NEAR(values[row], expected[row], tolerance);
  }
}

void invoke_style_performance(const fincore::dataframe &returns,
                              const fincore::dataframe &exposures) {
  static_cast<void>(fincore::analyze_performance_by_style(returns, exposures));
}

TEST(StylePerformance, MatchesExplicitPythonOracleAndOwnsFreshOutput) {
  auto returns = make_oracle_returns();
  auto exposures = make_oracle_exposures();
  auto result = fincore::analyze_performance_by_style(returns, exposures);

  expect_schema(
      result,
      {"blend_return", "hedged_return", "zero_return", "negative_total_return"},
      5U);
  expect_values(result, "blend_return",
                {.013333333333333334, -.01, .016666666666666666,
                 .013333333333333334, -.006666666666666667});
  expect_values(result, "hedged_return", {-.005, -.01, .045, -.03, .015});
  expect_values(result, "zero_return", {0.0, 0.0, 0.0, 0.0, 0.0});
  expect_values(result, "negative_total_return", {0.0, 0.0, 0.0, 0.0, 0.0});

  returns.get_column<double>("ALPHA")[0] = 999.0;
  exposures.get_column<double>("blend")[0] = 999.0;
  exposures.get_column<std::string>(fincore::style_asset_column.data())[0] =
      "changed";
  expect_values(result, "blend_return",
                {.013333333333333334, -.01, .016666666666666666,
                 .013333333333333334, -.006666666666666667});
}

TEST(StylePerformance, MatchesAutomaticPythonOracleThroughPublicStyleAnalysis) {
  const auto returns = make_oracle_returns();
  const auto result = fincore::analyze_performance_by_style(returns);

  expect_schema(result,
                {"equal_weight_return", "winner_return", "loser_return",
                 "value_return", "growth_return", "high_return", "low_return"},
                5U);
  expect_values(result, "equal_weight_return",
                {.006666666666666667, .006666666666666666, .013333333333333332,
                 .006666666666666667, .013333333333333334});
  expect_values(result, "winner_return", {.005, .02, .005, .01, .025});
  expect_values(result, "loser_return", {.01, -.02, .03, 0.0, -.01});
  expect_values(result, "value_return",
                {.006666666666666667, .006666666666666666, .013333333333333332,
                 .006666666666666667, .013333333333333334});
  expect_values(result, "growth_return",
                {.006666666666666667, .006666666666666666, .013333333333333332,
                 .006666666666666667, .013333333333333334});
  expect_values(result, "high_return", {0.0, 0.0, 0.0, 0.0, 0.0});
  expect_values(result, "low_return", {0.0, 0.0, 0.0, 0.0, 0.0});
}

TEST(StylePerformance, AlignsReorderedRowsByExactAssetIdentity) {
  const auto returns = make_oracle_returns();
  const auto reordered = make_exposures({"GAMMA", "ALPHA", "BETA"},
                                        {{"blend", {0.0, 2.0, 1.0}},
                                         {"hedged", {.5, 1.0, -.5}},
                                         {"zero", {0.0, 0.0, 0.0}},
                                         {"negative_total", {0.0, -1.0, 0.0}}});

  const auto result = fincore::analyze_performance_by_style(returns, reordered);
  expect_schema(
      result,
      {"blend_return", "hedged_return", "zero_return", "negative_total_return"},
      5U);
  expect_values(result, "blend_return",
                {.013333333333333334, -.01, .016666666666666666,
                 .013333333333333334, -.006666666666666667});
  expect_values(result, "hedged_return", {-.005, -.01, .045, -.03, .015});
}

TEST(StylePerformance, ZeroFillsMissingAssetsAndIgnoresExtraAssets) {
  const auto returns = make_oracle_returns();
  const auto exposures = make_exposures(
      {"DELTA", "BETA", "ALPHA"},
      {{"selected", {100.0, 1.0, 3.0}}, {"missing_only", {2.0, 0.0, 0.0}}});

  const auto result = fincore::analyze_performance_by_style(returns, exposures);
  expect_schema(result, {"selected_return", "missing_only_return"}, 5U);
  expect_values(result, "selected_return", {.0125, -.0125, .02, .01, -.0075});
  expect_values(result, "missing_only_return", {0.0, 0.0, 0.0, 0.0, 0.0});
}

TEST(StylePerformance, EmptySurfacesShortCircuitToCanonicalZeroByZeroFrame) {
  fincore::dataframe zero_rows;
  zero_rows.load_index({});
  zero_rows.load_column<int>("malformed", {},
                             hmdf::nan_policy::dont_pad_with_nans);
  expect_empty(fincore::analyze_performance_by_style(zero_rows));
  expect_empty(fincore::analyze_performance_by_style(zero_rows,
                                                     make_oracle_exposures()));

  fincore::dataframe zero_columns;
  zero_columns.load_index({42});
  expect_empty(fincore::analyze_performance_by_style(zero_columns));
  expect_empty(fincore::analyze_performance_by_style(zero_columns,
                                                     make_oracle_exposures()));

  fincore::dataframe zero_exposure_rows;
  zero_exposure_rows.load_index({});
  zero_exposure_rows.load_column<int>("malformed", {},
                                      hmdf::nan_policy::dont_pad_with_nans);
  expect_empty(fincore::analyze_performance_by_style(make_oracle_returns(),
                                                     zero_exposure_rows));

  const auto asset_only = make_exposures({"ALPHA", "BETA", "GAMMA"}, {});
  expect_empty(
      fincore::analyze_performance_by_style(make_oracle_returns(), asset_only));
}

TEST(StylePerformance, RejectsMalformedReturnStructuresAndValues) {
  fincore::dataframe wrong_type;
  wrong_type.load_index({1});
  wrong_type.load_column<int>("ALPHA", {1},
                              hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(wrong_type, make_oracle_exposures()),
               fincore::input_error);

  fincore::dataframe mixed;
  mixed.load_index({1});
  mixed.load_column<double>("ALPHA", {.01},
                            hmdf::nan_policy::dont_pad_with_nans);
  mixed.load_column<int>("metadata", {1}, hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(mixed, make_oracle_exposures()),
               fincore::input_error);

  fincore::dataframe short_column;
  short_column.load_index({1, 2});
  short_column.load_column<double>("ALPHA", {.01},
                                   hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(short_column, make_oracle_exposures()),
               fincore::input_error);

  fincore::dataframe empty_name;
  empty_name.load_index({1});
  empty_name.load_column<double>("", {.01},
                                 hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(empty_name, make_oracle_exposures()),
               fincore::input_error);

  for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
    fincore::dataframe returns;
    returns.load_index({1});
    returns.load_column<double>("ALPHA", {invalid},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(invoke_style_performance(
                     returns, make_exposures({"ALPHA"}, {{"style", {1.0}}})),
                 fincore::input_error);
  }
}

TEST(StylePerformance, RejectsMalformedExposureStructureAndIdentities) {
  const auto returns = make_oracle_returns();

  auto nonordinal_index = make_oracle_exposures();
  nonordinal_index.load_index({0, 2, 1});
  EXPECT_THROW(invoke_style_performance(returns, nonordinal_index),
               fincore::input_error);

  fincore::dataframe missing_asset;
  missing_asset.load_index({0, 1, 2});
  missing_asset.load_column<double>("style", {1.0, 1.0, 1.0},
                                    hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(returns, missing_asset),
               fincore::input_error);

  auto two_string_columns = make_oracle_exposures();
  two_string_columns.load_column<std::string>(
      "metadata", {"a", "b", "c"}, hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(returns, two_string_columns),
               fincore::input_error);

  auto mixed = make_oracle_exposures();
  mixed.load_column<int>("metadata", {1, 2, 3},
                         hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(returns, mixed), fincore::input_error);

  fincore::dataframe short_asset;
  short_asset.load_index({0, 1, 2});
  short_asset.load_column<std::string>(fincore::style_asset_column.data(),
                                       {"ALPHA", "BETA"},
                                       hmdf::nan_policy::dont_pad_with_nans);
  short_asset.load_column<double>("style", {1.0, 1.0, 1.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(returns, short_asset),
               fincore::input_error);

  fincore::dataframe short_style;
  short_style.load_index({0, 1, 2});
  short_style.load_column<std::string>(fincore::style_asset_column.data(),
                                       {"ALPHA", "BETA", "GAMMA"},
                                       hmdf::nan_policy::dont_pad_with_nans);
  short_style.load_column<double>("style", {1.0, 1.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(returns, short_style),
               fincore::input_error);

  for (const std::vector<std::string> &identities :
       {std::vector<std::string>{"", "BETA", "GAMMA"},
        std::vector<std::string>{"ALPHA", "ALPHA", "GAMMA"},
        std::vector<std::string>{std::string{"AL\0PHA", 6U}, "BETA",
                                 "GAMMA"}}) {
    EXPECT_THROW(
        invoke_style_performance(
            returns, make_exposures(identities, {{"style", {1, 1, 1}}})),
        fincore::input_error);
  }

  auto empty_style_name =
      make_exposures({"ALPHA", "BETA", "GAMMA"}, {{"", {1, 1, 1}}});
  EXPECT_THROW(invoke_style_performance(returns, empty_style_name),
               fincore::input_error);

  const std::string unrepresentable_style_name(
      fincore::dataframe::ColNameType::capacity() - 1U, 's');
  EXPECT_THROW(
      invoke_style_performance(
          returns, make_exposures({"ALPHA", "BETA", "GAMMA"},
                                  {{unrepresentable_style_name, {1, 1, 1}}})),
      fincore::input_error);
}

TEST(StylePerformance, RejectsNonfiniteWeightsAndArithmetic) {
  const auto returns = make_oracle_returns();
  for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
    EXPECT_THROW(invoke_style_performance(
                     returns, make_exposures({"ALPHA", "BETA", "GAMMA"},
                                             {{"style", {invalid, 0.0, 0.0}}})),
                 fincore::input_error);
  }

  const double maximum{std::numeric_limits<double>::max()};
  EXPECT_THROW(invoke_style_performance(
                   returns, make_exposures({"ALPHA", "BETA", "GAMMA"},
                                           {{"sum", {maximum, maximum, 0.0}}})),
               fincore::input_error);

  EXPECT_THROW(
      invoke_style_performance(
          returns,
          make_exposures({"ALPHA", "BETA", "GAMMA"},
                         {{"normalize",
                           {maximum, -maximum,
                            std::numeric_limits<double>::denorm_min()}}})),
      fincore::input_error);

  fincore::dataframe product_returns;
  product_returns.load_index({0});
  product_returns.load_column<double>("ALPHA", {maximum},
                                      hmdf::nan_policy::dont_pad_with_nans);
  product_returns.load_column<double>("BETA", {0.0},
                                      hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(invoke_style_performance(
                   product_returns, make_exposures({"ALPHA", "BETA"},
                                                   {{"product", {2.0, -1.0}}})),
               fincore::input_error);

  fincore::dataframe maximum_returns;
  maximum_returns.load_index({0});
  maximum_returns.load_column<double>("ALPHA", {maximum},
                                      hmdf::nan_policy::dont_pad_with_nans);
  maximum_returns.load_column<double>("BETA", {maximum},
                                      hmdf::nan_policy::dont_pad_with_nans);
  maximum_returns.load_column<double>("GAMMA", {-maximum},
                                      hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(
      invoke_style_performance(maximum_returns,
                               make_exposures({"ALPHA", "BETA", "GAMMA"},
                                              {{"row_sum", {1.0, 1.0, -1.0}}})),
      fincore::input_error);
}

TEST(StylePerformance, RemainsValueOwningAfterInputsAreDestroyed) {
  const auto retained = [] {
    auto returns = make_oracle_returns();
    auto exposures = make_oracle_exposures();
    return fincore::analyze_performance_by_style(returns, exposures);
  }();

  expect_schema(
      retained,
      {"blend_return", "hedged_return", "zero_return", "negative_total_return"},
      5U);
  expect_values(retained, "hedged_return", {-.005, -.01, .045, -.03, .015});
}

} // namespace
