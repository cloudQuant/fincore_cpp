#include <fincore/error.hpp>
#include <fincore/metrics/ratios.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

TEST(ExtendedRatios, MatchesPythonOracleForAlignedDailyReturns) {
  const auto returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, -0.02, 0.03, -0.01});
  const auto benchmark = fincore::make_series({1, 2, 3, 4}, "benchmark",
                                              {0.005, -0.01, 0.02, -0.005});

  EXPECT_NEAR(fincore::excess_sharpe(returns, benchmark),
              -4.750735894131342e-17, 1e-12);
  EXPECT_NEAR(fincore::information_ratio(returns, benchmark),
              -7.541559432251765e-16, 1e-12);
  EXPECT_NEAR(fincore::omega_ratio(returns), 1.3333333333333335, 1e-12);
  EXPECT_NEAR(fincore::stability_of_timeseries(returns), 0.08154934149362057,
              1e-12);
  EXPECT_NEAR(fincore::capture(returns, benchmark), 0.9361616468408038, 1e-12);
  EXPECT_NEAR(fincore::up_capture(returns, benchmark), 6.637108874074204,
              1e-12);
  EXPECT_NEAR(fincore::down_capture(returns, benchmark), 1.150300078888413,
              1e-12);
  EXPECT_NEAR(fincore::up_down_capture(returns, benchmark), 5.769893435535484,
              1e-12);
}

TEST(ExtendedRatios, AlignsBenchmarkPairsByTimestamp) {
  const auto returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, -0.02, 0.03, -0.01});
  const auto benchmark = fincore::make_series({2, 3, 4, 5}, "benchmark",
                                              {0.005, -0.01, 0.02, -0.005});

  EXPECT_NEAR(fincore::excess_sharpe(returns, benchmark), -0.12803687993289598,
              1e-12);
  EXPECT_NEAR(fincore::information_ratio(returns, benchmark),
              -2.0325224576824743, 1e-12);
  EXPECT_NEAR(fincore::capture(returns, benchmark), -0.023129239097150745,
              1e-12);
  EXPECT_NEAR(fincore::up_capture(returns, benchmark), -0.045007856456753544,
              1e-12);
  EXPECT_NEAR(fincore::down_capture(returns, benchmark), -1864.997304577222,
              1e-9);
  EXPECT_NEAR(fincore::up_down_capture(returns, benchmark),
              2.4132933782956014e-05, 1e-12);
}

TEST(ExtendedRatios, FiltersNonFinitePairsBeforePairedMetrics) {
  const auto returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, nan, -0.02, 0.03});
  const auto benchmark =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {0.005, 0.01, nan, -0.01});
  const auto infinity_returns = fincore::make_series(
      {1, 2, 3, 4}, "returns", {0.01, std::numeric_limits<double>::infinity(), -0.02,
                                  0.03});
  const auto infinity_benchmark =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {0.005, 0.01, -0.01, -0.01});

  EXPECT_NEAR(fincore::excess_sharpe(returns, benchmark), 0.9091372900969896,
              1e-12);
  EXPECT_NEAR(fincore::information_ratio(returns, benchmark),
              14.432107063270918, 1e-12);
  EXPECT_NEAR(fincore::excess_sharpe(infinity_returns, infinity_benchmark),
              0.45469886172314167, 1e-12);
  EXPECT_NEAR(fincore::information_ratio(infinity_returns, infinity_benchmark),
              7.218120657261474, 1e-12);
  EXPECT_NEAR(fincore::capture(infinity_returns, infinity_benchmark),
              -5.649133148105316, 1e-12);
  EXPECT_NEAR(fincore::up_capture(infinity_returns, infinity_benchmark),
              4.483826647836499, 1e-12);
  EXPECT_NEAR(fincore::down_capture(infinity_returns, infinity_benchmark),
              -2.4449308420844624, 1e-12);
  EXPECT_NEAR(fincore::up_down_capture(infinity_returns, infinity_benchmark),
              -1.833927803051372, 1e-12);
}

TEST(ExtendedRatios, ReturnsNanForUndefinedPairedMetrics) {
  const auto returns = fincore::make_series({1, 2}, "returns", {0.01, 0.0});
  const auto flat_benchmark =
      fincore::make_series({1, 2}, "benchmark", {0.01, 0.0});
  const auto zero_benchmark =
      fincore::make_series({1, 2}, "benchmark", {0.0, 0.0});
  const auto one_pair_returns = fincore::make_series({1, 2}, "returns", {0.01, nan});
  const auto one_pair_benchmark =
      fincore::make_series({1, 2}, "benchmark", {0.005, 0.01});
  const auto only_down =
      fincore::make_series({1, 2}, "benchmark", {0.0, -0.01});
  const auto only_up = fincore::make_series({1, 2}, "benchmark", {0.0, 0.01});
  const auto zero_down =
      fincore::make_series({1, 2}, "benchmark", {0.01, -0.01});

  EXPECT_TRUE(std::isnan(fincore::excess_sharpe(returns, flat_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::information_ratio(returns, flat_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::capture(returns, zero_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::excess_sharpe(one_pair_returns, one_pair_benchmark)));
  EXPECT_TRUE(
      std::isnan(fincore::information_ratio(one_pair_returns, one_pair_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::up_capture(returns, only_down)));
  EXPECT_TRUE(std::isnan(fincore::down_capture(returns, only_up)));
  EXPECT_TRUE(std::isnan(fincore::up_down_capture(returns, only_down)));
  EXPECT_TRUE(std::isnan(fincore::up_down_capture(returns, zero_down)));
}

TEST(ExtendedRatios, ValidatesOmegaDomainsAndFiniteValues) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns",
      {0.02, nan, -0.01, std::numeric_limits<double>::infinity()});
  const auto gains = fincore::make_series({1, 2}, "returns", {0.01, 0.02});
  const auto allowed_threshold =
      fincore::make_series({1, 2}, "returns", {-1.5, 0.0});

  EXPECT_NEAR(fincore::omega_ratio(returns), 2.0, 1e-12);
  EXPECT_TRUE(std::isnan(fincore::omega_ratio(gains)));
  EXPECT_TRUE(std::isnan(fincore::omega_ratio(gains, 0.0, -1.0)));
  EXPECT_NEAR(fincore::omega_ratio(allowed_threshold, 0.0, -1.0, 1.0), 2.0,
              1e-12);
  EXPECT_TRUE(std::isnan(fincore::omega_ratio(returns, nan)));
  EXPECT_TRUE(std::isnan(fincore::omega_ratio(returns, 0.0, nan)));
  EXPECT_TRUE(std::isnan(fincore::omega_ratio(returns, 0.0, 0.0, 0.0)));
  EXPECT_TRUE(std::isnan(fincore::omega_ratio(
      returns, 0.0, 0.0, std::numeric_limits<double>::infinity())));
}

TEST(ExtendedRatios, ValidatesStabilityDomains) {
  const auto finite_and_non_finite = fincore::make_series(
      {1, 2, 3, 4}, "returns",
      {0.01, nan, 0.02, std::numeric_limits<double>::infinity()});
  const auto one_finite = fincore::make_series({1, 2}, "returns", {0.01, nan});
  const auto zero_response = fincore::make_series({1, 2}, "returns", {0.0, 0.0});
  const auto invalid_log = fincore::make_series({1, 2}, "returns", {-1.0, 0.01});

  EXPECT_NEAR(fincore::stability_of_timeseries(finite_and_non_finite), 1.0, 1e-12);
  EXPECT_TRUE(std::isnan(fincore::stability_of_timeseries(one_finite)));
  EXPECT_TRUE(std::isnan(fincore::stability_of_timeseries(zero_response)));
  EXPECT_TRUE(std::isnan(fincore::stability_of_timeseries(invalid_log)));
}

TEST(ExtendedRatios, SupportsNamedColumnsAndRejectsStructuralErrors) {
  const auto returns = fincore::make_series({1, 2, 3, 4}, "strategy",
                                             {0.01, -0.02, 0.03, -0.01});
  const auto benchmark = fincore::make_series({1, 2, 3, 4}, "market",
                                               {0.005, -0.01, 0.02, -0.005});

  EXPECT_NEAR(fincore::excess_sharpe(returns, benchmark, "strategy", "market"),
              -4.750735894131342e-17, 1e-12);
  EXPECT_NEAR(fincore::capture(returns, benchmark, 252.0, "strategy", "market"),
              0.9361616468408038, 1e-12);
  EXPECT_THROW(fincore::omega_ratio(returns, 0.0, 0.0, 252.0, "missing"),
               fincore::input_error);
  EXPECT_THROW(
      fincore::information_ratio(returns, benchmark, 252.0, "strategy", "missing"),
      fincore::input_error);
}

}  // namespace
