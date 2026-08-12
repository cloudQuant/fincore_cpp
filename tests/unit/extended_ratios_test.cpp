#include <fincore/metrics/ratios.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

using common_sense_ratio_signature =
    double (*)(const fincore::dataframe&, std::string_view);

static_assert(std::is_same_v<
              decltype(static_cast<common_sense_ratio_signature>(
                  &fincore::common_sense_ratio)),
              common_sense_ratio_signature>);

TEST(ExtendedRatios, MatchesCommonSenseRatioPythonOracle) {
  const auto mixed =
      fincore::make_series({1, 2, 3, 4}, "returns", {.01, -.02, .03, -.01});
  const auto zeros_mixed = fincore::make_series(
      {1, 2, 3, 4, 5}, "returns", {.01, 0., -.02, .03, 0.});
  const auto zero_tail = fincore::make_series({1, 2}, "returns", {0., .01});

  EXPECT_NEAR(fincore::common_sense_ratio(mixed), 1.4594594594594592, 1e-12);
  EXPECT_NEAR(fincore::common_sense_ratio(zeros_mixed), 1.0833333333333333,
              1e-12);
  EXPECT_DOUBLE_EQ(fincore::common_sense_ratio(zero_tail), 19.0);
}

TEST(ExtendedRatios, PreservesCommonSenseRatioRawLengthAndBoundaryBranches) {
  fincore::dataframe zero_rows;
  zero_rows.load_index(std::vector<fincore::timestamp>{});
  zero_rows.load_column<double>("returns", std::vector<double>{},
                                hmdf::nan_policy::dont_pad_with_nans);

  const auto single = fincore::make_series({1}, "returns", {.01});
  const auto one_finite_two_raw =
      fincore::make_series({1, 2}, "returns", {.01, nan});
  const auto all_nan = fincore::make_series({1, 2}, "returns", {nan, nan});
  const auto no_win =
      fincore::make_series({1, 2, 3}, "returns", {-.01, 0., -.02});
  const auto all_win =
      fincore::make_series({1, 2, 3}, "returns", {.01, .02, .03});

  EXPECT_TRUE(std::isnan(fincore::common_sense_ratio(zero_rows)));
  EXPECT_TRUE(std::isnan(fincore::common_sense_ratio(single)));
  EXPECT_TRUE(std::isnan(fincore::common_sense_ratio(all_nan)));
  EXPECT_TRUE(std::isnan(fincore::common_sense_ratio(no_win)));

  const double one_finite_result =
      fincore::common_sense_ratio(one_finite_two_raw);
  EXPECT_TRUE(std::isinf(one_finite_result));
  EXPECT_GT(one_finite_result, 0.0);

  const double all_win_result = fincore::common_sense_ratio(all_win);
  EXPECT_TRUE(std::isinf(all_win_result));
  EXPECT_GT(all_win_result, 0.0);
}

TEST(ExtendedRatios, UsesAcceptedNativeFiniteFilteringForCommonSenseRatio) {
  const auto positive_infinity = fincore::make_series(
      {1, 2, 3, 4}, "returns",
      {.01, std::numeric_limits<double>::infinity(), -.02, .03});
  const auto negative_infinity = fincore::make_series(
      {1, 2, 3, 4}, "returns",
      {.01, -std::numeric_limits<double>::infinity(), -.02, .03});

  EXPECT_NEAR(fincore::common_sense_ratio(positive_infinity),
              3.294117647058822, 1e-12);
  EXPECT_NEAR(fincore::common_sense_ratio(negative_infinity),
              3.294117647058822, 1e-12);
}

TEST(ExtendedRatios, ValidatesCommonSenseRatioSelectorBeforeRawLength) {
  const auto strategy = fincore::make_series({1, 2, 3, 4}, "strategy",
                                              {.01, -.02, .03, -.01});
  const auto single_strategy =
      fincore::make_series({1}, "strategy", std::vector<double>{.01});
  const std::array<char, 8> bounded_name{'s', 't', 'r', 'a',
                                          't', 'e', 'g', 'y'};
  const std::string_view bounded_selector{bounded_name.data(),
                                          bounded_name.size()};
  const std::array<char, 13> embedded_name{
      's', 't', 'r', 'a', 't', 'e', 'g', 'y', '\0', 'e', 'v', 'i', 'l'};
  const std::string_view embedded_selector{embedded_name.data(),
                                           embedded_name.size()};

  EXPECT_NEAR(fincore::common_sense_ratio(strategy, bounded_selector),
              1.4594594594594592, 1e-12);
  EXPECT_THROW(static_cast<void>(
                   fincore::common_sense_ratio(single_strategy,
                                               embedded_selector)),
               fincore::input_error);
  EXPECT_THROW(
      static_cast<void>(fincore::common_sense_ratio(single_strategy, "")),
      fincore::input_error);
  EXPECT_THROW(
      static_cast<void>(
          fincore::common_sense_ratio(single_strategy, "missing")),
      fincore::input_error);

  fincore::dataframe wrong_type;
  wrong_type.load_index(std::vector<fincore::timestamp>{1});
  wrong_type.load_column<int>("returns", std::vector<int>{1},
                              hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(fincore::common_sense_ratio(wrong_type)),
               fincore::input_error);

  fincore::dataframe short_column;
  short_column.load_index(std::vector<fincore::timestamp>{1, 2});
  short_column.load_column<double>("returns", std::vector<double>{.01},
                                   hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(static_cast<void>(fincore::common_sense_ratio(short_column)),
               fincore::input_error);
}

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
