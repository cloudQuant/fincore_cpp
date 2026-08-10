#include <fincore/error.hpp>
#include <fincore/metrics/alpha_beta.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

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

void expect_pair_near(const std::pair<double, double>& actual,
                      double expected_alpha,
                      double expected_beta) {
  EXPECT_NEAR(actual.first, expected_alpha, 1e-12);
  EXPECT_NEAR(actual.second, expected_beta, 1e-12);
}

void expect_nan_pair(const std::pair<double, double>& value) {
  EXPECT_TRUE(std::isnan(value.first));
  EXPECT_TRUE(std::isnan(value.second));
}

TEST(ExtendedAlphaBeta, MatchesOverallUpAndDownPythonOracles) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", strategy_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(), "benchmark", factor_values());

  expect_pair_near(fincore::alpha_beta(returns, benchmark, 0.0, 1.0),
                   0.00098825261780111084, 1.1072133507853401);
  expect_pair_near(fincore::up_alpha_beta(returns, benchmark, 0.0, 1.0),
                   0.00047044263456097468, 1.1223286118980171);
  expect_pair_near(fincore::down_alpha_beta(returns, benchmark, 0.0, 1.0),
                   0.00058368902439021042, 1.0843780487804882);
}

TEST(ExtendedAlphaBeta, AlignsAllPairsByTimestampInsteadOfPosition) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", strategy_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(2), "benchmark", factor_values());

  expect_pair_near(fincore::alpha_beta(returns, benchmark, 0.0, 1.0),
                   0.01094515223921344, 1.1199165073730202);
  expect_pair_near(fincore::up_alpha_beta(returns, benchmark, 0.0, 1.0),
                   0.0086816438356165371, 1.2019041095890413);
  expect_pair_near(fincore::down_alpha_beta(returns, benchmark, 0.0, 1.0),
                   0.0089735137195121606, 1.0177149390243903);
}

TEST(ExtendedAlphaBeta, ComposesRiskFreeAndFiltersNonFinitePairs) {
  const auto returns =
      fincore::make_series(twelve_indices(), "returns", strategy_values());
  const auto benchmark =
      fincore::make_series(twelve_indices(), "benchmark", factor_values());
  auto non_finite_values = strategy_values();
  non_finite_values[2] = nan;
  non_finite_values[6] = std::numeric_limits<double>::infinity();
  const auto non_finite_returns =
      fincore::make_series(twelve_indices(), "returns", std::move(non_finite_values));

  expect_pair_near(fincore::alpha_beta(returns, benchmark, 0.001, 1.0),
                   0.001095465968586451, 1.1072133507853401);
  expect_pair_near(fincore::alpha_beta(non_finite_returns, benchmark, 0.0, 1.0),
                   0.0010221045543585827, 1.1071371204701275);
  expect_pair_near(fincore::up_alpha_beta(non_finite_returns, benchmark, 0.0, 1.0),
                   0.0004467010309279207, 1.1228221649484538);
  expect_pair_near(fincore::down_alpha_beta(non_finite_returns, benchmark, 0.0, 1.0),
                   0.0005995419847328876, 1.0844637404580153);
}

TEST(ExtendedAlphaBeta, SelectsRawBenchmarkBeforeRiskFreeAdjustment) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns",
      {0.0012500875, 0.0014701714999999999, -0.010265000000000002, -0.02116});
  const auto benchmark =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {0.0005, 0.0007, -0.01, -0.02});

  expect_pair_near(fincore::up_alpha_beta(returns, benchmark, 0.001, 1.0),
                   0.0008002975000001022, 1.1004199999999997);
  expect_pair_near(fincore::down_alpha_beta(returns, benchmark, 0.001, 1.0),
                   0.000719499999999984, 1.0895);
}

TEST(ExtendedAlphaBeta, IncludesZeroBenchmarkInDownSelection) {
  const auto returns =
      fincore::make_series({1, 2, 3}, "returns", {-0.010265, 0.0007, 0.011735});
  const auto benchmark =
      fincore::make_series({1, 2, 3}, "benchmark", {-0.01, 0.0, 0.01});

  expect_nan_pair(fincore::up_alpha_beta(returns, benchmark, 0.0, 1.0));
  expect_pair_near(fincore::down_alpha_beta(returns, benchmark, 0.0, 1.0),
                   0.0006999999999999229, 1.0965000000000003);
}

TEST(ExtendedAlphaBeta, ReturnsNanPairsForInvalidOrUndefinedInputs) {
  const auto returns = fincore::make_series({1, 2, 3}, "returns", {0.01, -0.02, 0.03});
  const auto flat_benchmark =
      fincore::make_series({1, 2, 3}, "benchmark", {0.01, 0.01, 0.01});
  const auto only_down =
      fincore::make_series({1, 2, 3}, "benchmark", {-0.01, -0.02, 0.0});
  const auto only_up =
      fincore::make_series({1, 2, 3}, "benchmark", {0.01, 0.02, 0.03});
  const auto no_common =
      fincore::make_series({4, 5, 6}, "benchmark", {0.01, 0.02, 0.03});

  expect_nan_pair(fincore::alpha_beta(returns, flat_benchmark, 0.0, 1.0));
  expect_nan_pair(fincore::up_alpha_beta(returns, only_down, 0.0, 1.0));
  expect_nan_pair(fincore::down_alpha_beta(returns, only_up, 0.0, 1.0));
  expect_nan_pair(fincore::alpha_beta(returns, no_common, 0.0, 1.0));
  expect_nan_pair(fincore::alpha_beta(returns, only_up, nan, 1.0));
  expect_nan_pair(fincore::alpha_beta(returns, only_up, 0.0, 0.0));
  expect_nan_pair(fincore::alpha_beta(
      returns, only_up, 0.0, std::numeric_limits<double>::infinity()));
}

TEST(ExtendedAlphaBeta, SupportsNamedColumnsAndRejectsStructuralErrors) {
  const auto returns = fincore::make_series(
      twelve_indices(), "strategy", strategy_values());
  const auto benchmark = fincore::make_series(
      twelve_indices(), "market", factor_values());

  expect_pair_near(
      fincore::alpha_beta(returns, benchmark, 0.0, 1.0, "strategy", "market"),
      0.00098825261780111084, 1.1072133507853401);
  EXPECT_THROW(
      fincore::up_alpha_beta(returns, benchmark, 0.0, 1.0, "missing", "market"),
      fincore::input_error);
  EXPECT_THROW(fincore::down_alpha_beta(returns, benchmark, 0.0, 1.0, "strategy",
                                         "missing"),
               fincore::input_error);
}

}  // namespace
