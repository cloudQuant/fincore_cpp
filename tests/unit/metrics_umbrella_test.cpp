#include <fincore/calendar.hpp>
#include <fincore/metrics.hpp>

#include <gtest/gtest.h>

#include <cmath>
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
}

}  // namespace
