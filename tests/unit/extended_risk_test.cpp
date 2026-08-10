#include <fincore/error.hpp>
#include <fincore/metrics/risk.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

TEST(ExtendedRisk, MatchesPythonOracleForAlignedDailyReturns) {
  const auto returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, -0.02, 0.03, -0.01});
  const auto benchmark = fincore::make_series({1, 2, 3, 4}, "benchmark",
                                              {0.005, -0.01, 0.02, -0.005});

  EXPECT_NEAR(fincore::downside_risk(returns), 0.1774823934929885, 1e-12);
  EXPECT_NEAR(fincore::tail_ratio(returns), 1.4594594594594592, 1e-12);
  EXPECT_NEAR(fincore::tracking_error(returns, benchmark), 0.14491376746189438,
              1e-12);
  EXPECT_NEAR(fincore::residual_risk(returns, benchmark), 0.03741657386773943,
              1e-12);
}

TEST(ExtendedRisk, AlignsBenchmarkPairsByTimestamp) {
  const auto returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, -0.02, 0.03, -0.01});
  const auto benchmark = fincore::make_series({2, 3, 4, 5}, "benchmark",
                                              {0.005, -0.01, 0.02, -0.005});

  EXPECT_NEAR(fincore::tracking_error(returns, benchmark), 0.6199193495931548,
              1e-12);
  EXPECT_NEAR(fincore::residual_risk(returns, benchmark), 0.2749545416973504,
              1e-12);
}

TEST(ExtendedRisk, FiltersNanValuesForUnpairedRiskMetrics) {
  const auto downside =
      fincore::make_series({1, 2, 3}, "returns", {0.01, nan, -0.02});
  const auto tail =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, nan, -0.02, 0.03});

  EXPECT_NEAR(fincore::downside_risk(downside), 0.2244994432064365, 1e-12);
  EXPECT_NEAR(fincore::tail_ratio(tail), 1.6470588235294115, 1e-12);
}

TEST(ExtendedRisk, FiltersNanPairsAndRejectsInsufficientPairs) {
  const auto returns =
      fincore::make_series({1, 2, 3}, "returns", {0.01, nan, -0.02});
  const auto benchmark =
      fincore::make_series({1, 2, 3}, "benchmark", {0.005, 0.01, -0.01});
  const auto one_pair_benchmark =
      fincore::make_series({1, 2, 3}, "benchmark", {nan, 0.01, nan});

  EXPECT_NEAR(fincore::tracking_error(returns, benchmark),
              0.015 * std::sqrt(126.0), 1e-12);
  EXPECT_NEAR(fincore::residual_risk(returns, benchmark), 0.0, 1e-12);
  EXPECT_TRUE(std::isnan(fincore::tracking_error(returns, one_pair_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::residual_risk(returns, one_pair_benchmark)));
}

TEST(ExtendedRisk, ComputesResidualRiskFromFiniteAlignedPairsOnly) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns",
      {0.01, -0.02, 0.03, std::numeric_limits<double>::infinity()});
  const auto benchmark = fincore::make_series({1, 2, 3, 4}, "benchmark",
                                              {0.005, -0.01, 0.02, 0.1});

  EXPECT_NEAR(fincore::residual_risk(returns, benchmark), 0.04582575694955841,
              1e-12);
}

TEST(ExtendedRisk, ReturnsNanForUndefinedTailAndResidualRisk) {
  const auto zero_tail = fincore::make_series({1, 2}, "returns", {0.0, 0.0});
  const auto returns =
      fincore::make_series({1, 2, 3}, "returns", {0.01, -0.02, 0.03});
  const auto flat_benchmark =
      fincore::make_series({1, 2, 3}, "benchmark", {0.01, 0.01, 0.01});
  const auto empty = fincore::make_series({}, "returns", {});

  EXPECT_TRUE(std::isnan(fincore::tail_ratio(zero_tail)));
  EXPECT_TRUE(std::isnan(fincore::tail_ratio(empty)));
  EXPECT_TRUE(std::isnan(fincore::downside_risk(empty)));
  EXPECT_TRUE(std::isnan(fincore::residual_risk(returns, flat_benchmark)));
}

TEST(ExtendedRisk, ValidatesSelectedColumns) {
  const auto returns = fincore::make_series({1, 2}, "strategy", {0.01, -0.02});
  const auto benchmark = fincore::make_series({1, 2}, "market", {0.005, -0.01});

  EXPECT_NEAR(fincore::downside_risk(returns, 0.0, 252.0, "strategy"),
              0.2244994432064365, 1e-12);
  EXPECT_NEAR(fincore::tail_ratio(returns, "strategy"), 0.0085 / 0.0185, 1e-12);
  EXPECT_NEAR(
      fincore::tracking_error(returns, benchmark, 252.0, "strategy", "market"),
      0.015 * std::sqrt(126.0), 1e-12);
  EXPECT_THROW(fincore::downside_risk(returns, 0.0, 252.0, "missing"),
               fincore::input_error);
  EXPECT_THROW(
      fincore::tracking_error(returns, benchmark, 252.0, "strategy", "missing"),
      fincore::input_error);
}

TEST(ExtendedRisk, MatchesNormalValueAtRiskOracleAndDomainRules) {
  EXPECT_NEAR(fincore::var_cov_var_normal(100000.0, 0.99, 0.001, 0.02),
              4552.695748081678, 1e-9);
  EXPECT_TRUE(std::isinf(fincore::var_cov_var_normal(100.0, 0.0)));
  EXPECT_LT(fincore::var_cov_var_normal(100.0, 0.0), 0.0);
  EXPECT_TRUE(std::isinf(fincore::var_cov_var_normal(100.0, 1.0)));
  EXPECT_GT(fincore::var_cov_var_normal(100.0, 1.0), 0.0);
  EXPECT_TRUE(std::isnan(fincore::var_cov_var_normal(100.0, -0.01)));
  EXPECT_TRUE(std::isnan(fincore::var_cov_var_normal(100.0, 1.01)));
  EXPECT_TRUE(std::isnan(fincore::var_cov_var_normal(100.0, nan)));
  EXPECT_TRUE(std::isnan(fincore::var_cov_var_normal(100.0, 0.95, 0.0, 0.0)));
  EXPECT_TRUE(std::isnan(fincore::var_cov_var_normal(100.0, 0.95, 0.0, nan)));
}

} // namespace
