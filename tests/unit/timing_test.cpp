#include <fincore/calendar.hpp>
#include <fincore/error.hpp>
#include <fincore/metrics/timing.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string>
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

std::vector<fincore::timestamp> daily_index(int year, int month, int day,
                                            int count) {
    constexpr std::int64_t nanoseconds_per_day = 86'400'000'000'000LL;
    std::vector<fincore::timestamp> index;
    index.reserve(static_cast<std::size_t>(count));
    const fincore::timestamp start = fincore::utc_timestamp(year, month, day);
    for (int position = 0; position < count; ++position) {
        index.push_back(start + position * nanoseconds_per_day);
    }
    return index;
}

std::vector<double> graded_values(int count) {
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(count));
    for (int position = 0; position < count; ++position) {
        values.push_back(static_cast<double>(position) / 1000.0);
    }
    return values;
}

TEST(Timing, ExtractsHistoricalCrisisRangesInPinnedOrder) {
    const auto returns = fincore::make_series(
        daily_index(2008, 7, 25, 78), "returns", graded_values(78));
    const auto ranges = fincore::extract_interesting_date_ranges(returns);

    ASSERT_EQ(ranges.size(), 3U);
    EXPECT_EQ(ranges[0].name, "Lehman");
    EXPECT_EQ(ranges[1].name, "Sept08");
    EXPECT_EQ(ranges[2].name, "GFC Crash");

    const auto lehman =
        fincore::require_double_column(ranges[0].returns, "returns");
    ASSERT_EQ(lehman.size(), 62U);
    EXPECT_DOUBLE_EQ(lehman.front(), 0.007);
    EXPECT_DOUBLE_EQ(lehman.back(), 0.068);
    EXPECT_EQ(ranges[0].returns.get_index().front(),
              fincore::utc_timestamp(2008, 8, 1));
    EXPECT_EQ(ranges[0].returns.get_index().back(),
              fincore::utc_timestamp(2008, 10, 1));

    const auto september =
        fincore::require_double_column(ranges[1].returns, "returns");
    ASSERT_EQ(september.size(), 31U);
    EXPECT_DOUBLE_EQ(september.front(), 0.038);
    EXPECT_DOUBLE_EQ(september.back(), 0.068);

    const auto gfc =
        fincore::require_double_column(ranges[2].returns, "returns");
    ASSERT_EQ(gfc.size(), 78U);
    EXPECT_DOUBLE_EQ(gfc.front(), 0.0);
    EXPECT_DOUBLE_EQ(gfc.back(), 0.077);
}

TEST(Timing, ExtractsNewNormalWindowAndSkipsEmptyFrames) {
    const auto short_series = fincore::make_series(
        daily_index(2020, 1, 1, 2), "returns", {0.01, 0.02});
    const auto ranges = fincore::extract_interesting_date_ranges(short_series);

    ASSERT_EQ(ranges.size(), 1U);
    EXPECT_EQ(ranges[0].name, "New Normal");
    EXPECT_EQ(fincore::require_double_column(ranges[0].returns, "returns")
                  .size(),
              2U);

    const auto empty = fincore::make_series({}, "returns", {});
    EXPECT_TRUE(fincore::extract_interesting_date_ranges(empty).empty());
}

TEST(Timing, CrisisRangesSupportSelectorsAndRejectNul) {
    const auto strategy = fincore::make_series(
        daily_index(2008, 8, 1, 1), "strategy", {0.01});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};

    const auto named =
        fincore::extract_interesting_date_ranges(strategy, "strategy");
    ASSERT_EQ(named.size(), 2U);
    EXPECT_EQ(named[0].name, "Lehman");
    EXPECT_EQ(named[1].name, "GFC Crash");
    EXPECT_EQ(fincore::require_double_column(named[0].returns, "strategy")
                  .size(),
              1U);

    const auto bounded =
        fincore::extract_interesting_date_ranges(strategy, bounded_selector);
    ASSERT_EQ(bounded.size(), 2U);
    EXPECT_DOUBLE_EQ(
        fincore::require_double_column(bounded[0].returns, "strategy")
            .front(),
        0.01);

    EXPECT_THROW(
        static_cast<void>(
            fincore::extract_interesting_date_ranges(strategy,
                                                      embedded_selector)),
        fincore::input_error);
    EXPECT_THROW(static_cast<void>(
                     fincore::extract_interesting_date_ranges(strategy, "")),
                 fincore::input_error);
}

TEST(Timing, CrisisRangesOwnTheirFramesAfterSourceLifetimeEnds) {
    const auto ranges = [] {
        const auto source = fincore::make_series(
            daily_index(2008, 7, 25, 78), "returns", graded_values(78));
        return fincore::extract_interesting_date_ranges(source);
    }();

    ASSERT_EQ(ranges.size(), 3U);
    EXPECT_EQ(fincore::require_double_column(ranges[2].returns, "returns")
                  .size(),
              78U);
}

}  // namespace
