#include <fincore/metrics/perf_attrib.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
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

fincore::dataframe factor_returns() {
  fincore::dataframe factors;
  factors.load_index(std::vector<timestamp>(dates()));
  factors.load_column<double>("market", {.01, .02, -.01},
                              hmdf::nan_policy::dont_pad_with_nans);
  factors.load_column<double>("value", {.03, -.02, .04},
                              hmdf::nan_policy::dont_pad_with_nans);
  return factors;
}

fincore::dataframe strategy_returns() {
  return fincore::make_series(std::vector<timestamp>(dates()), "returns",
                              {.02, -.01, .03});
}

template <std::size_t count>
void expect_columns(const fincore::dataframe &frame,
                    const std::array<std::string_view, count> &expected) {
  const auto columns = frame.get_columns_info<double>();
  ASSERT_EQ(columns.size(), expected.size());
  for (std::size_t position = 0U; position < expected.size(); ++position) {
    EXPECT_STREQ(std::get<0>(columns[position]).c_str(),
                 std::string{expected[position]}.c_str());
    EXPECT_EQ(std::get<2>(columns[position]), typeid(double));
  }
}

void expect_same_double_frame(const fincore::dataframe &actual,
                              const fincore::dataframe &expected) {
  EXPECT_EQ(actual.get_index(), expected.get_index());
  const auto actual_columns = actual.get_columns_info<double>();
  const auto expected_columns = expected.get_columns_info<double>();
  ASSERT_EQ(actual_columns.size(), expected_columns.size());
  for (std::size_t position = 0U; position < expected_columns.size();
       ++position) {
    EXPECT_STREQ(std::get<0>(actual_columns[position]).c_str(),
                 std::get<0>(expected_columns[position]).c_str());
    EXPECT_EQ(std::get<2>(actual_columns[position]), typeid(double));
    EXPECT_EQ(std::get<2>(actual_columns[position]),
              std::get<2>(expected_columns[position]));
    const auto name = std::get<0>(expected_columns[position]);
    EXPECT_EQ(actual.get_column<double>(name.c_str()),
              expected.get_column<double>(name.c_str()));
  }
}

TEST(PerfAttrib, NormalizesDollarPositionsAndComputesOrderedExposures) {
  const auto normalized =
      fincore::normalize_attribution_positions(dollar_positions());
  EXPECT_EQ(normalized.get_index(), dates());
  ASSERT_FALSE(normalized.has_column("cash"));
  ASSERT_FALSE(normalized.has_column("metadata"));
  expect_columns(normalized, std::array<std::string_view, 2U>{"A", "B"});
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

  auto wrong_factor = loadings;
  wrong_factor.remove_column<double>("market");
  wrong_factor.load_column<int>("market", {1, 2, 3, 4, 5, 6},
                                hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::compute_exposures(positions, wrong_factor, market),
               fincore::input_error);
}

TEST(PerfAttrib, RejectsDuplicatePositionIndexesInPublicPrimitives) {
  fincore::dataframe duplicate_positions;
  duplicate_positions.load_index({dates()[0], dates()[0]});
  duplicate_positions.load_column<double>("A", {.5, .5},
                                          hmdf::nan_policy::dont_pad_with_nans);
  constexpr std::array<std::string_view, 1U> market{"market"};

  EXPECT_THROW(fincore::normalize_attribution_positions(duplicate_positions),
               fincore::input_error);
  EXPECT_THROW(fincore::compute_exposures(duplicate_positions,
                                          factor_loadings(), market),
               fincore::input_error);
}

TEST(PerfAttrib, RejectsEmptyPositionAssetNamesAndLoadingSymbols) {
  fincore::dataframe empty_asset_positions;
  empty_asset_positions.load_index({dates()[0]});
  empty_asset_positions.load_column<double>(
      "", {.5}, hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::normalize_attribution_positions(empty_asset_positions),
               fincore::input_error);

  fincore::dataframe loadings_with_empty_symbol;
  loadings_with_empty_symbol.load_index({dates()[0], dates()[0], dates()[1],
                                         dates()[1], dates()[2], dates()[2],
                                         dates()[0]});
  loadings_with_empty_symbol.load_column<std::string>(
      "symbol", {"A", "B", "A", "B", "A", "B", ""},
      hmdf::nan_policy::dont_pad_with_nans);
  loadings_with_empty_symbol.load_column<double>(
      "market", {.2, .4, .5, .2, .1, .6, .0},
      hmdf::nan_policy::dont_pad_with_nans);
  constexpr std::array<std::string_view, 1U> market{"market"};
  EXPECT_THROW(fincore::compute_exposures(
                   fincore::normalize_attribution_positions(dollar_positions()),
                   loadings_with_empty_symbol, market),
               fincore::input_error);
}

TEST(PerfAttrib, NormalizesNanPositionsAndSumsAllNanProductsToZero) {
  fincore::dataframe positions;
  positions.load_index(std::vector<timestamp>(dates()));
  positions.load_column<double>("A", {std::numeric_limits<double>::quiet_NaN(),
                                       2.0, 3.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("B", {4.0, 2.0, 1.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  const auto normalized = fincore::normalize_attribution_positions(positions);
  EXPECT_TRUE(std::isnan(normalized.get_column<double>("A")[0]));
  EXPECT_DOUBLE_EQ(normalized.get_column<double>("B")[0], 1.0);

  auto loadings = factor_loadings();
  loadings.remove_column<double>("market");
  loadings.load_column<double>(
      "market", {std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::quiet_NaN(), .5, .2, .1, .6},
      hmdf::nan_policy::dont_pad_with_nans);
  constexpr std::array<std::string_view, 1U> market{"market"};
  const auto exposures = fincore::compute_exposures(normalized, loadings, market);
  EXPECT_DOUBLE_EQ(exposures.get_column<double>("market")[0], 0.0);

  fincore::dataframe zero_gross;
  zero_gross.load_index({dates()[0]});
  zero_gross.load_column<double>("A", {0.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
  const auto zero_normalized =
      fincore::normalize_attribution_positions(zero_gross);
  EXPECT_TRUE(std::isnan(zero_normalized.get_column<double>("A")[0]));

  auto infinite_loadings = factor_loadings();
  infinite_loadings.remove_column<double>("market");
  infinite_loadings.load_column<double>(
      "market", {std::numeric_limits<double>::infinity(), .4, .5, .2, .1, .6},
      hmdf::nan_policy::dont_pad_with_nans);
  const auto infinite =
      fincore::compute_exposures(fincore::normalize_attribution_positions(
                                     dollar_positions()),
                                 infinite_loadings, market);
  EXPECT_TRUE(std::isinf(infinite.get_column<double>("market")[0]));
  EXPECT_GT(infinite.get_column<double>("market")[0], 0.0);
}

TEST(PerfAttrib, ProducesCoreAttributionStatisticsAndCostAdjustedReturns) {
  const auto normalized =
      fincore::normalize_attribution_positions(dollar_positions());
  const auto core = fincore::perf_attrib_core(strategy_returns(), normalized,
                                               factor_returns(), factor_loadings());
  const auto dollar = fincore::perf_attrib(strategy_returns(), dollar_positions(),
                                           factor_returns(), factor_loadings());
  const auto weights = fincore::perf_attrib(strategy_returns(), normalized,
                                            factor_returns(), factor_loadings(), false);

  EXPECT_EQ(core.risk_exposures.get_index(), dates());
  EXPECT_EQ(core.attribution.get_index(), dates());
  expect_columns(core.risk_exposures,
                 std::array<std::string_view, 2U>{"market", "value"});
  const auto attribution_columns = core.attribution.get_columns_info<double>();
  const std::array<std::string_view, 7U> expected_attribution_columns{
      "market", "value", "total_returns", "common_returns", "specific_returns",
      "tilt_returns", "timing_returns"};
  ASSERT_EQ(attribution_columns.size(), expected_attribution_columns.size());
  for (std::size_t position = 0U; position < expected_attribution_columns.size();
       ++position) {
    EXPECT_STREQ(std::get<0>(attribution_columns[position]).c_str(),
                 std::string{expected_attribution_columns[position]}.c_str());
    EXPECT_EQ(std::get<2>(attribution_columns[position]), typeid(double));
  }
  for (const auto *result : {&dollar, &weights}) {
    expect_same_double_frame(result->risk_exposures, core.risk_exposures);
    expect_same_double_frame(result->attribution, core.attribution);
  }

  const auto &market = core.attribution.get_column<double>("market");
  const auto &value = core.attribution.get_column<double>("value");
  const auto &common = core.attribution.get_column<double>("common_returns");
  const auto &specific = core.attribution.get_column<double>("specific_returns");
  const auto &tilt = core.attribution.get_column<double>("tilt_returns");
  const auto &timing = core.attribution.get_column<double>("timing_returns");
  const std::array<double, 3U> expected_market{.003428571428571, .0065, -.0025};
  const std::array<double, 3U> expected_value{-.000857142857143, -.0, -.012};
  const std::array<double, 3U> expected_common{.002571428571429, .0065, -.0145};
  const std::array<double, 3U> expected_specific{.017428571428571, -.0165, .0445};
  const std::array<double, 3U> expected_tilt{-.000226190476190, .008309523809524,
                                             -.007440476190476};
  const std::array<double, 3U> expected_timing{.002797619047619, -.001809523809524,
                                               -.007059523809524};
  for (std::size_t row = 0U; row < expected_market.size(); ++row) {
    EXPECT_NEAR(market[row], expected_market[row], 1.0e-12);
    EXPECT_NEAR(value[row], expected_value[row], 1.0e-12);
    EXPECT_NEAR(common[row], expected_common[row], 1.0e-12);
    EXPECT_NEAR(specific[row], expected_specific[row], 1.0e-12);
    EXPECT_NEAR(tilt[row], expected_tilt[row], 1.0e-12);
    EXPECT_NEAR(timing[row], expected_timing[row], 1.0e-12);
  }

  const auto statistics = fincore::create_perf_attrib_stats(core);
  const std::array<std::pair<std::string_view, double>, 7U> expected_summary{{
      {"Annualized Specific Return", 39.89648506339985},
      {"Annualized Common Return", -.37309539033531},
      {"Annualized Total Return", 26.170501183432872},
      {"Specific Sharpe Ratio", 7.864941869197365},
      {"Cumulative Specific Return", .0451695245},
      {"Cumulative Common Return", -.005543635214286},
      {"Total Returns", .040094},
  }};
  for (const auto &[name, expected] : expected_summary) {
    ASSERT_TRUE(statistics.summary.contains(std::string{name}));
    EXPECT_NEAR(statistics.summary.at(std::string{name}), expected, 1.0e-12);
  }
  const auto total_frame = fincore::make_series(
      std::vector<timestamp>(core.attribution.get_index()), "returns",
      std::vector<double>(core.attribution.get_column<double>("total_returns").begin(),
                          core.attribution.get_column<double>("total_returns").end()));
  const auto common_frame = fincore::make_series(
      std::vector<timestamp>(core.attribution.get_index()), "returns",
      std::vector<double>(core.attribution.get_column<double>("common_returns").begin(),
                          core.attribution.get_column<double>("common_returns").end()));
  const auto specific_frame = fincore::make_series(
      std::vector<timestamp>(core.attribution.get_index()), "returns",
      std::vector<double>(core.attribution.get_column<double>("specific_returns").begin(),
                          core.attribution.get_column<double>("specific_returns").end()));
  EXPECT_NEAR(statistics.summary.at("Annualized Specific Return"),
              fincore::annual_return(specific_frame), 1.0e-12);
  EXPECT_NEAR(statistics.summary.at("Annualized Common Return"),
              fincore::annual_return(common_frame), 1.0e-12);
  EXPECT_NEAR(statistics.summary.at("Annualized Total Return"),
              fincore::annual_return(total_frame), 1.0e-12);
  EXPECT_NEAR(statistics.summary.at("Specific Sharpe Ratio"),
              fincore::sharpe_ratio(specific_frame), 1.0e-12);
  EXPECT_NEAR(statistics.summary.at("Cumulative Specific Return"),
              fincore::cumulative_return(specific_frame), 1.0e-12);
  EXPECT_NEAR(statistics.summary.at("Cumulative Common Return"),
              fincore::cumulative_return(common_frame), 1.0e-12);
  EXPECT_NEAR(statistics.summary.at("Total Returns"),
              fincore::cumulative_return(total_frame), 1.0e-12);
  EXPECT_EQ(statistics.factor_summary.get_index(),
            (std::vector<timestamp>{0, 1}));
  const auto factor_summary_columns =
      statistics.factor_summary.get_columns_info<std::string, double>();
  const std::array<std::string_view, 4U> expected_factor_summary_columns{
      "factor", "Average Risk Factor Exposure", "Annualized Return",
      "Cumulative Return"};
  ASSERT_EQ(factor_summary_columns.size(), expected_factor_summary_columns.size());
  for (std::size_t position = 0U; position < expected_factor_summary_columns.size();
       ++position) {
    EXPECT_STREQ(std::get<0>(factor_summary_columns[position]).c_str(),
                 std::string{expected_factor_summary_columns[position]}.c_str());
    EXPECT_EQ(std::get<2>(factor_summary_columns[position]),
              position == 0U ? typeid(std::string) : typeid(double));
  }
  EXPECT_EQ(statistics.factor_summary.get_column<std::string>("factor"),
            (std::vector<std::string>{"market", "value"}));
  const auto &average = statistics.factor_summary.get_column<double>(
      "Average Risk Factor Exposure");
  const auto &annualized =
      statistics.factor_summary.get_column<double>("Annualized Return");
  const auto &cumulative =
      statistics.factor_summary.get_column<double>("Cumulative Return");
  EXPECT_NEAR(average[0], .305952380952381, 1.0e-12);
  EXPECT_NEAR(average[1], -.109523809523810, 1.0e-12);
  EXPECT_NEAR(annualized[0], .861676871127715, 1.0e-12);
  EXPECT_NEAR(annualized[1], -.662479050655964, 1.0e-12);
  EXPECT_NEAR(cumulative[0], .00742598, 1.0e-12);
  EXPECT_NEAR(cumulative[1], -.012846857142857, 1.0e-12);

  const auto returns = strategy_returns();
  const auto no_costs = fincore::cumulative_returns_less_costs(returns);
  EXPECT_EQ(no_costs.get_column<double>("returns"),
            fincore::cumulative_returns(returns).get_column<double>("returns"));
  const auto costs = fincore::make_series(std::vector<timestamp>(dates()), "returns",
                                          {.001, .002, .003});
  const auto after_costs = fincore::cumulative_returns_less_costs(returns, costs);
  const std::array<double, 3U> expected_after_costs{.019, .006772, .033954844};
  for (std::size_t row = 0U; row < expected_after_costs.size(); ++row) {
    EXPECT_NEAR(after_costs.get_column<double>("returns")[row],
                expected_after_costs[row], 1.0e-12);
  }
}

TEST(PerfAttrib, ValidatesCoreSchemasAndSupportsEmptyAndCustomInputs) {
  fincore::dataframe empty_positions;
  empty_positions.load_index({});
  empty_positions.load_column<double>("A", {}, hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe empty_factors;
  empty_factors.load_index({});
  empty_factors.load_column<double>("market", {},
                                    hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe empty_loadings;
  empty_loadings.load_index({});
  empty_loadings.load_column<std::string>("symbol", {},
                                          hmdf::nan_policy::dont_pad_with_nans);
  empty_loadings.load_column<double>("market", {},
                                     hmdf::nan_policy::dont_pad_with_nans);
  const auto empty_returns = fincore::make_series({}, "returns", {});
  const auto empty = fincore::perf_attrib_core(empty_returns, empty_positions,
                                                empty_factors, empty_loadings);
  EXPECT_TRUE(empty.risk_exposures.get_index().empty());
  EXPECT_TRUE(empty.attribution.get_index().empty());
  expect_columns(empty.risk_exposures,
                 std::array<std::string_view, 1U>{"market"});
  expect_columns(empty.attribution,
                 std::array<std::string_view, 6U>{
                     "market", "total_returns", "common_returns",
                     "specific_returns", "tilt_returns", "timing_returns"});
  EXPECT_EQ(empty.risk_exposures.get_column<double>("market").size(), 0U);

  fincore::dataframe positions;
  positions.load_index(std::vector<timestamp>(dates()));
  positions.load_column<double>("A", {1.0, .5, -.5},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("balance", {.2, .1, .3},
                                hmdf::nan_policy::dont_pad_with_nans);
  auto custom_loadings = factor_loadings();
  custom_loadings.remove_column<std::string>("symbol");
  custom_loadings.load_column<std::string>("ticker", {"A", "B", "A", "B", "A", "B"},
                                           hmdf::nan_policy::dont_pad_with_nans);
  const auto custom_returns = fincore::make_series(std::vector<timestamp>(dates()),
                                                    "strategy", {.02, -.01, .03});
  const auto custom = fincore::perf_attrib(custom_returns, positions,
                                            factor_returns(), custom_loadings, false,
                                            "strategy", "balance", "ticker");
  EXPECT_EQ(custom.attribution.get_column<double>("total_returns"),
            custom_returns.get_column<double>("strategy"));

  auto mismatched_returns = strategy_returns();
  mismatched_returns.load_index({dates()[1], dates()[0], dates()[2]});
  EXPECT_THROW(fincore::perf_attrib_core(mismatched_returns, empty_positions,
                                         empty_factors, empty_loadings),
               fincore::input_error);
  fincore::dataframe no_factors;
  no_factors.load_index(std::vector<timestamp>(dates()));
  no_factors.load_column<int>("market", {1, 2, 3},
                               hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::perf_attrib_core(strategy_returns(),
                                         fincore::normalize_attribution_positions(
                                             dollar_positions()),
                                         no_factors, factor_loadings()),
               fincore::input_error);
  auto reserved_factors = factor_returns();
  reserved_factors.remove_column<double>("market");
  reserved_factors.load_column<double>("total_returns", {.01, .02, -.01},
                                       hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::perf_attrib_core(strategy_returns(),
                                         fincore::normalize_attribution_positions(
                                             dollar_positions()),
                                         reserved_factors, factor_loadings()),
               fincore::input_error);
  auto symbol_factor = factor_returns();
  symbol_factor.remove_column<double>("market");
  symbol_factor.load_column<double>("symbol", {.01, .02, -.01},
                                   hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::perf_attrib_core(strategy_returns(),
                                         fincore::normalize_attribution_positions(
                                             dollar_positions()),
                                         symbol_factor, factor_loadings()),
               fincore::input_error);
  auto missing_factor_loading = factor_loadings();
  missing_factor_loading.remove_column<double>("market");
  EXPECT_THROW(fincore::perf_attrib_core(strategy_returns(),
                                         fincore::normalize_attribution_positions(
                                             dollar_positions()),
                                         factor_returns(), missing_factor_loading),
               fincore::input_error);
}

TEST(PerfAttrib, RejectsMisalignedOrMalformedCostFrames) {
  const auto returns = strategy_returns();
  const auto wrong_index =
      fincore::make_series({dates()[1], dates()[0], dates()[2]}, "returns",
                           {.001, .002, .003});
  EXPECT_THROW(fincore::cumulative_returns_less_costs(returns, wrong_index),
               fincore::input_error);
  fincore::dataframe wrong_type;
  wrong_type.load_index(std::vector<timestamp>(dates()));
  wrong_type.load_column<int>("returns", {1, 2, 3},
                              hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::cumulative_returns_less_costs(returns, wrong_type),
               fincore::input_error);
  fincore::dataframe malformed;
  malformed.load_index(std::vector<timestamp>(dates()));
  malformed.load_column<double>("returns", {.001, .002},
                                hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::cumulative_returns_less_costs(returns, malformed),
               fincore::input_error);
}

TEST(PerfAttrib, PreservesUnsortedDailyOrderAndPropagatesNegativeInfinity) {
  const std::vector<timestamp> unsorted{dates()[2], dates()[0], dates()[1]};
  auto returns = strategy_returns();
  auto positions = fincore::normalize_attribution_positions(dollar_positions());
  auto factors = factor_returns();
  returns.load_index(std::vector<timestamp>(unsorted));
  positions.load_index(std::vector<timestamp>(unsorted));
  factors.load_index(std::vector<timestamp>(unsorted));
  const auto unsorted_result =
      fincore::perf_attrib_core(returns, positions, factors, factor_loadings());
  EXPECT_EQ(unsorted_result.risk_exposures.get_index(), unsorted);
  EXPECT_EQ(unsorted_result.attribution.get_index(), unsorted);

  auto loadings = factor_loadings();
  loadings.remove_column<double>("market");
  loadings.load_column<double>(
      "market", {-std::numeric_limits<double>::infinity(), .4, .5, .2, .1, .6},
      hmdf::nan_policy::dont_pad_with_nans);
  const auto result = fincore::perf_attrib_core(
      strategy_returns(), fincore::normalize_attribution_positions(dollar_positions()),
      factor_returns(), loadings);
  EXPECT_TRUE(std::isinf(result.risk_exposures.get_column<double>("market")[0]));
  EXPECT_LT(result.risk_exposures.get_column<double>("market")[0], 0.0);
  EXPECT_TRUE(std::isinf(result.attribution.get_column<double>("market")[0]));
  EXPECT_LT(result.attribution.get_column<double>("market")[0], 0.0);
  EXPECT_TRUE(
      std::isinf(result.attribution.get_column<double>("common_returns")[0]));
  EXPECT_LT(result.attribution.get_column<double>("common_returns")[0], 0.0);
  EXPECT_TRUE(
      std::isinf(result.attribution.get_column<double>("specific_returns")[0]));
  EXPECT_GT(result.attribution.get_column<double>("specific_returns")[0], 0.0);
}

} // namespace
