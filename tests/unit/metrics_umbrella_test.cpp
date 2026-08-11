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
#include <vector>

namespace {

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
  EXPECT_TRUE(std::isfinite(fincore::win_rate(returns)));

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

}  // namespace
