#include <fincore/error.hpp>
#include <fincore/metrics/timing.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double numerical_tolerance = 1.0e-10;

std::vector<fincore::timestamp> twelve_indices(fincore::timestamp first = 1) {
  std::vector<fincore::timestamp> result;
  result.reserve(12);
  for (fincore::timestamp value = first; value < first + 12; ++value) {
    result.push_back(value);
  }
  return result;
}

std::vector<double> factor_values() {
  return {-0.04, -0.03, -0.02, -0.01, -0.005, 0.005,
          0.01,  0.02,  0.03,  0.04,  0.05,  0.06};
}

std::vector<double> strategy_values() {
  const auto factors = factor_values();
  std::vector<double> result;
  result.reserve(factors.size());
  for (const double factor : factors) {
    result.push_back(0.0007 + 1.1 * factor + 0.35 * factor * factor);
  }
  return result;
}

void expect_timing_near(const fincore::dataframe& returns,
                        const fincore::dataframe& benchmark,
                        double expected_treynor_mazuy,
                        double expected_henriksson_merton,
                        double expected_market_return,
                        double expected_cornell,
                        double risk_free = 0.0,
                        std::string_view returns_column_name = fincore::returns_column,
                        std::string_view benchmark_column_name = fincore::benchmark_column) {
  EXPECT_NEAR(fincore::treynor_mazuy_timing(returns, benchmark, risk_free,
                                            returns_column_name, benchmark_column_name),
              expected_treynor_mazuy, numerical_tolerance);
  EXPECT_NEAR(fincore::henriksson_merton_timing(
                  returns, benchmark, risk_free, returns_column_name, benchmark_column_name),
              expected_henriksson_merton, numerical_tolerance);
  EXPECT_NEAR(fincore::market_timing_return(returns, benchmark, risk_free,
                                             returns_column_name, benchmark_column_name),
              expected_market_return, numerical_tolerance);
  EXPECT_NEAR(fincore::cornell_timing(returns, benchmark, risk_free,
                                       returns_column_name, benchmark_column_name),
              expected_cornell, numerical_tolerance);
}

void expect_nan_timing(const fincore::dataframe& returns,
                       const fincore::dataframe& benchmark,
                       double risk_free = 0.0) {
  EXPECT_TRUE(std::isnan(fincore::treynor_mazuy_timing(returns, benchmark, risk_free)));
  EXPECT_TRUE(std::isnan(fincore::henriksson_merton_timing(returns, benchmark, risk_free)));
  EXPECT_TRUE(std::isnan(fincore::market_timing_return(returns, benchmark, risk_free)));
  EXPECT_TRUE(std::isnan(fincore::cornell_timing(returns, benchmark, risk_free)));
}

TEST(Timing, MatchesTwelvePairPythonOracles) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", strategy_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(), "benchmark", factor_values());

  expect_timing_near(returns, benchmark, 0.34999999999999637,
                     0.0004285816588285324, 0.0003543749999999964,
                     0.039162486406123742);
}

TEST(Timing, UsesExcessMarketValuesForNonzeroRiskFree) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", strategy_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(), "benchmark", factor_values());

  expect_timing_near(returns, benchmark, 0.3499999999999982,
                     0.00042858165882856373, 0.0003483083333333315,
                     0.03883329905388688, 0.001);
}

TEST(Timing, FiltersNanAndInfinityCommonPairsBeforeRegression) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", strategy_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(), "benchmark", factor_values());
  auto nan_return_values = strategy_values();
  nan_return_values[5] = nan;
  const auto nan_returns =
      fincore::make_series(twelve_indices(), "returns", std::move(nan_return_values));
  auto infinity_benchmark_values = factor_values();
  infinity_benchmark_values[5] = std::numeric_limits<double>::infinity();
  const auto infinity_benchmark = fincore::make_series(
      twelve_indices(), "benchmark", std::move(infinity_benchmark_values));

  expect_timing_near(nan_returns, benchmark, 0.35000000000000175,
                     0.00033602950713341572, 0.00038579545454545648,
                     0.040857423359513145);
  expect_timing_near(returns, infinity_benchmark, 0.35000000000000175,
                     0.00033602950713341572, 0.00038579545454545648,
                     0.040857423359513145);
}

TEST(Timing, ReturnsNanForInsufficientOrRankDeficientDesigns) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", strategy_values());
  const auto fewer_than_ten_common =
      fincore::make_series(twelve_indices(4), "benchmark", factor_values());
  const auto constant_benchmark = fincore::make_series(
      twelve_indices(), "benchmark", std::vector<double>(12, 0.01));

  expect_nan_timing(returns, fewer_than_ten_common);
  expect_nan_timing(returns, constant_benchmark);
}

TEST(Timing, ReturnsNanForNonFiniteRiskFree) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", strategy_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(), "benchmark", factor_values());

  expect_nan_timing(returns, benchmark, nan);
  expect_nan_timing(returns, benchmark, std::numeric_limits<double>::infinity());
}

TEST(Timing, SupportsNamedColumnsAndRejectsStructuralErrors) {
  const auto returns =
      fincore::make_series(twelve_indices(), "strategy", strategy_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(), "market", factor_values());

  expect_timing_near(returns, benchmark, 0.34999999999999637,
                     0.0004285816588285324, 0.0003543749999999964,
                     0.039162486406123742, 0.0, "strategy", "market");
  EXPECT_THROW(fincore::treynor_mazuy_timing(returns, benchmark, 0.0, "missing", "market"),
               fincore::input_error);
  EXPECT_THROW(fincore::henriksson_merton_timing(returns, benchmark, 0.0, "strategy",
                                                  "missing"),
               fincore::input_error);
  EXPECT_THROW(fincore::market_timing_return(returns, benchmark, 0.0, "missing", "market"),
               fincore::input_error);
  EXPECT_THROW(fincore::cornell_timing(returns, benchmark, 0.0, "strategy", "missing"),
               fincore::input_error);
}

}  // namespace
