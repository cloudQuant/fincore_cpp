#include <fincore/calendar.hpp>
#include <fincore/metrics.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
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
}

}  // namespace
