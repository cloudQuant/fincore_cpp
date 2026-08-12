#include <fincore/metrics/risk.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

using var_excess_return_signature =
    double (*)(const fincore::dataframe&, double, double, fincore::frequency,
               std::optional<double>, std::string_view);
using trading_value_at_risk_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);

static_assert(std::is_same_v<
              decltype(static_cast<var_excess_return_signature>(
                  &fincore::var_excess_return)),
              var_excess_return_signature>);
static_assert(std::is_same_v<
              decltype(static_cast<trading_value_at_risk_signature>(
                  &fincore::trading_value_at_risk)),
              trading_value_at_risk_signature>);

template <typename callable>
void expect_input_error_message(callable&& invoke, const char* message) {
  try {
    static_cast<void>(std::forward<callable>(invoke)());
    FAIL() << "expected fincore::input_error";
  } catch (const fincore::input_error& error) {
    EXPECT_STREQ(error.what(), message);
  } catch (const std::exception& error) {
    FAIL() << "unexpected exception: " << error.what();
  }
}

TEST(ExtendedRisk, MatchesVarExcessReturnDefaultPythonOracle) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns", {.10, -.05, .02, -.01});

  EXPECT_NEAR(fincore::var_excess_return(returns), 649.7873043614145, 1e-12);
}

TEST(ExtendedRisk, MatchesTradingValueAtRiskDefaultPythonOracle) {
  const auto returns =
      fincore::make_series({1, 2, 3}, "returns", {.01, -.02, .03});

  EXPECT_NEAR(fincore::trading_value_at_risk(returns),
              -0.043665562901805, 1e-15);
}

TEST(ExtendedRisk, MatchesTradingValueAtRiskPythonOracleAcrossSigmaValues) {
  const auto returns =
      fincore::make_series({1, 2, 3}, "returns", {.01, -.02, .03});
  const double infinity = std::numeric_limits<double>::infinity();

  EXPECT_NEAR(fincore::trading_value_at_risk(returns, 1.0),
              -0.018499448117569168, 1e-15);
  EXPECT_NEAR(fincore::trading_value_at_risk(returns, 0.0),
              0.006666666666666665, 1e-15);
  EXPECT_NEAR(fincore::trading_value_at_risk(returns, -3.0),
              0.08216501101937417, 1e-15);
  EXPECT_TRUE(std::isnan(fincore::trading_value_at_risk(returns, nan)));

  const double negative_infinite =
      fincore::trading_value_at_risk(returns, infinity);
  EXPECT_TRUE(std::isinf(negative_infinite));
  EXPECT_LT(negative_infinite, 0.0);

  const double positive_infinite =
      fincore::trading_value_at_risk(returns, -infinity);
  EXPECT_TRUE(std::isinf(positive_infinite));
  EXPECT_GT(positive_infinite, 0.0);
}

TEST(ExtendedRisk, UsesSampleRatherThanPopulationStandardDeviation) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns", {.02, -.01, .04, 0.0});
  constexpr double mean = .0125;
  constexpr double squared_deviations = .001475;
  const double expected_sample =
      mean - 1.5 * std::sqrt(squared_deviations / 3.0);
  const double population_mutant =
      mean - 1.5 * std::sqrt(squared_deviations / 4.0);

  const double result = fincore::trading_value_at_risk(returns, 1.5);

  EXPECT_NEAR(result, expected_sample, 1e-15);
  EXPECT_GT(std::abs(result - population_mutant), 1e-3);
}

TEST(ExtendedRisk, ShortcutsTypedTradingValueAtRiskSeriesBeforeArithmetic) {
  fincore::dataframe empty;
  empty.load_index(std::vector<fincore::timestamp>{});
  empty.load_column<double>("returns", std::vector<double>{},
                            hmdf::nan_policy::dont_pad_with_nans);
  const auto one = fincore::make_series({1}, "returns", {.10});
  const double infinity = std::numeric_limits<double>::infinity();

  EXPECT_TRUE(std::isnan(
      fincore::trading_value_at_risk(empty, infinity)));
  EXPECT_TRUE(std::isnan(fincore::trading_value_at_risk(one, nan)));
}

TEST(ExtendedRisk, PreservesConstantTradingValueAtRiskArithmetic) {
  const auto flat_positive =
      fincore::make_series({1, 2}, "returns", {.1, .1});
  const auto flat_zero =
      fincore::make_series({1, 2}, "returns", {0.0, 0.0});
  const double infinity = std::numeric_limits<double>::infinity();

  EXPECT_DOUBLE_EQ(fincore::trading_value_at_risk(flat_positive), .1);
  EXPECT_DOUBLE_EQ(fincore::trading_value_at_risk(flat_zero), 0.0);
  EXPECT_TRUE(std::isnan(
      fincore::trading_value_at_risk(flat_positive, infinity)));
  EXPECT_TRUE(
      std::isnan(fincore::trading_value_at_risk(flat_zero, infinity)));
}

TEST(ExtendedRisk, DoesNotFilterNonfiniteTradingValueAtRiskObservations) {
  const auto nan_mixed =
      fincore::make_series({1, 2, 3}, "returns", {.1, -.1, nan});
  const auto positive_infinity_mixed = fincore::make_series(
      {1, 2, 3}, "returns",
      {.1, -.1, std::numeric_limits<double>::infinity()});
  const auto negative_infinity_mixed = fincore::make_series(
      {1, 2, 3}, "returns",
      {.1, -.1, -std::numeric_limits<double>::infinity()});
  const auto both_infinities = fincore::make_series(
      {1, 2, 3, 4}, "returns",
      {.1, -.1, std::numeric_limits<double>::infinity(),
       -std::numeric_limits<double>::infinity()});

  EXPECT_TRUE(std::isnan(fincore::trading_value_at_risk(nan_mixed)));
  EXPECT_TRUE(std::isnan(
      fincore::trading_value_at_risk(positive_infinity_mixed)));
  EXPECT_TRUE(std::isnan(
      fincore::trading_value_at_risk(negative_infinity_mixed)));
  EXPECT_TRUE(std::isnan(fincore::trading_value_at_risk(both_infinities)));
}

TEST(ExtendedRisk, PreservesSignedInfiniteTradingValueAtRiskResults) {
  const double maximum = std::numeric_limits<double>::max();
  const auto symmetric_maximum =
      fincore::make_series({1, 2}, "returns", {maximum, -maximum});

  const double negative_infinite =
      fincore::trading_value_at_risk(symmetric_maximum);
  EXPECT_TRUE(std::isinf(negative_infinite));
  EXPECT_LT(negative_infinite, 0.0);

  const double positive_infinite =
      fincore::trading_value_at_risk(symmetric_maximum, -2.0);
  EXPECT_TRUE(std::isinf(positive_infinite));
  EXPECT_GT(positive_infinite, 0.0);
}

TEST(ExtendedRisk, ValidatesTradingValueAtRiskSelectorBeforeRawLength) {
  const auto strategy =
      fincore::make_series({1, 2, 3}, "strategy", {.01, -.02, .03});
  const auto single_strategy =
      fincore::make_series({1}, "strategy", std::vector<double>{.10});
  const std::array<char, 9> bounded_storage{
      's', 't', 'r', 'a', 't', 'e', 'g', 'y', '!'};
  const std::string_view bounded_selector{bounded_storage.data(), 8U};
  const std::array<char, 10> embedded_storage{
      's', 't', 'r', 'a', 't', 'e', 'g', 'y', '\0', '!'};
  const std::string_view embedded_selector{embedded_storage.data(),
                                           embedded_storage.size()};

  EXPECT_NEAR(fincore::trading_value_at_risk(strategy, 2.0,
                                              bounded_selector),
              -0.043665562901805, 1e-15);
  expect_input_error_message(
      [&] {
        return fincore::trading_value_at_risk(single_strategy, 2.0,
                                               embedded_selector);
      },
      "column name must not contain an embedded NUL");
  expect_input_error_message(
      [&] { return fincore::trading_value_at_risk(single_strategy, 2.0, ""); },
      "column name must not be empty");
  expect_input_error_message(
      [&] {
        return fincore::trading_value_at_risk(single_strategy, 2.0,
                                               "missing");
      },
      "required double column is absent");

  fincore::dataframe wrong_type;
  wrong_type.load_index(std::vector<fincore::timestamp>{1});
  wrong_type.load_column<int>("returns", std::vector<int>{1},
                              hmdf::nan_policy::dont_pad_with_nans);
  expect_input_error_message(
      [&] { return fincore::trading_value_at_risk(wrong_type); },
      "required column is not a double column");

  fincore::dataframe mismatched;
  mismatched.load_index(std::vector<fincore::timestamp>{1});
  mismatched.load_column<double>("returns", std::vector<double>{},
                                 hmdf::nan_policy::dont_pad_with_nans);
  expect_input_error_message(
      [&] { return fincore::trading_value_at_risk(mismatched); },
      "required double column must match the frame index length");
}

TEST(ExtendedRisk, MatchesVarExcessReturnPythonOracleAcrossParameters) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns", {.10, -.05, .02, -.01});
  const auto endpoint_returns =
      fincore::make_series({1, 2, 3}, "returns", {.10, -.20, .03});

  EXPECT_NEAR(
      fincore::var_excess_return(returns, .05, 0.0,
                                 fincore::frequency::weekly),
      22.994057986872573, 1e-12);
  EXPECT_NEAR(fincore::var_excess_return(
                  returns, .05, 0.0, fincore::frequency::daily, 7.0),
              2.2423926838665866, 1e-12);
  EXPECT_NEAR(fincore::var_excess_return(endpoint_returns, 0.0),
              -4.998700305323209, 1e-12);
  EXPECT_NEAR(fincore::var_excess_return(endpoint_returns, 1.0),
              -9.997400610646418, 1e-12);
}

TEST(ExtendedRisk, ComposesAcceptedPublicLeavesForFiniteInputs) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "strategy", {.10, -.05, .02, -.01});
  struct finite_case {
    double cutoff;
    double risk_free;
    fincore::frequency period;
    std::optional<double> annualization;
  };
  const std::array cases{
      finite_case{.05, 0.0, fincore::frequency::daily, std::nullopt},
      finite_case{.40, .01, fincore::frequency::weekly, std::nullopt},
      finite_case{.75, -.02, fincore::frequency::monthly, 7.0},
  };

  for (const auto& test_case : cases) {
    const double factor = fincore::annualization_factor(
        test_case.period, test_case.annualization);
    const double annualized_return =
        fincore::annual_return(returns, factor, "strategy");
    const double value_at_risk = fincore::historical_value_at_risk(
        returns, test_case.cutoff, "strategy");
    const double expected =
        (annualized_return - test_case.risk_free) / std::abs(value_at_risk);

    EXPECT_NEAR(fincore::var_excess_return(
                    returns, test_case.cutoff, test_case.risk_free,
                    test_case.period, test_case.annualization, "strategy"),
                expected, 1e-12);
  }
}

TEST(ExtendedRisk, PreservesVarExcessReturnFrequencyAndCutoffOrder) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns", {.10, -.05, .02, -.01});
  constexpr auto unknown_frequency = static_cast<fincore::frequency>(999);

  expect_input_error_message(
      [&] {
        return fincore::var_excess_return(returns, -.1, 0.0,
                                          unknown_frequency);
      },
      "unrecognized frequency");
  EXPECT_NEAR(fincore::var_excess_return(returns, .05, 0.0,
                                         unknown_frequency, 7.0),
              2.2423926838665866, 1e-12);
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(
      returns, -.1, 0.0, unknown_frequency, 7.0)));
}

TEST(ExtendedRisk, PreservesNativeAnnualizationOverrideBoundary) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns", {.10, -.05, .02, -.01});
  const double infinity = std::numeric_limits<double>::infinity();

  EXPECT_DOUBLE_EQ(fincore::var_excess_return(
                       returns, .05, 0.0, fincore::frequency::daily, 0.0),
                   0.0);
  EXPECT_NEAR(fincore::var_excess_return(
                  returns, .05, 0.0, fincore::frequency::daily, -3.0),
              -0.8982857935175003, 1e-12);
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(
      returns, .05, 0.0, fincore::frequency::daily, nan)));
  const double positive_infinite = fincore::var_excess_return(
      returns, .05, 0.0, fincore::frequency::daily, infinity);
  EXPECT_TRUE(std::isinf(positive_infinite));
  EXPECT_GT(positive_infinite, 0.0);
  EXPECT_NEAR(fincore::var_excess_return(
                  returns, .05, 0.0, fincore::frequency::daily, -infinity),
              -22.727272727272727, 1e-12);
}

TEST(ExtendedRisk, ShortcutsRawLengthBeforeNumericalParameters) {
  fincore::dataframe empty;
  empty.load_index(std::vector<fincore::timestamp>{});
  empty.load_column<double>("returns", std::vector<double>{},
                            hmdf::nan_policy::dont_pad_with_nans);
  const auto one = fincore::make_series({1}, "returns", {.10});
  constexpr auto unknown_frequency = static_cast<fincore::frequency>(999);
  const double infinity = std::numeric_limits<double>::infinity();

  EXPECT_TRUE(std::isnan(fincore::var_excess_return(
      empty, -.1, infinity, unknown_frequency, std::nullopt)));
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(
      one, nan, nan, unknown_frequency, -3.0)));
}

TEST(ExtendedRisk, ReturnsNanForUndefinedVarExcessReturnInputs) {
  const auto finite = fincore::make_series(
      {1, 2, 3, 4}, "returns", {.10, -.05, .02, -.01});
  const auto zero_var =
      fincore::make_series({1, 2}, "returns", {0.0, 0.0});
  const auto nan_return =
      fincore::make_series({1, 2}, "returns", {-.1, nan});
  const auto positive_infinite_return = fincore::make_series(
      {1, 2}, "returns", {-.1, std::numeric_limits<double>::infinity()});
  const auto negative_infinite_return = fincore::make_series(
      {1, 2}, "returns", {-.1, -std::numeric_limits<double>::infinity()});

  EXPECT_TRUE(std::isnan(fincore::var_excess_return(zero_var)));
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(finite, -.1)));
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(finite, 1.1)));
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(finite, nan)));
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(nan_return)));
  EXPECT_TRUE(std::isnan(
      fincore::var_excess_return(positive_infinite_return)));
  EXPECT_TRUE(std::isnan(
      fincore::var_excess_return(negative_infinite_return)));
}

TEST(ExtendedRisk, ReturnsNanForNondegenerateZeroValueAtRisk) {
  const auto returns =
      fincore::make_series({1, 2}, "returns", {0.0, 0.1});

  EXPECT_GT(fincore::annual_return(returns), 0.0);
  EXPECT_DOUBLE_EQ(fincore::historical_value_at_risk(returns, 0.0), 0.0);
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(returns, 0.0)));
}

TEST(ExtendedRisk, PreservesDirectIeeeVarExcessReturnArithmetic) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "returns", {.10, -.05, .02, -.01});
  const double infinity = std::numeric_limits<double>::infinity();

  const double negative_infinite =
      fincore::var_excess_return(returns, .05, infinity);
  EXPECT_TRUE(std::isinf(negative_infinite));
  EXPECT_LT(negative_infinite, 0.0);
  EXPECT_TRUE(std::isnan(fincore::var_excess_return(returns, .05, nan)));

  const auto overflowing_annual_return =
      fincore::make_series({1, 2}, "returns", {1.0, 1.0});
  const double positive_infinite = fincore::var_excess_return(
      overflowing_annual_return, .05, 0.0, fincore::frequency::daily,
      std::numeric_limits<double>::max());
  EXPECT_TRUE(std::isinf(positive_infinite));
  EXPECT_GT(positive_infinite, 0.0);
}

TEST(ExtendedRisk, ValidatesVarExcessReturnSelectorBeforeRawLength) {
  const auto strategy = fincore::make_series(
      {1, 2, 3, 4}, "strategy", {.10, -.05, .02, -.01});
  const auto single_strategy =
      fincore::make_series({1}, "strategy", std::vector<double>{.10});
  const std::array<char, 8> bounded_name{'s', 't', 'r', 'a',
                                          't', 'e', 'g', 'y'};
  const std::string_view bounded_selector{bounded_name.data(),
                                          bounded_name.size()};
  const std::array<char, 13> embedded_name{
      's', 't', 'r', 'a', 't', 'e', 'g', 'y', '\0', 'e', 'v', 'i', 'l'};
  const std::string_view embedded_selector{embedded_name.data(),
                                           embedded_name.size()};

  EXPECT_NEAR(fincore::var_excess_return(
                  strategy, .05, 0.0, fincore::frequency::daily,
                  std::nullopt, bounded_selector),
              649.7873043614145, 1e-12);
  expect_input_error_message(
      [&] {
        return fincore::var_excess_return(
            single_strategy, .05, 0.0, fincore::frequency::daily,
            std::nullopt, embedded_selector);
      },
      "column name must not contain an embedded NUL");
  expect_input_error_message(
      [&] {
        return fincore::var_excess_return(
            single_strategy, .05, 0.0, fincore::frequency::daily,
            std::nullopt, "");
      },
      "column name must not be empty");
  expect_input_error_message(
      [&] {
        return fincore::var_excess_return(
            single_strategy, -.1, 0.0, static_cast<fincore::frequency>(999),
            std::nullopt, "missing");
      },
      "required double column is absent");

  fincore::dataframe wrong_type;
  wrong_type.load_index(std::vector<fincore::timestamp>{1});
  wrong_type.load_column<int>("returns", std::vector<int>{1},
                              hmdf::nan_policy::dont_pad_with_nans);
  expect_input_error_message(
      [&] { return fincore::var_excess_return(wrong_type); },
      "required column is not a double column");

  fincore::dataframe short_column;
  short_column.load_index(std::vector<fincore::timestamp>{1, 2});
  short_column.load_column<double>("returns", std::vector<double>{.01},
                                   hmdf::nan_policy::dont_pad_with_nans);
  expect_input_error_message(
      [&] { return fincore::var_excess_return(short_column); },
      "required double column must match the frame index length");
}

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
