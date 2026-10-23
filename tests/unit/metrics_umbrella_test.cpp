#include <fincore/calendar.hpp>
#include <fincore/metrics.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using var_excess_return_signature =
    double (*)(const fincore::dataframe&, double, double, fincore::frequency,
               std::optional<double>, std::string_view);
using trading_value_at_risk_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);
using r_cubed_signature =
    double (*)(const fincore::dataframe&, const fincore::dataframe&,
               std::string_view, std::string_view);
using kappa_three_ratio_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);
using adjusted_sharpe_ratio_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);
using mar_ratio_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);
using conditional_sharpe_ratio_signature =
    double (*)(const fincore::dataframe&, double, double, double,
               std::string_view);

static_assert(std::is_same_v<
              decltype(static_cast<var_excess_return_signature>(
                  &fincore::var_excess_return)),
              var_excess_return_signature>);
static_assert(std::is_same_v<
              decltype(static_cast<trading_value_at_risk_signature>(
                  &fincore::trading_value_at_risk)),
              trading_value_at_risk_signature>);
static_assert(std::is_same_v<
              decltype(static_cast<r_cubed_signature>(&fincore::r_cubed)),
              r_cubed_signature>);
static_assert(std::is_same_v<
              decltype(static_cast<kappa_three_ratio_signature>(
                  &fincore::kappa_three_ratio)),
              kappa_three_ratio_signature>);
static_assert(std::is_same_v<
              decltype(&fincore::adjusted_sharpe_ratio),
              adjusted_sharpe_ratio_signature>);

std::vector<fincore::timestamp> twelve_indices() {
  std::vector<fincore::timestamp> result;
  result.reserve(12);
  for (fincore::timestamp value = 1; value <= 12; ++value) {
    result.push_back(value);
  }
  return result;
}

std::vector<double> benchmark_values() {
  return {-0.04, -0.03, -0.02, -0.01, -0.005, 0.005,
          0.01,  0.02,  0.03,  0.04,  0.05,  0.06};
}

std::vector<double> return_values() {
  const auto benchmark = benchmark_values();
  std::vector<double> result;
  result.reserve(benchmark.size());
  for (const double value : benchmark) {
    result.push_back(0.0007 + 1.1 * value + 0.35 * value * value);
  }
  return result;
}

std::vector<fincore::timestamp> calendar_indices() {
  return {
      fincore::utc_timestamp(2020, 1, 2),
      fincore::utc_timestamp(2020, 1, 3),
      fincore::utc_timestamp(2020, 1, 6),
      fincore::utc_timestamp(2021, 1, 4),
      fincore::utc_timestamp(2021, 1, 5),
      fincore::utc_timestamp(2021, 1, 6),
  };
}

template <typename statistics_type>
std::set<std::string> statistic_keys(const statistics_type& statistics) {
  std::set<std::string> result;
  for (const auto& [key, value] : statistics) {
    static_cast<void>(value);
    result.insert(key);
  }
  return result;
}

TEST(MetricsUmbrella, ExposesOnlyImplementedDataframeNativeMetrics) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", return_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(), "benchmark", benchmark_values());
  const auto adjusted = fincore::adjust_returns(returns, 0.0);

  EXPECT_DOUBLE_EQ(fincore::annualization_factor(fincore::frequency::daily), 252.0);
  EXPECT_TRUE(std::isfinite(fincore::cumulative_return(adjusted)));
  EXPECT_TRUE(std::isfinite(fincore::max_drawdown(returns)));
  EXPECT_TRUE(std::isfinite(fincore::sharpe_ratio(returns)));
  EXPECT_TRUE(std::isfinite(fincore::downside_risk(returns)));
  EXPECT_TRUE(std::isfinite(fincore::trading_value_at_risk(returns)));
  EXPECT_TRUE(std::isfinite(fincore::win_rate(returns)));
  EXPECT_TRUE(std::isfinite(fincore::r_cubed(returns, benchmark)));
  EXPECT_TRUE(std::isfinite(fincore::kappa_three_ratio(returns)));
  EXPECT_TRUE(std::isfinite(fincore::adjusted_sharpe_ratio(returns)));
  EXPECT_TRUE(std::isfinite(fincore::mar_ratio(returns)));
  EXPECT_TRUE(std::isfinite(fincore::conditional_sharpe_ratio(returns, 0.5)));

  const double var_excess_return_var =
      fincore::historical_value_at_risk(returns);
  EXPECT_NEAR(fincore::var_excess_return(returns),
              fincore::annual_return(returns) /
                  std::abs(var_excess_return_var),
              1e-12);

  const double common_sense_tail = fincore::tail_ratio(returns);
  const double common_sense_win_rate = fincore::win_rate(returns);
  EXPECT_NEAR(fincore::common_sense_ratio(returns),
              common_sense_tail * common_sense_win_rate /
                  (1.0 - common_sense_win_rate),
              1e-12);

  const auto common_sense_with_zeros = fincore::make_series(
      std::vector<fincore::timestamp>{1, 2, 3, 4, 5}, "returns",
      std::vector<double>{.01, 0., -.02, .03, 0.});
  const double zero_tail = fincore::tail_ratio(common_sense_with_zeros);
  const double zero_win_rate = fincore::win_rate(common_sense_with_zeros);
  EXPECT_NEAR(fincore::common_sense_ratio(common_sense_with_zeros),
              zero_tail * zero_win_rate / (1.0 - zero_win_rate), 1e-12);

  const auto undefined_common_sense_tail = fincore::make_series(
      std::vector<fincore::timestamp>{1, 2, 3}, "returns",
      std::vector<double>{0., 0., .01});
  EXPECT_TRUE(std::isnan(fincore::tail_ratio(undefined_common_sense_tail)));
  EXPECT_DOUBLE_EQ(fincore::win_rate(undefined_common_sense_tail), 1.0 / 3.0);
  EXPECT_TRUE(
      std::isnan(fincore::common_sense_ratio(undefined_common_sense_tail)));

  std::vector<fincore::timestamp> infinite_tail_index;
  std::vector<double> infinite_tail_values(
      19U, -std::numeric_limits<double>::denorm_min());
  infinite_tail_index.reserve(20U);
  for (fincore::timestamp value = 1; value <= 20; ++value) {
    infinite_tail_index.push_back(value);
  }
  infinite_tail_values.push_back(std::numeric_limits<double>::max());
  const auto infinite_common_sense_tail = fincore::make_series(
      std::move(infinite_tail_index), "returns",
      std::move(infinite_tail_values));
  EXPECT_TRUE(std::isinf(fincore::tail_ratio(infinite_common_sense_tail)));
  EXPECT_DOUBLE_EQ(fincore::win_rate(infinite_common_sense_tail), 0.05);
  const double infinite_common_sense_result =
      fincore::common_sense_ratio(infinite_common_sense_tail);
  EXPECT_TRUE(std::isinf(infinite_common_sense_result));
  EXPECT_GT(infinite_common_sense_result, 0.0);

  const std::vector<fincore::drawdown_period> detailed_drawdowns =
      fincore::get_all_drawdowns_detailed(returns);
  EXPECT_FALSE(detailed_drawdowns.empty());

  const fincore::daily_consecutive_statistics consecutive =
      fincore::daily_consecutive_stats(returns);
  EXPECT_DOUBLE_EQ(consecutive.max_consecutive_up_days, 7.0);
  EXPECT_DOUBLE_EQ(consecutive.max_consecutive_down_days, 5.0);

  const auto [alpha, beta] = fincore::alpha_beta(returns, benchmark, 0.0, 1.0);
  EXPECT_TRUE(std::isfinite(alpha));
  EXPECT_TRUE(std::isfinite(beta));
  EXPECT_TRUE(std::isfinite(fincore::treynor_mazuy_timing(returns, benchmark)));

  const auto calendar_returns = fincore::make_series(
      calendar_indices(), "returns", {.01, -.01, .02, .015, -.005, .01});
  const auto calendar_benchmark = fincore::make_series(
      calendar_indices(), "benchmark", {.005, -.01, .01, .006, -.004, .008});
  const std::vector<fincore::timestamp> expected_year_index{
      fincore::utc_timestamp(2020), fincore::utc_timestamp(2021)};

  EXPECT_EQ(fincore::annual_return_by_year(calendar_returns).get_index(),
            expected_year_index);
  EXPECT_EQ(fincore::annual_alpha(calendar_returns, calendar_benchmark).get_index(),
            expected_year_index);
  EXPECT_EQ(fincore::annual_beta(calendar_returns, calendar_benchmark).get_index(),
            expected_year_index);

  const auto rolling_returns = fincore::make_series(
      std::vector<fincore::timestamp>{1, 2, 3}, "strategy",
      std::vector<double>{.01, -.02, .03});
  const auto rolling_benchmark = fincore::make_series(
      std::vector<fincore::timestamp>{1, 2, 3}, "market",
      std::vector<double>{.005, -.01, .02});
  const std::vector<fincore::timestamp> expected_rolling_endpoint_index{3};

  const auto rolling_sharpe_ratio =
      fincore::roll_sharpe_ratio(rolling_returns, 3U, 0.0,
                                 fincore::frequency::daily, std::nullopt,
                                 "strategy");
  const auto rolling_drawdown =
      fincore::roll_max_drawdown(rolling_returns, 3U, "strategy");
  const auto rolling_volatility =
      fincore::rolling_volatility(rolling_returns, 3U,
                                  fincore::frequency::daily, std::nullopt,
                                  "strategy");
  const auto rolling_sharpe =
      fincore::rolling_sharpe(rolling_returns, 3U, fincore::frequency::daily,
                              std::nullopt, "strategy");
  const auto rolling_alpha = fincore::roll_alpha(
      rolling_returns, rolling_benchmark, 3U, 0.0, fincore::frequency::daily,
      std::nullopt, "strategy", "market");
  const auto rolling_beta = fincore::roll_beta(
      rolling_returns, rolling_benchmark, 3U, 0.0, fincore::frequency::daily,
      std::nullopt, "strategy", "market");
  const auto rolling_up_capture = fincore::roll_up_capture(
      rolling_returns, rolling_benchmark, 3U, fincore::frequency::daily,
      "strategy", "market");
  const auto rolling_down_capture = fincore::roll_down_capture(
      rolling_returns, rolling_benchmark, 3U, fincore::frequency::daily,
      "strategy", "market");
  const auto rolling_up_down_capture = fincore::roll_up_down_capture(
      rolling_returns, rolling_benchmark, 3U, fincore::frequency::daily,
      "strategy", "market");

  EXPECT_EQ(rolling_sharpe_ratio.get_index(), expected_rolling_endpoint_index);
  EXPECT_EQ(rolling_drawdown.get_index(), expected_rolling_endpoint_index);
  EXPECT_EQ(rolling_volatility.get_index(), rolling_returns.get_index());
  EXPECT_EQ(rolling_sharpe.get_index(), rolling_returns.get_index());
  EXPECT_TRUE(rolling_sharpe_ratio.has_column("strategy"));
  EXPECT_TRUE(rolling_drawdown.has_column("strategy"));
  EXPECT_TRUE(rolling_volatility.has_column("strategy"));
  EXPECT_TRUE(rolling_sharpe.has_column("strategy"));

  for (const auto* paired_endpoint : {&rolling_alpha, &rolling_beta,
                                      &rolling_up_capture, &rolling_down_capture,
                                      &rolling_up_down_capture}) {
    EXPECT_EQ(paired_endpoint->get_index(), expected_rolling_endpoint_index);
    EXPECT_TRUE(paired_endpoint->has_column("strategy"));
    EXPECT_EQ(paired_endpoint->get_columns_info<double>().size(), 1U);
  }

  fincore::dataframe rolling_factors;
  rolling_factors.load_index(std::vector<fincore::timestamp>{1, 2, 3});
  rolling_factors.load_column<double>(
      "market", std::vector<double>{.005, -.01, .02},
      hmdf::nan_policy::dont_pad_with_nans);
  rolling_factors.load_column<double>(
      "inverse", std::vector<double>{-.005, .01, -.02},
      hmdf::nan_policy::dont_pad_with_nans);
  const std::array<std::string_view, 2U> factor_names{"market", "inverse"};

  const auto rolling_alpha_beta = fincore::roll_alpha_beta(
      rolling_returns, rolling_benchmark, 3U, 0.0, fincore::frequency::daily,
      std::nullopt, "strategy", "market");
  const auto single_factor_beta = fincore::rolling_beta(
      rolling_returns, rolling_benchmark, 3U, "strategy", "market");
  const auto multi_factor_beta = fincore::rolling_beta(
      rolling_returns, rolling_factors, factor_names, 3U, "strategy");
  const auto rolling_regression = fincore::rolling_regression(
      rolling_returns, rolling_benchmark, 3U, "strategy", "market");

  EXPECT_EQ(rolling_alpha_beta.get_index(), expected_rolling_endpoint_index);
  EXPECT_EQ(rolling_regression.get_index(), expected_rolling_endpoint_index);
  EXPECT_EQ(single_factor_beta.get_index(), rolling_returns.get_index());
  EXPECT_EQ(multi_factor_beta.get_index(), rolling_returns.get_index());
  EXPECT_TRUE(single_factor_beta.has_column("strategy"));

  for (const auto* two_column_result : {&rolling_alpha_beta,
                                        &rolling_regression}) {
    const auto columns = two_column_result->get_columns_info<double>();
    ASSERT_EQ(columns.size(), 2U);
    EXPECT_EQ(std::get<0>(columns[0]), "alpha");
    EXPECT_EQ(std::get<0>(columns[1]), "beta");
  }
  const auto multi_factor_columns = multi_factor_beta.get_columns_info<double>();
  ASSERT_EQ(multi_factor_columns.size(), 2U);
  EXPECT_EQ(std::get<0>(multi_factor_columns[0]), "market");
  EXPECT_EQ(std::get<0>(multi_factor_columns[1]), "inverse");

  const auto unary_statistics = fincore::perf_stats(
      rolling_returns, fincore::frequency::daily, "strategy");
  const auto benchmark_statistics = fincore::perf_stats(
      rolling_returns, rolling_benchmark, fincore::frequency::daily,
      "strategy", "market");
  const std::set<std::string> expected_unary_statistic_keys{
      "Annual return",       "Cumulative returns", "Annual volatility",
      "Sharpe ratio",        "Calmar ratio",       "Stability",
      "Max drawdown",        "Omega ratio",        "Sortino ratio",
      "Skew",                "Kurtosis",           "Tail ratio",
      "Daily value at risk"};
  auto expected_benchmark_statistic_keys = expected_unary_statistic_keys;
  expected_benchmark_statistic_keys.insert("Alpha");
  expected_benchmark_statistic_keys.insert("Beta");

  EXPECT_EQ(statistic_keys(unary_statistics), expected_unary_statistic_keys);
  EXPECT_EQ(statistic_keys(benchmark_statistics),
            expected_benchmark_statistic_keys);
  EXPECT_EQ(unary_statistics.size(), 13U);
  EXPECT_EQ(benchmark_statistics.size(), 15U);
}

TEST(MetricsUmbrella, ConsumesReturnTransformationsThroughPublicHeadersOnly) {
  const std::vector<fincore::timestamp> return_index{
      fincore::utc_timestamp(2020, 12, 28, 15),
      fincore::utc_timestamp(2020, 12, 31, 9),
      fincore::utc_timestamp(2021, 1, 1, 11),
      fincore::utc_timestamp(2021, 1, 3, 18),
      fincore::utc_timestamp(2021, 1, 4, 8)};
  const auto returns = fincore::make_series(
      return_index, "strategy",
      std::vector<double>{.1, .02, .03,
                          std::numeric_limits<double>::quiet_NaN(), -.1});
  const auto values = fincore::make_series(
      std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 12, 28),
                                       fincore::utc_timestamp(2021, 1, 4),
                                       fincore::utc_timestamp(2021, 1, 11)},
      "equity", std::vector<double>{100.0, 110.0, 90.0});

  EXPECT_EQ(fincore::returns_column, "returns");
  EXPECT_DOUBLE_EQ(fincore::daily_annualization, 252.0);
  EXPECT_EQ(fincore::return_aggregation_year_column, "year");
  EXPECT_EQ(fincore::return_aggregation_period_column, "period");

  const auto cumulative =
      fincore::cumulative_returns(returns, 100.0, "strategy");
  EXPECT_EQ(cumulative.get_index(), return_index);
  EXPECT_TRUE(cumulative.has_column(fincore::returns_column.data()));
  const auto& cumulative_values =
      cumulative.get_column<double>(fincore::returns_column.data());
  ASSERT_EQ(cumulative_values.size(), 5U);
  EXPECT_NEAR(cumulative_values[0], 110.0, 1.0e-12);
  EXPECT_NEAR(cumulative_values[1], 112.2, 1.0e-12);
  EXPECT_NEAR(cumulative_values[2], 115.566, 1.0e-12);
  EXPECT_NEAR(cumulative_values[3], 115.566, 1.0e-12);
  EXPECT_NEAR(cumulative_values[4], 104.0094, 1.0e-12);
  EXPECT_NEAR(fincore::cumulative_return(returns, 100.0, "strategy"),
              104.0094, 1.0e-12);

  const auto normalized = fincore::normalize(values, 1.0, "equity");
  EXPECT_EQ(normalized.get_index(), values.get_index());
  const auto& normalized_values = normalized.get_column<double>("equity");
  ASSERT_EQ(normalized_values.size(), 3U);
  EXPECT_DOUBLE_EQ(normalized_values[0], 1.0);
  EXPECT_DOUBLE_EQ(normalized_values[1], 1.1);
  EXPECT_DOUBLE_EQ(normalized_values[2], .9);

  const auto weekly = fincore::aggregate_returns(
      returns, fincore::return_aggregation_period::weekly, "strategy");
  EXPECT_EQ(weekly.get_index(),
            (std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 12, 28),
                                              fincore::utc_timestamp(2021, 1, 4),
                                              fincore::utc_timestamp(2021, 1, 1)}));
  EXPECT_EQ(weekly.shape().second, 3U);
  const auto weekly_columns = weekly.get_columns_info<int, double>();
  ASSERT_EQ(weekly_columns.size(), 3U);
  EXPECT_EQ(std::get<0>(weekly_columns[0]), "year");
  EXPECT_EQ(std::get<0>(weekly_columns[1]), "period");
  EXPECT_EQ(std::get<0>(weekly_columns[2]), "strategy");
  EXPECT_EQ(std::get<2>(weekly_columns[0]), std::type_index(typeid(int)));
  EXPECT_EQ(std::get<2>(weekly_columns[1]), std::type_index(typeid(int)));
  EXPECT_EQ(std::get<2>(weekly_columns[2]), std::type_index(typeid(double)));
  EXPECT_EQ(weekly.get_column<int>("year"),
            (std::vector<int>{2020, 2021, 2021}));
  EXPECT_EQ(weekly.get_column<int>("period"),
            (std::vector<int>{53, 1, 53}));
  const auto& weekly_values = weekly.get_column<double>("strategy");
  ASSERT_EQ(weekly_values.size(), 3U);
  EXPECT_NEAR(weekly_values[0], .122, 1.0e-12);
  EXPECT_NEAR(weekly_values[1], -.1, 1.0e-12);
  EXPECT_NEAR(weekly_values[2], .03, 1.0e-12);
}

TEST(MetricsUmbrella, ConsumesRoundTripsThroughPublicHeadersOnly) {
  constexpr fincore::timestamp jan_2_2024_09 = 1704186000000000000LL;
  constexpr auto hour = std::chrono::hours{1};
  const auto at = [](const std::chrono::nanoseconds offset) {
    return jan_2_2024_09 + offset.count();
  };

  fincore::dataframe transactions;
  transactions.load_index(
      std::vector<fincore::timestamp>{at(std::chrono::nanoseconds{0}),
                                       at(8 * hour), at(10 * hour), at(hour),
                                       at(3 * hour)});
  transactions.load_column<std::string>(
      "symbol", std::vector<std::string>{"A", "A", "A", "B", "B"},
      hmdf::nan_policy::dont_pad_with_nans);
  transactions.load_column<double>(
      "amount", std::vector<double>{2.0, 2.0, -4.0, -2.0, 2.0},
      hmdf::nan_policy::dont_pad_with_nans);
  transactions.load_column<double>(
      "price", std::vector<double>{10.0, 20.0, 25.0, 30.0, 20.0},
      hmdf::nan_policy::dont_pad_with_nans);

  const auto blocks = fincore::groupby_consecutive_transactions(transactions);
  EXPECT_EQ(blocks.get_index(),
            (std::vector<fincore::timestamp>{at(std::chrono::nanoseconds{0}),
                                              at(10 * hour), at(hour),
                                              at(3 * hour)}));
  EXPECT_EQ(blocks.get_column<double>("amount"),
            (std::vector<double>{4.0, -4.0, -2.0, 2.0}));
  EXPECT_EQ(blocks.get_column<std::string>("symbol"),
            (std::vector<std::string>{"A", "A", "B", "B"}));
  EXPECT_EQ(blocks.get_column<double>("price"),
            (std::vector<double>{15.0, 25.0, 30.0, 20.0}));

  const auto round_trips = fincore::extract_round_trips(transactions);
  EXPECT_EQ(round_trips.get_index(), (std::vector<fincore::timestamp>{0, 1}));
  EXPECT_DOUBLE_EQ(round_trips.get_column<double>("pnl").at(0), 40.0);
  EXPECT_EQ(round_trips.get_column<fincore::timestamp>("open_dt").at(0),
            at(std::chrono::nanoseconds{0}));
  EXPECT_EQ(round_trips.get_column<fincore::timestamp>("close_dt").at(0),
            at(10 * hour));
  EXPECT_TRUE(round_trips.get_column<bool>("long").at(0));
  EXPECT_NEAR(round_trips.get_column<double>("rt_returns").at(0), 2.0 / 3.0,
              1.0e-12);
  EXPECT_EQ(round_trips.get_column<std::string>("symbol").at(0), "A");
  EXPECT_EQ(round_trips.get_column<fincore::timestamp>("duration").at(0),
            std::chrono::duration_cast<std::chrono::nanoseconds>(10 * hour)
                .count());

  const auto columns = round_trips.get_columns_info<
      double, std::string, bool, fincore::timestamp>();
  ASSERT_EQ(columns.size(), 7U);
  EXPECT_EQ(std::get<0>(columns[1]), "open_dt");
  EXPECT_EQ(std::get<2>(columns[1]), std::type_index(typeid(fincore::timestamp)));
  EXPECT_EQ(std::get<0>(columns[3]), "long");
  EXPECT_EQ(std::get<2>(columns[3]), std::type_index(typeid(bool)));
  EXPECT_EQ(std::get<0>(columns[5]), "symbol");
  EXPECT_EQ(std::get<2>(columns[5]), std::type_index(typeid(std::string)));
  EXPECT_EQ(std::get<0>(columns[6]), "duration");
  EXPECT_EQ(std::get<2>(columns[6]), std::type_index(typeid(fincore::timestamp)));
}

TEST(MetricsUmbrella, ExposesPerformanceAttributionThroughPublicHeadersOnly) {
  const std::vector<fincore::timestamp> index{
      fincore::utc_timestamp(2024, 1, 2),
      fincore::utc_timestamp(2024, 1, 3)};
  const auto returns =
      fincore::make_series(index, "returns", std::vector<double>{.02, -.01});

  fincore::dataframe positions;
  positions.load_index(std::vector<fincore::timestamp>(index));
  positions.load_column<double>("A", {.6, .5},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("B", {.4, .5},
                                hmdf::nan_policy::dont_pad_with_nans);

  fincore::dataframe factor_returns;
  factor_returns.load_index(std::vector<fincore::timestamp>(index));
  factor_returns.load_column<double>("market", {.01, .02},
                                     hmdf::nan_policy::dont_pad_with_nans);
  factor_returns.load_column<double>("value", {.03, -.01},
                                     hmdf::nan_policy::dont_pad_with_nans);

  fincore::dataframe factor_loadings;
  factor_loadings.load_index({index[0], index[0], index[1], index[1]});
  factor_loadings.load_column<std::string>("symbol", {"A", "B", "A", "B"},
                                           hmdf::nan_policy::dont_pad_with_nans);
  factor_loadings.load_column<double>("market", {.2, .4, .1, .3},
                                      hmdf::nan_policy::dont_pad_with_nans);
  factor_loadings.load_column<double>("value", {.5, -.5, .2, .4},
                                      hmdf::nan_policy::dont_pad_with_nans);

  const auto attribution = fincore::perf_attrib(
      returns, positions, factor_returns, factor_loadings, false);
  EXPECT_NEAR(attribution.risk_exposures.get_column<double>("market")[0], .28,
              1.0e-12);
  EXPECT_NEAR(attribution.attribution.get_column<double>("common_returns")[0],
              .0058, 1.0e-12);

  const auto cumulative = fincore::cumulative_returns_less_costs(returns);
  EXPECT_NEAR(cumulative.get_column<double>("returns")[1], .0098, 1.0e-12);
}

TEST(MetricsUmbrella, ExposesNativePositionMetrics) {
  fincore::dataframe positions;
  const auto expected_index = calendar_indices();
  positions.load_index(calendar_indices());
  positions.load_column<double>(
      "equity", std::vector<double>{10.0, -5.0,
                                    std::numeric_limits<double>::quiet_NaN(),
                                    0.0, 5.0, -2.0},
      hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<int>("metadata", std::vector<int>{1, 2, 3, 4, 5, 6},
                             hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("bond",
                                std::vector<double>{-5.0, -2.0, 3.0,
                                                    0.0, -5.0, 3.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("cash",
                                std::vector<double>{95.0, 100.0, 0.0,
                                                    100.0, 100.0, 99.0},
                                hmdf::nan_policy::dont_pad_with_nans);

  const auto allocations = fincore::get_percent_alloc(positions);
  const auto long_short = fincore::get_long_short_pos(positions);
  const auto leverage = fincore::gross_lev(positions);
  const auto concentration =
      fincore::get_max_median_position_concentration(positions);

  for (const auto* result : {&allocations, &long_short, &leverage,
                             &concentration}) {
    EXPECT_EQ(result->get_index(), expected_index);
  }

  const auto allocation_columns = allocations.get_columns_info<double>();
  ASSERT_EQ(allocation_columns.size(), 3U);
  EXPECT_EQ(std::get<0>(allocation_columns[0]), "equity");
  EXPECT_EQ(std::get<0>(allocation_columns[1]), "bond");
  EXPECT_EQ(std::get<0>(allocation_columns[2]), "cash");
  EXPECT_FALSE(allocations.has_column("metadata"));
  EXPECT_DOUBLE_EQ(allocations.get_column<double>("equity")[0], 0.1);
  EXPECT_TRUE(std::isnan(allocations.get_column<double>("equity")[2]));
  EXPECT_DOUBLE_EQ(allocations.get_column<double>("bond")[2], 1.0);

  const auto long_short_columns = long_short.get_columns_info<double>();
  ASSERT_EQ(long_short_columns.size(), 2U);
  EXPECT_EQ(std::get<0>(long_short_columns[0]), "long");
  EXPECT_EQ(std::get<0>(long_short_columns[1]), "short");
  EXPECT_DOUBLE_EQ(long_short.get_column<double>("long")[0], 10.0);
  EXPECT_DOUBLE_EQ(long_short.get_column<double>("short")[1], 7.0);

  const auto leverage_columns = leverage.get_columns_info<double>();
  ASSERT_EQ(leverage_columns.size(), 1U);
  EXPECT_EQ(std::get<0>(leverage_columns[0]), "gross_lev");
  EXPECT_DOUBLE_EQ(leverage.get_column<double>("gross_lev")[0], 0.15);
  EXPECT_DOUBLE_EQ(leverage.get_column<double>("gross_lev")[2], 1.0);

  const auto concentration_columns = concentration.get_columns_info<double>();
  ASSERT_EQ(concentration_columns.size(), 4U);
  EXPECT_EQ(std::get<0>(concentration_columns[0]), "max_long");
  EXPECT_EQ(std::get<0>(concentration_columns[1]), "median_long");
  EXPECT_EQ(std::get<0>(concentration_columns[2]), "median_short");
  EXPECT_EQ(std::get<0>(concentration_columns[3]), "max_short");
  EXPECT_DOUBLE_EQ(concentration.get_column<double>("max_long")[0], 0.1);
  EXPECT_DOUBLE_EQ(concentration.get_column<double>("median_short")[1],
                   -0.03763440860215054);
  EXPECT_TRUE(
      std::isnan(concentration.get_column<double>("max_short")[2]));
}

TEST(MetricsUmbrella, ExposesNativeTransactionMetrics) {
  const auto jan_2 = fincore::utc_timestamp(2024, 1, 2);
  const auto jan_3 = fincore::utc_timestamp(2024, 1, 3);
  const auto jan_4 = fincore::utc_timestamp(2024, 1, 4);
  const auto transaction_only_jan_5 = fincore::utc_timestamp(2024, 1, 5);

  fincore::dataframe transactions;
  transactions.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2024, 1, 5, 11),
      fincore::utc_timestamp(2024, 1, 2, 15, 45),
      fincore::utc_timestamp(2024, 1, 3, 10),
      fincore::utc_timestamp(2024, 1, 2, 9, 30),
  });
  transactions.load_column<double>(
      "amount",
      std::vector<double>{3.0, -4.0, std::numeric_limits<double>::quiet_NaN(),
                          10.0},
      hmdf::nan_policy::dont_pad_with_nans);
  transactions.load_column<double>(
      "price",
      std::vector<double>{std::numeric_limits<double>::infinity(), 10.0, 7.0,
                          5.0},
      hmdf::nan_policy::dont_pad_with_nans);

  fincore::dataframe positions;
  positions.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2024, 1, 2, 17),
      fincore::utc_timestamp(2024, 1, 3, 11),
      fincore::utc_timestamp(2024, 1, 4, 7),
  });
  positions.load_column<double>("asset", std::vector<double>{100.0, -50.0, 20.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<int>("metadata", std::vector<int>{1, 2, 3},
                             hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("cash", std::vector<double>{900.0, 1050.0, 980.0},
                                hmdf::nan_policy::dont_pad_with_nans);

  const auto daily = fincore::get_txn_vol(transactions);
  const auto actual_gross_book = fincore::get_turnover(
      positions, transactions, fincore::turnover_denominator::actual_gross_book);
  const auto portfolio_value = fincore::get_turnover(
      positions, transactions, fincore::turnover_denominator::portfolio_value);

  const std::vector<fincore::timestamp> expected_daily_index{
      jan_2, jan_3, transaction_only_jan_5};
  EXPECT_EQ(daily.get_index(), expected_daily_index);
  const auto daily_columns = daily.get_columns_info<double>();
  ASSERT_EQ(daily_columns.size(), 2U);
  EXPECT_EQ(std::get<0>(daily_columns[0]), "txn_volume");
  EXPECT_EQ(std::get<0>(daily_columns[1]), "txn_shares");
  EXPECT_DOUBLE_EQ(daily.get_column<double>("txn_volume")[0], 90.0);
  EXPECT_DOUBLE_EQ(daily.get_column<double>("txn_shares")[0], 14.0);
  EXPECT_DOUBLE_EQ(daily.get_column<double>("txn_volume")[1], 0.0);
  EXPECT_EQ(daily.get_column<double>("txn_volume")[2],
            std::numeric_limits<double>::infinity());

  const std::vector<fincore::timestamp> expected_turnover_index{
      jan_2, jan_3, jan_4, transaction_only_jan_5};
  for (const auto* turnover : {&actual_gross_book, &portfolio_value}) {
    EXPECT_EQ(turnover->get_index(), expected_turnover_index);
    const auto columns = turnover->get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_EQ(std::get<0>(columns[0]), "turnover");
    EXPECT_EQ(turnover->get_index().back(), transaction_only_jan_5);
    EXPECT_DOUBLE_EQ(turnover->get_column<double>("turnover").back(), 0.0);
  }
  EXPECT_DOUBLE_EQ(actual_gross_book.get_column<double>("turnover")[0], 1.8);
  EXPECT_DOUBLE_EQ(portfolio_value.get_column<double>("turnover")[0], 0.09);
}

TEST(MetricsUmbrella, ExposesSlippageAdjustedReturns) {
  const auto jan_2 = fincore::utc_timestamp(2024, 1, 2);
  const auto jan_3 = fincore::utc_timestamp(2024, 1, 3);
  const auto jan_4 = fincore::utc_timestamp(2024, 1, 4);
  const auto jan_5 = fincore::utc_timestamp(2024, 1, 5);
  const auto jan_6 = fincore::utc_timestamp(2024, 1, 6);

  const auto returns = fincore::make_series(
      std::vector<fincore::timestamp>{
          fincore::utc_timestamp(2024, 1, 2, 14),
          fincore::utc_timestamp(2024, 1, 3, 14),
          fincore::utc_timestamp(2024, 1, 5, 14),
          fincore::utc_timestamp(2024, 1, 6, 14),
      },
      "returns",
      std::vector<double>{.1, .2, std::numeric_limits<double>::quiet_NaN(),
                          .3});

  fincore::dataframe positions;
  positions.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2024, 1, 2, 16),
      fincore::utc_timestamp(2024, 1, 4, 16),
      fincore::utc_timestamp(2024, 1, 5, 16),
  });
  positions.load_column<double>("asset",
                                std::vector<double>{100.0, 200.0, 300.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<double>("cash",
                                std::vector<double>{900.0, 800.0, 700.0},
                                hmdf::nan_policy::dont_pad_with_nans);
  positions.load_column<int>("metadata", std::vector<int>{1, 2, 3},
                             hmdf::nan_policy::dont_pad_with_nans);

  fincore::dataframe transactions;
  transactions.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2024, 1, 5, 10),
      fincore::utc_timestamp(2024, 1, 2, 10),
      fincore::utc_timestamp(2024, 1, 4, 10),
      fincore::utc_timestamp(2024, 1, 3, 10),
  });
  transactions.load_column<double>("amount",
                                   std::vector<double>{10.0, 10.0, 10.0, 10.0},
                                   hmdf::nan_policy::dont_pad_with_nans);
  transactions.load_column<double>("price",
                                   std::vector<double>{10.0, 10.0, 10.0, 10.0},
                                   hmdf::nan_policy::dont_pad_with_nans);

  const auto adjusted = fincore::adjust_returns_for_slippage(
      returns, positions, transactions, 10.0);

  EXPECT_EQ(adjusted.get_index(), (std::vector<fincore::timestamp>{
                                      jan_2, jan_3, jan_4, jan_5, jan_6}));
  ASSERT_EQ(adjusted.shape().second, 1U);
  const auto columns = adjusted.get_columns_info<double>();
  ASSERT_EQ(columns.size(), 1U);
  EXPECT_EQ(std::get<0>(columns[0]), "returns");
  const auto& values = adjusted.get_column<double>("returns");
  ASSERT_EQ(values.size(), 5U);
  EXPECT_DOUBLE_EQ(values[0], .0999);
  EXPECT_DOUBLE_EQ(values[1], 0.0);
  EXPECT_DOUBLE_EQ(values[2], 0.0);
  EXPECT_DOUBLE_EQ(values[3], 0.0);
  EXPECT_DOUBLE_EQ(values[4], 0.0);
}

TEST(MetricsUmbrella, ExposesQuadraticVolumeShareSlippagePenalty) {
  const auto jan_1 = fincore::utc_timestamp(2024, 1, 1);
  const auto jan_2 = fincore::utc_timestamp(2024, 1, 2);
  const auto jan_3 = fincore::utc_timestamp(2024, 1, 3);
  const auto returns = fincore::make_series(
      std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 1, 9),
                                       fincore::utc_timestamp(2024, 1, 2, 9),
                                       fincore::utc_timestamp(2024, 1, 3, 9)},
      "returns", std::vector<double>{1.0, 1.0, 1.0});

  fincore::dataframe transactions;
  transactions.load_index(
      std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 1, 15),
                                       fincore::utc_timestamp(2024, 1, 2, 15),
                                       fincore::utc_timestamp(2024, 1, 3, 15)});
  transactions.load_column<double>(
      "amount", std::vector<double>{100000.0, 100000.0, 100000.0},
      hmdf::nan_policy::dont_pad_with_nans);
  transactions.load_column<double>("price", std::vector<double>{1.0, 1.0, 1.0},
                                   hmdf::nan_policy::dont_pad_with_nans);
  transactions.load_column<double>(
      "volume", std::vector<double>{1000000.0, 2000000.0, 3000000.0},
      hmdf::nan_policy::dont_pad_with_nans);

  const auto adjusted = fincore::apply_slippage_penalty(
      returns, transactions, 1000000.0, 1000000.0, 1.0);

  EXPECT_EQ(adjusted.get_index(),
            (std::vector<fincore::timestamp>{jan_1, jan_2, jan_3}));
  ASSERT_EQ(adjusted.shape().second, 1U);
  const auto columns = adjusted.get_columns_info<double>();
  ASSERT_EQ(columns.size(), 1U);
  EXPECT_EQ(std::get<0>(columns[0]), "returns");
  const auto& values = adjusted.get_column<double>("returns");
  ASSERT_EQ(values.size(), 3U);
  EXPECT_DOUBLE_EQ(values[0], .9995);
  EXPECT_DOUBLE_EQ(values[1], .9999375);
  EXPECT_DOUBLE_EQ(values[2], .9999861111111111);
}

}  // namespace
