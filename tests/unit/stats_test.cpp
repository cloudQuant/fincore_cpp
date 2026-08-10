#include <fincore/error.hpp>
#include <fincore/metrics/stats.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

TEST(Stats, MatchesPythonOracleForAlignedDailyReturns) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4, 5}, "returns", {0.01, -0.02, 0.03, -0.01, 0.02});
  const auto benchmark = fincore::make_series(
      {1, 2, 3, 4, 5}, "benchmark", {0.005, -0.01, 0.02, -0.005, 0.015});

  EXPECT_NEAR(fincore::skewness(returns), -0.15798755143759596, 1e-12);
  EXPECT_NEAR(fincore::kurtosis(returns), -1.4908058409951324, 1e-12);
  EXPECT_NEAR(fincore::serial_correlation(returns), -0.8525371202334078,
              1e-12);
  EXPECT_DOUBLE_EQ(fincore::win_rate(returns), 0.6);
  EXPECT_DOUBLE_EQ(fincore::loss_rate(returns), 0.4);
  EXPECT_NEAR(fincore::safe_correlation(returns, benchmark),
              0.9930437840630139, 1e-12);
  EXPECT_DOUBLE_EQ(fincore::relative_win_rate(returns, benchmark), 0.6);
  EXPECT_NEAR(fincore::capm_r_squared(returns, benchmark),
              0.9861359570661897, 1e-12);
  EXPECT_NEAR(fincore::tracking_difference(returns, benchmark),
              0.004563664875000262, 1e-12);
}

TEST(Stats, AlignsPairedMetricsByTimestamp) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4, 5}, "returns", {0.01, -0.02, 0.03, -0.01, 0.02});
  const auto benchmark = fincore::make_series(
      {2, 3, 4, 5, 6}, "benchmark", {0.005, -0.01, 0.02, -0.005, 0.015});

  EXPECT_NEAR(fincore::safe_correlation(returns, benchmark),
              -0.7938841860374447, 1e-12);
  EXPECT_DOUBLE_EQ(fincore::relative_win_rate(returns, benchmark), 0.5);
  EXPECT_NEAR(fincore::capm_r_squared(returns, benchmark),
              0.6302521008403359, 1e-12);
  EXPECT_NEAR(fincore::tracking_difference(returns, benchmark),
              0.009517365000000222, 1e-12);
}

TEST(Stats, FiltersNonFiniteObservationsAndPairs) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4, 5}, "returns",
      {0.01, nan, -0.02, 0.03, std::numeric_limits<double>::infinity()});
  const auto benchmark = fincore::make_series(
      {1, 2, 3, 4}, "benchmark", {0.005, 0.1, -0.01, -0.01});
  const auto finite_pair_returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, nan, -0.02, 0.03});
  const auto finite_pair_benchmark =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {0.005, 0.01, nan, -0.01});

  EXPECT_NEAR(fincore::skewness(returns), -0.23906314692954447, 1e-12);
  EXPECT_TRUE(std::isnan(fincore::kurtosis(returns)));
  EXPECT_NEAR(fincore::serial_correlation(returns), -1.0, 1e-12);
  EXPECT_NEAR(fincore::win_rate(returns), 2.0 / 3.0, 1e-12);
  EXPECT_NEAR(fincore::loss_rate(returns), 1.0 / 3.0, 1e-12);

  EXPECT_NEAR(fincore::safe_correlation(returns, benchmark),
              0.11470786693528093, 1e-12);
  EXPECT_NEAR(fincore::relative_win_rate(returns, benchmark), 2.0 / 3.0, 1e-12);
  EXPECT_NEAR(fincore::capm_r_squared(returns, benchmark),
              0.013157894736842115, 1e-12);
  EXPECT_NEAR(fincore::tracking_difference(returns, benchmark),
              0.03449350000000029, 1e-12);

  EXPECT_NEAR(fincore::safe_correlation(finite_pair_returns, finite_pair_benchmark),
              -1.0, 1e-12);
  EXPECT_DOUBLE_EQ(
      fincore::relative_win_rate(finite_pair_returns, finite_pair_benchmark), 1.0);
  EXPECT_NEAR(fincore::capm_r_squared(finite_pair_returns, finite_pair_benchmark),
              1.0, 1e-12);
  EXPECT_NEAR(fincore::tracking_difference(finite_pair_returns, finite_pair_benchmark),
              0.04535000000000011, 1e-12);
}

TEST(Stats, ReturnsNanAtInvalidNumericalBoundaries) {
  const auto two_values = fincore::make_series({1, 2}, "returns", {0.01, -0.02});
  const auto three_values =
      fincore::make_series({1, 2, 3}, "returns", {0.01, -0.02, 0.03});
  const auto constant =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, 0.01, 0.01, 0.01});
  const auto zeroes = fincore::make_series({1, 2, 3}, "returns", {0.0, 0.0, 0.0});
  const auto no_finite = fincore::make_series(
      {1, 2}, "returns", {nan, std::numeric_limits<double>::infinity()});
  const auto benchmark =
      fincore::make_series({1, 2, 3}, "benchmark", {0.01, 0.02, 0.03});
  const auto no_common_benchmark =
      fincore::make_series({4, 5, 6}, "benchmark", {0.01, 0.02, 0.03});
  const auto one_pair_benchmark =
      fincore::make_series({1, 2}, "benchmark", {0.005, nan});
  const auto tied_benchmark =
      fincore::make_series({1, 2}, "benchmark", {0.01, 0.01});
  const auto tiny_variation =
      fincore::make_series({1, 2}, "returns", {0.0, 5.0e-16});
  const auto huge_returns = fincore::make_series(
      {1, 2}, "returns", {std::numeric_limits<double>::max(),
                             std::numeric_limits<double>::max()});
  const auto zero_benchmark =
      fincore::make_series({1, 2}, "benchmark", {0.0, 0.0});

  EXPECT_TRUE(std::isnan(fincore::skewness(two_values)));
  EXPECT_TRUE(std::isnan(fincore::kurtosis(three_values)));
  EXPECT_TRUE(std::isnan(fincore::skewness(constant)));
  EXPECT_TRUE(std::isnan(fincore::kurtosis(constant)));
  EXPECT_TRUE(std::isnan(fincore::serial_correlation(three_values, 0)));
  EXPECT_TRUE(std::isnan(fincore::serial_correlation(two_values)));
  EXPECT_TRUE(std::isnan(fincore::serial_correlation(constant)));
  EXPECT_DOUBLE_EQ(fincore::win_rate(zeroes), 0.0);
  EXPECT_DOUBLE_EQ(fincore::loss_rate(zeroes), 0.0);
  EXPECT_TRUE(std::isnan(fincore::win_rate(no_finite)));
  EXPECT_TRUE(std::isnan(fincore::loss_rate(no_finite)));
  EXPECT_TRUE(std::isnan(fincore::safe_correlation(two_values, one_pair_benchmark)));
  EXPECT_DOUBLE_EQ(fincore::relative_win_rate(two_values, one_pair_benchmark), 1.0);
  EXPECT_DOUBLE_EQ(fincore::relative_win_rate(two_values, tied_benchmark), 0.0);
  EXPECT_TRUE(std::isnan(fincore::safe_correlation(constant, benchmark)));
  EXPECT_TRUE(std::isnan(
      fincore::safe_correlation(tiny_variation, two_values, "returns", "returns")));
  EXPECT_TRUE(std::isnan(fincore::safe_correlation(two_values, no_common_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::relative_win_rate(two_values, no_common_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::capm_r_squared(constant, benchmark)));
  EXPECT_TRUE(std::isnan(fincore::capm_r_squared(two_values, no_common_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::tracking_difference(two_values, no_common_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::tracking_difference(huge_returns, zero_benchmark)));
}

TEST(Stats, SupportsNamedColumnsAndRejectsStructuralErrors) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4, 5}, "strategy", {0.01, -0.02, 0.03, -0.01, 0.02});
  const auto benchmark = fincore::make_series(
      {1, 2, 3, 4, 5}, "market", {0.005, -0.01, 0.02, -0.005, 0.015});

  EXPECT_NEAR(fincore::skewness(returns, "strategy"), -0.15798755143759596,
              1e-12);
  EXPECT_NEAR(fincore::safe_correlation(returns, benchmark, "strategy", "market"),
              0.9930437840630139, 1e-12);
  EXPECT_THROW(fincore::win_rate(returns, "missing"), fincore::input_error);
  EXPECT_THROW(
      fincore::tracking_difference(returns, benchmark, "strategy", "missing"),
      fincore::input_error);
}

}  // namespace
