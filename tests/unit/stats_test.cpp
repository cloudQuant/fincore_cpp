#include <fincore/metrics/stats.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

using hurst_exponent_signature =
    double (*)(const fincore::dataframe&, std::string_view);
using stutzer_index_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);

static_assert(std::is_same_v<decltype(&fincore::hurst_exponent),
                             hurst_exponent_signature>);
static_assert(std::is_same_v<decltype(&fincore::stutzer_index),
                             stutzer_index_signature>);

using r_cubed_signature =
    double (*)(const fincore::dataframe&, const fincore::dataframe&,
               std::string_view, std::string_view);

static_assert(std::is_same_v<
              decltype(static_cast<r_cubed_signature>(&fincore::r_cubed)),
              r_cubed_signature>);

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
  const auto infinity_pair_returns = fincore::make_series(
      {1, 2, 3, 4}, "returns",
      {0.01, std::numeric_limits<double>::infinity(), -0.02, 0.03});
  const auto infinity_pair_benchmark =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {0.005, 0.1, -0.01, -0.01});
  const auto finite_pair_returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {0.01, nan, -0.02, 0.03});
  const auto finite_pair_benchmark =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {0.005, 0.01, nan, -0.01});

  EXPECT_NEAR(fincore::skewness(returns), -0.23906314692954447, 1e-12);
  EXPECT_TRUE(std::isnan(fincore::kurtosis(returns)));
  EXPECT_NEAR(fincore::serial_correlation(returns), -1.0, 1e-12);
  EXPECT_NEAR(fincore::win_rate(returns), 2.0 / 3.0, 1e-12);
  EXPECT_NEAR(fincore::loss_rate(returns), 1.0 / 3.0, 1e-12);

  EXPECT_NEAR(fincore::safe_correlation(infinity_pair_returns, infinity_pair_benchmark),
              0.11470786693528093, 1e-12);
  EXPECT_NEAR(fincore::relative_win_rate(infinity_pair_returns, infinity_pair_benchmark),
              2.0 / 3.0, 1e-12);
  EXPECT_NEAR(fincore::capm_r_squared(infinity_pair_returns, infinity_pair_benchmark),
              0.013157894736842115, 1e-12);
  EXPECT_NEAR(fincore::tracking_difference(infinity_pair_returns,
                                            infinity_pair_benchmark),
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

TEST(Stats, MatchesSignedRCubedPythonOracleAndDefaultColumns) {
  const auto returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {1.0, 2.0, 4.0, 8.0});
  const auto positive_benchmark =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {1.0, 3.0, 4.0, 7.0});
  const auto negative_benchmark = fincore::make_series(
      {1, 2, 3, 4}, "benchmark", {-1.0, -3.0, -4.0, -7.0});
  const auto perfect_returns =
      fincore::make_series({1, 2, 3, 4}, "returns", {1.0, 2.0, 3.0, 4.0});
  const auto perfect_positive =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {2.0, 4.0, 6.0, 8.0});
  const auto perfect_negative =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {4.0, 3.0, 2.0, 1.0});

  EXPECT_NEAR(fincore::r_cubed(returns, positive_benchmark),
              0.9407744656599973, 1e-12);
  EXPECT_NEAR(fincore::r_cubed(returns, negative_benchmark),
              -0.9407744656599973, 1e-12);
  EXPECT_NEAR(fincore::r_cubed(perfect_returns, perfect_positive), 1.0,
              1e-12);
  EXPECT_NEAR(fincore::r_cubed(perfect_returns, perfect_negative), -1.0,
              1e-12);
}

TEST(Stats, RCubedUsesTheDirectSignedPublicLeafCube) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4, 5}, "strategy", {.01, -.02, .03, -.01, .02});
  const auto benchmark = fincore::make_series(
      {1, 2, 3, 4, 5}, "market", {.005, -.01, .02, -.005, .015});
  const double correlation =
      fincore::safe_correlation(returns, benchmark, "strategy", "market");

  EXPECT_DOUBLE_EQ(
      fincore::r_cubed(returns, benchmark, "strategy", "market"),
      correlation * correlation * correlation);
}

TEST(Stats, RCubedAlignsTimestampIntersectionInLeftInputOrder) {
  const auto returns = fincore::make_series(
      {30, 10, 40, 20}, "returns", {4.0, 1.0, 8.0, 2.0});
  const auto benchmark = fincore::make_series(
      {20, 40, 10, 50, 30}, "benchmark", {3.0, 7.0, 0.0, 99.0, 5.0});
  const double correlation = fincore::safe_correlation(returns, benchmark);
  const double positional_cube = -0.0799905546687648;
  const double result = fincore::r_cubed(returns, benchmark);

  EXPECT_NEAR(correlation, 0.9285310170665338, 1e-12);
  EXPECT_NEAR(result, 0.8005514473838972, 1e-12);
  EXPECT_DOUBLE_EQ(result, correlation * correlation * correlation);
  EXPECT_GT(std::abs(result - positional_cube), 0.8);
}

TEST(Stats, RCubedInheritsPairFilteringAndInvalidNumericalBoundaries) {
  const auto pairwise_nan =
      fincore::make_series({1, 2, 3, 4}, "returns", {1.0, nan, 3.0, 4.0});
  const auto pairwise_benchmark =
      fincore::make_series({1, 2, 3, 4}, "benchmark", {2.0, 5.0, 6.0, 8.0});
  const auto no_common_returns =
      fincore::make_series({1, 2}, "returns", {.01, -.02});
  const auto no_common_benchmark =
      fincore::make_series({3, 4}, "benchmark", {.02, -.01});
  const auto one_finite_benchmark =
      fincore::make_series({1, 2}, "benchmark", {.02, nan});
  const auto constant =
      fincore::make_series({1, 2, 3}, "returns", {1.0, 1.0, 1.0});
  const auto varying =
      fincore::make_series({1, 2, 3}, "benchmark", {1.0, 2.0, 3.0});
  const auto tiny_variation =
      fincore::make_series({1, 2}, "returns", {0.0, 5e-16});
  const auto tiny_benchmark =
      fincore::make_series({1, 2}, "benchmark", {.01, -.02});
  const double maximum = std::numeric_limits<double>::max();
  const auto symmetric_maximum =
      fincore::make_series({1, 2}, "returns", {maximum, -maximum});
  const auto symmetric_benchmark =
      fincore::make_series({1, 2}, "benchmark", {1.0, -1.0});

  fincore::dataframe empty_returns;
  empty_returns.load_index(std::vector<fincore::timestamp>{});
  empty_returns.load_column<double>("returns", std::vector<double>{},
                                    hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe empty_benchmark;
  empty_benchmark.load_index(std::vector<fincore::timestamp>{});
  empty_benchmark.load_column<double>("benchmark", std::vector<double>{},
                                      hmdf::nan_policy::dont_pad_with_nans);

  EXPECT_NEAR(fincore::r_cubed(pairwise_nan, pairwise_benchmark), 1.0,
              1e-12);
  EXPECT_TRUE(
      std::isnan(fincore::r_cubed(no_common_returns, no_common_benchmark)));
  EXPECT_TRUE(
      std::isnan(fincore::r_cubed(no_common_returns, one_finite_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::r_cubed(empty_returns, empty_benchmark)));
  EXPECT_TRUE(std::isnan(fincore::r_cubed(constant, varying)));
  EXPECT_TRUE(std::isnan(fincore::r_cubed(tiny_variation, tiny_benchmark)));
  EXPECT_TRUE(
      std::isnan(fincore::r_cubed(symmetric_maximum, symmetric_benchmark)));
}

TEST(Stats, RCubedInheritsSignedInfinityFilteringDivergence) {
  const double infinity = std::numeric_limits<double>::infinity();
  const auto positive_infinity =
      fincore::make_series({1, 2, 3, 4}, "returns", {.01, infinity, -.02, .03});
  const auto negative_infinity = fincore::make_series(
      {1, 2, 3, 4}, "returns", {.01, -infinity, -.02, .03});
  const auto benchmark = fincore::make_series(
      {1, 2, 3, 4}, "benchmark", {.005, .1, -.01, -.01});

  EXPECT_NEAR(fincore::r_cubed(positive_infinity, benchmark),
              0.0015093140386221175, 1e-15);
  EXPECT_NEAR(fincore::r_cubed(negative_infinity, benchmark),
              0.0015093140386221175, 1e-15);
}

TEST(Stats, RCubedPreservesRoundedEndpointOvershoot) {
  const auto returns =
      fincore::make_series({1, 2}, "returns", {-10.0, -0.3});
  const auto benchmark =
      fincore::make_series({1, 2}, "benchmark", {-10.0, -0.3});
  const double correlation = fincore::safe_correlation(returns, benchmark);
  const double result = fincore::r_cubed(returns, benchmark);

  EXPECT_GT(correlation, 1.0);
  EXPECT_GT(result, 1.0);
  EXPECT_DOUBLE_EQ(result, correlation * correlation * correlation);
}

TEST(Stats, RCubedSupportsNamedAndBoundedNonterminatedSelectors) {
  const auto returns = fincore::make_series(
      {1, 2, 3, 4}, "strategy", {1.0, 2.0, 4.0, 8.0});
  const auto benchmark =
      fincore::make_series({1, 2, 3, 4}, "market", {1.0, 3.0, 4.0, 7.0});
  const std::array<char, 10> returns_storage{
      's', 't', 'r', 'a', 't', 'e', 'g', 'y', '!', '\0'};
  const std::array<char, 8> benchmark_storage{
      'm', 'a', 'r', 'k', 'e', 't', '!', '\0'};
  const std::string_view returns_selector{returns_storage.data(), 8U};
  const std::string_view benchmark_selector{benchmark_storage.data(), 6U};

  EXPECT_NEAR(fincore::r_cubed(returns, benchmark, returns_selector,
                               benchmark_selector),
              0.9407744656599973, 1e-12);
}

TEST(Stats, RCubedRejectsBothEmbeddedNulSelectorsBeforeDataframeAccess) {
  const auto valid_returns =
      fincore::make_series({1, 2}, "strategy", {.01, -.02});
  const auto valid_benchmark =
      fincore::make_series({1, 2}, "market", {.02, -.01});
  const std::array<char, 10> embedded_returns_storage{
      's', 't', 'r', 'a', 't', 'e', 'g', 'y', '\0', '!'};
  const std::array<char, 8> embedded_benchmark_storage{
      'm', 'a', 'r', 'k', 'e', 't', '\0', '!'};
  const std::string_view embedded_returns{embedded_returns_storage.data(),
                                          embedded_returns_storage.size()};
  const std::string_view embedded_benchmark{
      embedded_benchmark_storage.data(), embedded_benchmark_storage.size()};
  fincore::dataframe invalid_frame;

  expect_input_error_message(
      [&] {
        return fincore::r_cubed(valid_returns, invalid_frame,
                                embedded_returns, "missing");
      },
      "returns column name must not contain an embedded NUL");
  expect_input_error_message(
      [&] {
        return fincore::r_cubed(invalid_frame, valid_benchmark, "missing",
                                embedded_benchmark);
      },
      "benchmark column name must not contain an embedded NUL");
  expect_input_error_message(
      [&] {
        return fincore::r_cubed(invalid_frame, invalid_frame,
                                embedded_returns, embedded_benchmark);
      },
      "returns column name must not contain an embedded NUL");
}

TEST(Stats, RCubedRetainsBothSidesStructuralValidationOrder) {
  fincore::dataframe typed_empty_returns;
  typed_empty_returns.load_index(std::vector<fincore::timestamp>{});
  typed_empty_returns.load_column<double>(
      "returns", std::vector<double>{},
      hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe missing_frame;
  fincore::dataframe wrong_returns;
  wrong_returns.load_index(std::vector<fincore::timestamp>{});
  wrong_returns.load_column<int>("returns", std::vector<int>{},
                                 hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe wrong_benchmark;
  wrong_benchmark.load_index(std::vector<fincore::timestamp>{});
  wrong_benchmark.load_column<int>("benchmark", std::vector<int>{},
                                   hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe malformed_returns;
  malformed_returns.load_index(std::vector<fincore::timestamp>{1});
  malformed_returns.load_column<double>(
      "returns", std::vector<double>{},
      hmdf::nan_policy::dont_pad_with_nans);
  fincore::dataframe malformed_benchmark;
  malformed_benchmark.load_index(std::vector<fincore::timestamp>{1});
  malformed_benchmark.load_column<double>(
      "benchmark", std::vector<double>{},
      hmdf::nan_policy::dont_pad_with_nans);

  expect_input_error_message(
      [&] {
        return fincore::r_cubed(typed_empty_returns, missing_frame, "",
                                "missing");
      },
      "column name must not be empty");
  expect_input_error_message(
      [&] {
        return fincore::r_cubed(typed_empty_returns, missing_frame, "returns",
                                "");
      },
      "column name must not be empty");
  expect_input_error_message(
      [&] { return fincore::r_cubed(missing_frame, missing_frame); },
      "required double column is absent");
  expect_input_error_message(
      [&] {
        return fincore::r_cubed(typed_empty_returns, missing_frame);
      },
      "required double column is absent");
  expect_input_error_message(
      [&] { return fincore::r_cubed(wrong_returns, missing_frame); },
      "required column is not a double column");
  expect_input_error_message(
      [&] { return fincore::r_cubed(typed_empty_returns, wrong_benchmark); },
      "required column is not a double column");
  expect_input_error_message(
      [&] { return fincore::r_cubed(malformed_returns, missing_frame); },
      "required double column must match the frame index length");
  expect_input_error_message(
      [&] {
        return fincore::r_cubed(typed_empty_returns, malformed_benchmark);
      },
      "required double column must match the frame index length");
}

TEST(Stats, RCubedReturnsAnOwnedScalar) {
  const double result = [] {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4}, "strategy", {1.0, 2.0, 4.0, 8.0});
    const auto benchmark =
        fincore::make_series({1, 2, 3, 4}, "market", {1.0, 3.0, 4.0, 7.0});
    const std::array<char, 10> returns_storage{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y', '!', '\0'};
    const std::array<char, 8> benchmark_storage{
        'm', 'a', 'r', 'k', 'e', 't', '!', '\0'};
    return fincore::r_cubed(
        returns, benchmark,
        std::string_view{returns_storage.data(), 8U},
        std::string_view{benchmark_storage.data(), 6U});
  }();

  EXPECT_NEAR(result, 0.9407744656599973, 1e-12);
}

std::vector<fincore::timestamp> hurst_index(std::size_t count) {
    std::vector<fincore::timestamp> index(count);
    for (std::size_t position = 0; position < count; ++position) {
        index[position] = static_cast<fincore::timestamp>(position + 1);
    }
    return index;
}

std::vector<double> hurst_long_values(double frequency, double offset,
                                      bool cosine) {
    std::vector<double> values;
    values.reserve(120);
    for (int index = 0; index < 120; ++index) {
        const double argument = static_cast<double>(index) * frequency;
        values.push_back(0.01 * (cosine ? std::cos(argument) : std::sin(argument)) +
                        offset);
    }
    return values;
}

TEST(Stats, HurstExponentMatchesPythonOracleForShortAndLongSeries) {
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto long_sin = fincore::make_series(
        hurst_index(120), "returns", hurst_long_values(0.7, 0.0, false));
    const auto long_cos = fincore::make_series(
        hurst_index(120), "returns", hurst_long_values(1.3, 0.002, true));

    EXPECT_NEAR(fincore::hurst_exponent(ordinary), 1.0, 1e-12);
    EXPECT_NEAR(fincore::hurst_exponent(long_sin), 0.6317378848594388, 1e-6);
    EXPECT_NEAR(fincore::hurst_exponent(long_cos), 0.385513237072807, 1e-6);
}

TEST(Stats, HurstExponentGatesShortNaNConstantAndSpikeSeries) {
    const auto short_frame =
        fincore::make_series(hurst_index(7), "returns",
                             std::vector<double>(7, 0.01));
    const auto exact_constant =
        fincore::make_series(hurst_index(10), "returns",
                             std::vector<double>(10, 0.5));
    const auto flat_zero =
        fincore::make_series(hurst_index(10), "returns",
                             std::vector<double>(10, 0.0));
    const auto nan_mixed = fincore::make_series(
        hurst_index(10), "returns",
        {0.01, nan, 0.02, -0.03, 0.01, 0.02, -0.01, 0.03, nan, 0.01});
    const auto two_spike = fincore::make_series(
        hurst_index(16), "returns",
        {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1, -0.1, 0.0, 0.0, 0.0,
         0.0, 0.0, 0.0});
    const auto eight_values = fincore::make_series(
        hurst_index(8), "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01});

    EXPECT_TRUE(std::isnan(fincore::hurst_exponent(short_frame)));
    EXPECT_TRUE(std::isnan(fincore::hurst_exponent(exact_constant)));
    EXPECT_TRUE(std::isnan(fincore::hurst_exponent(flat_zero)));
    EXPECT_NEAR(fincore::hurst_exponent(nan_mixed), 1.0, 1e-12);
    EXPECT_NEAR(fincore::hurst_exponent(two_spike), 0.7280957897994347, 1e-6);
    EXPECT_NEAR(fincore::hurst_exponent(eight_values), 1.0, 1e-12);
}

TEST(Stats, HurstExponentSupportsSelectorsAndRejectsNul) {
    const auto strategy = fincore::make_series(
        hurst_index(10), "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};

    EXPECT_DOUBLE_EQ(fincore::hurst_exponent(strategy, bounded_selector),
                     fincore::hurst_exponent(strategy, "strategy"));
    EXPECT_THROW(
        static_cast<void>(fincore::hurst_exponent(strategy, embedded_selector)),
        fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::hurst_exponent(strategy, "")),
                 fincore::input_error);
}

TEST(Stats, HurstExponentReturnsOwnedScalarAfterInputLifetimeEnds) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto frame = fincore::make_series(
            hurst_index(10), "returns",
            {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
        return fincore::hurst_exponent(frame, selector);
    }();

    EXPECT_NEAR(result, 1.0, 1e-12);
}

TEST(Stats, StutzerIndexMatchesPythonOracleForBothSigns) {
    const auto ordinary = fincore::make_series(
        hurst_index(10), "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto negative = fincore::make_series(
        hurst_index(10), "returns",
        {-0.01, -0.02, 0.05, -0.03, -0.01, 0.02, -0.04, -0.01, -0.02, 0.03});

    EXPECT_NEAR(fincore::stutzer_index(ordinary), 0.14630936783628698, 1e-6);
    EXPECT_NEAR(fincore::stutzer_index(ordinary, 0.001),
                0.11013897196625373, 1e-6);
    EXPECT_NEAR(fincore::stutzer_index(negative),
                -0.14630936783628545, 1e-6);
    EXPECT_NEAR(fincore::stutzer_index(negative, -0.001),
                -0.11013897196625273, 1e-6);
}

TEST(Stats, StutzerIndexGatesShortZeroAndNaNInputs) {
    const auto zeros = fincore::make_series(hurst_index(4), "returns",
                                            std::vector<double>(4, 0.0));
    const auto raw_zero = fincore::make_series({}, "returns", {});
    const auto raw_one = fincore::make_series({1}, "returns", {0.01});
    const auto one_clean = fincore::make_series(
        hurst_index(4), "returns", {nan, nan, 0.01, nan});
    const auto all_nan = fincore::make_series(
        hurst_index(3), "returns", {nan, nan, nan});
    const auto nan_mixed = fincore::make_series(
        hurst_index(5), "returns", {0.01, nan, 0.02, -0.03, 0.01});

    EXPECT_DOUBLE_EQ(fincore::stutzer_index(zeros), 0.0);
    EXPECT_TRUE(std::isnan(fincore::stutzer_index(raw_zero)));
    EXPECT_TRUE(std::isnan(fincore::stutzer_index(raw_one)));
    EXPECT_TRUE(std::isnan(fincore::stutzer_index(one_clean)));
    EXPECT_TRUE(std::isnan(fincore::stutzer_index(all_nan)));
    EXPECT_NEAR(fincore::stutzer_index(nan_mixed), 0.12766869919010967, 1e-6);
}

TEST(Stats, StutzerIndexHandlesConstantsOverflowAndInfinity) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto positive = fincore::make_series(hurst_index(4), "returns",
                                               std::vector<double>(4, 0.01));
    const auto negative = fincore::make_series(hurst_index(4), "returns",
                                               std::vector<double>(4, -0.01));
    const auto overflow = fincore::make_series(
        hurst_index(4), "returns",
        {-5.0, 0.01, 0.02, 0.03});
    const auto positive_inf = fincore::make_series(
        hurst_index(4), "returns", {infinity, 0.01, -0.02, 0.03});
    const auto negative_inf = fincore::make_series(
        hurst_index(4), "returns", {-infinity, 0.01, -0.02, 0.03});

    EXPECT_NEAR(fincore::stutzer_index(positive), 1.0, 1e-6);
    EXPECT_NEAR(fincore::stutzer_index(negative), -1.0, 1e-6);
    EXPECT_NEAR(fincore::stutzer_index(overflow), -0.7294427821824202, 1e-6);
    EXPECT_NEAR(fincore::stutzer_index(positive_inf), 0.8242437395987398,
                1e-6);
    EXPECT_NEAR(fincore::stutzer_index(negative_inf), -0.7585275606810086,
                1e-6);
}

TEST(Stats, StutzerIndexSupportsSelectorsAndRejectsNul) {
    const auto strategy = fincore::make_series(
        hurst_index(10), "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};

    EXPECT_NEAR(fincore::stutzer_index(strategy, 0.0, bounded_selector),
                0.14630936783628698, 1e-6);
    EXPECT_THROW(
        static_cast<void>(fincore::stutzer_index(strategy, 0.0,
                                                 embedded_selector)),
        fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::stutzer_index(strategy, 0.0, "")),
                 fincore::input_error);
}

TEST(Stats, StutzerIndexReturnsOwnedScalarAfterInputLifetimeEnds) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto frame = fincore::make_series(
            hurst_index(10), "returns",
            {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
        return fincore::stutzer_index(frame, 0.0, selector);
    }();

    EXPECT_NEAR(result, 0.14630936783628698, 1e-6);
}

}  // namespace
