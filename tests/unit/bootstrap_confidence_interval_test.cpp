#include <fincore/simulation/bootstrap.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using confidence_interval_function = fincore::bootstrap_confidence_interval_result (*) (
    const fincore::dataframe&, std::size_t, double, fincore::bootstrap_statistic,
    std::optional<std::uint64_t>, std::string_view);

static_assert(std::is_same_v<decltype(&fincore::bootstrap_confidence_interval),
                             confidence_interval_function>);
static_assert(std::is_same_v<decltype(fincore::bootstrap_confidence_interval_result::lower),
                             double>);
static_assert(std::is_same_v<decltype(fincore::bootstrap_confidence_interval_result::upper),
                             double>);

constexpr double tolerance = 1.0e-12;

fincore::dataframe nonconstant_returns() {
    return fincore::make_series({9, -4, 9, 2}, "returns", {-0.02, 0.0, 0.01, 0.03});
}

double finite_linear_percentile(std::vector<double> values, const double percentile) {
    EXPECT_FALSE(values.empty());
    for (const double value : values) {
        EXPECT_TRUE(std::isfinite(value));
    }
    std::sort(values.begin(), values.end());
    const double position = percentile * static_cast<double>(values.size() - 1U);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = std::min(lower + 1U, values.size() - 1U);
    const double weight = position - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

TEST(BootstrapConfidenceInterval, PublishesExactSignatureAndPermitsDefaultArguments) {
    const auto constant = fincore::make_series({2, -3}, "returns", {0.01, 0.01});
    const auto defaults = fincore::bootstrap_confidence_interval(constant);

    EXPECT_DOUBLE_EQ(defaults.lower, 0.01);
    EXPECT_DOUBLE_EQ(defaults.upper, 0.01);

    const auto retained = [] {
        const auto local = fincore::make_series({5, 4}, "returns", {0.01, 0.01});
        return fincore::bootstrap_confidence_interval(local, 4U, 0.2,
                                                       fincore::bootstrap_statistic::mean,
                                                       42U);
    }();
    EXPECT_DOUBLE_EQ(retained.lower, 0.01);
    EXPECT_DOUBLE_EQ(retained.upper, 0.01);
}

TEST(BootstrapConfidenceInterval, MatchesDeterministicConstantAndNanDropSourceCases) {
    const auto constant = fincore::make_series({2, -3}, "returns", {0.01, 0.01});
    const auto mean = fincore::bootstrap_confidence_interval(
        constant, 4U, 0.05, fincore::bootstrap_statistic::mean, 42U);
    EXPECT_DOUBLE_EQ(mean.lower, 0.01);
    EXPECT_DOUBLE_EQ(mean.upper, 0.01);

    const auto standard_deviation = fincore::bootstrap_confidence_interval(
        constant, 4U, 0.05, fincore::bootstrap_statistic::sample_standard_deviation, 42U);
    EXPECT_DOUBLE_EQ(standard_deviation.lower, 0.0);
    EXPECT_DOUBLE_EQ(standard_deviation.upper, 0.0);

    const auto nan_drop = fincore::make_series(
        {7, 8, 9}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN(), 0.01});
    const auto nan_mean = fincore::bootstrap_confidence_interval(
        nan_drop, 3U, 0.05, fincore::bootstrap_statistic::mean, 42U);
    EXPECT_DOUBLE_EQ(nan_mean.lower, 0.01);
    EXPECT_DOUBLE_EQ(nan_mean.upper, 0.01);
}

TEST(BootstrapConfidenceInterval, UsesFiniteLinearEndpointsOfThePublicDistribution) {
    const auto returns = nonconstant_returns();
    constexpr std::size_t resample_count = 7U;
    constexpr double alpha = 0.4;
    constexpr auto statistic = fincore::bootstrap_statistic::median;
    constexpr std::uint64_t seed = 42U;

    const auto distribution = fincore::bootstrap_distribution(
        returns, resample_count, statistic, seed);
    const auto values = distribution.get_column<double>(fincore::bootstrap_statistic_column.data());
    const auto actual = fincore::bootstrap_confidence_interval(
        returns, resample_count, alpha, statistic, seed);

    EXPECT_NEAR(actual.lower, finite_linear_percentile(values, alpha / 2.0), tolerance);
    EXPECT_NEAR(actual.upper,
                finite_linear_percentile(values, 1.0 - alpha / 2.0), tolerance);
}

TEST(BootstrapConfidenceInterval, RejectsInvalidAlphaBeforeDistributionWork) {
    const auto returns = nonconstant_returns();
    for (const double alpha : {0.0, 1.0, std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
        EXPECT_THROW(static_cast<void>(fincore::bootstrap_confidence_interval(
                         returns, 4U, alpha, fincore::bootstrap_statistic::mean, 42U)),
                     fincore::input_error);
    }

    // The frame is invalid as well, so the caught wording is the ordering proof:
    // only validation that ran before the distribution work can name alpha.
    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{1, 2});
    try {
        static_cast<void>(fincore::bootstrap_confidence_interval(
            missing, 4U, 0.0, fincore::bootstrap_statistic::mean, 42U));
        FAIL() << "invalid alpha must throw before validating the distribution input";
    } catch (const fincore::input_error& error) {
        EXPECT_NE(std::string_view{error.what()}.find("alpha"), std::string_view::npos);
    }
}

TEST(BootstrapConfidenceInterval, RejectsEmptyAndNonfiniteGeneratedDistributions) {
    // The frames below are all valid; every rejection concerns the generated
    // distribution: empty for zero resamples, infinite for a constant book's
    // Sharpe, NaN for a singleton's deviation.
    const auto constant = fincore::make_series({1, 2}, "returns", {0.01, 0.01});
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_confidence_interval(
                     constant, 0U, 0.05, fincore::bootstrap_statistic::mean, 42U)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_confidence_interval(
                     constant, 4U, 0.05, fincore::bootstrap_statistic::sharpe_ratio, 42U)),
                 fincore::input_error);

    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_confidence_interval(
                     singleton, 4U, 0.05,
                     fincore::bootstrap_statistic::sample_standard_deviation, 42U)),
                 fincore::input_error);
}

TEST(BootstrapConfidenceInterval, PropagatesDistributionValidationFailures) {
    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{1, 2});
    EXPECT_THROW(fincore::bootstrap_confidence_interval(missing), fincore::input_error);

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1, 2});
    wrong_type.load_column<int>("returns", {1, 2}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::bootstrap_confidence_interval(wrong_type), fincore::input_error);

    const auto invalid_value = fincore::make_series(
        {1, 2}, "returns", {0.01, std::numeric_limits<double>::infinity()});
    EXPECT_THROW(fincore::bootstrap_confidence_interval(invalid_value), fincore::input_error);
    EXPECT_THROW(fincore::bootstrap_confidence_interval(
                     nonconstant_returns(), 1U, 0.05,
                     static_cast<fincore::bootstrap_statistic>(99), 42U),
                 fincore::input_error);
    EXPECT_THROW(fincore::bootstrap_confidence_interval(
                     nonconstant_returns(), std::numeric_limits<std::size_t>::max(), 0.05,
                     fincore::bootstrap_statistic::mean, 42U),
                 fincore::input_error);

    constexpr std::array<char, 12U> nul_name{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l'};
    const std::string_view embedded_nul{nul_name.data(), nul_name.size()};
    EXPECT_THROW(fincore::bootstrap_confidence_interval(
                     nonconstant_returns(), 1U, 0.05,
                     fincore::bootstrap_statistic::mean, 42U, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::bootstrap_confidence_interval(
                     nonconstant_returns(), 1U, 0.05,
                     fincore::bootstrap_statistic::mean, 42U, embedded_nul),
                 fincore::input_error);
}

TEST(BootstrapConfidenceInterval, IgnoresRawInputIndexAndRepeatsNativeSeed) {
    const auto ordered = fincore::make_series({1, 2, 3, 4}, "returns",
                                               {-0.02, 0.0, 0.01, 0.03});
    const auto irregular = fincore::make_series({88, -4, 88, 0}, "returns",
                                                 {-0.02, 0.0, 0.01, 0.03});
    const auto expected = fincore::bootstrap_confidence_interval(
        ordered, 17U, 0.2, fincore::bootstrap_statistic::mean, 91U);
    const auto actual = fincore::bootstrap_confidence_interval(
        irregular, 17U, 0.2, fincore::bootstrap_statistic::mean, 91U);
    const auto repeated = fincore::bootstrap_confidence_interval(
        ordered, 17U, 0.2, fincore::bootstrap_statistic::mean, 91U);

    EXPECT_DOUBLE_EQ(actual.lower, expected.lower);
    EXPECT_DOUBLE_EQ(actual.upper, expected.upper);
    EXPECT_DOUBLE_EQ(repeated.lower, expected.lower);
    EXPECT_DOUBLE_EQ(repeated.upper, expected.upper);
    // Native seeding is repeatable; the API intentionally makes no NumPy stream claim.
}

}  // namespace
