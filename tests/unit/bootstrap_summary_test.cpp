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
#include <utility>
#include <vector>

namespace {

using bootstrap_summary_function = fincore::bootstrap_summary_result (*)(
    const fincore::dataframe &, std::size_t, double,
    std::optional<std::uint64_t>, std::string_view);

static_assert(std::is_same_v<decltype(&fincore::bootstrap_summary),
                             bootstrap_summary_function>);
static_assert(std::is_same_v<decltype(fincore::bootstrap_summary(
                                 std::declval<const fincore::dataframe &>())),
                             fincore::bootstrap_summary_result>);
static_assert(std::is_same_v<
              decltype(fincore::bootstrap_summary_statistic::value), double>);
static_assert(std::is_same_v<
              decltype(fincore::bootstrap_summary_statistic::standard_error),
              double>);
static_assert(
    std::is_same_v<
        decltype(fincore::bootstrap_summary_statistic::confidence_interval),
        fincore::bootstrap_confidence_interval_result>);
static_assert(std::is_same_v<decltype(fincore::bootstrap_summary_result::mean),
                             fincore::bootstrap_summary_statistic>);
static_assert(
    std::is_same_v<
        decltype(fincore::bootstrap_summary_result::sample_standard_deviation),
        fincore::bootstrap_summary_statistic>);
static_assert(
    std::is_same_v<decltype(fincore::bootstrap_summary_result::sharpe_ratio),
                   fincore::bootstrap_summary_statistic>);
static_assert(
    std::is_same_v<decltype(fincore::bootstrap_summary_result::sortino_ratio),
                   fincore::bootstrap_summary_statistic>);

constexpr double tolerance = 1.0e-12;

fincore::dataframe finite_ratio_returns() {
    return fincore::make_series({9, -4, 9, 2}, "returns",
                                {-0.04, -0.03, -0.02, -0.01});
}

fincore::dataframe mixed_sign_ratio_returns() {
    return fincore::make_series({1, 2, 3, 4, 5, 6}, "returns",
                                {-0.04, -0.03, -0.02, -0.01, -0.005, 0.02});
}

double arithmetic_mean(const std::vector<double> &values) {
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

double sample_standard_deviation(const std::vector<double> &values) {
    EXPECT_GE(values.size(), 2U);
    const double mean = arithmetic_mean(values);
    double sum_of_squares = 0.0;
    for (const double value : values) {
        const double difference = value - mean;
        sum_of_squares += difference * difference;
    }
    return std::sqrt(sum_of_squares / static_cast<double>(values.size() - 1U));
}

double linear_percentile(std::vector<double> values, const double percentile) {
    EXPECT_FALSE(values.empty());
    std::sort(values.begin(), values.end());
    const double position =
        percentile * static_cast<double>(values.size() - 1U);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = std::min(lower + 1U, values.size() - 1U);
    const double weight = position - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

std::vector<double> public_distribution_values(
    const fincore::dataframe &returns, const std::size_t resample_count,
    const fincore::bootstrap_statistic statistic, const std::uint64_t seed,
    const std::string_view column = fincore::returns_column) {
    const auto distribution = fincore::bootstrap_distribution(
        returns, resample_count, statistic, seed, column);
    const auto values = distribution.get_column<double>(
        fincore::bootstrap_statistic_column.data());
    return {values.begin(), values.end()};
}

fincore::bootstrap_summary_statistic expected_from_public_distribution(
    const std::vector<double> &source_values, const fincore::dataframe &returns,
    const std::size_t resample_count, const double alpha,
    const fincore::bootstrap_statistic statistic, const std::uint64_t seed,
    const std::string_view column = fincore::returns_column) {
    const auto distribution_values = public_distribution_values(
        returns, resample_count, statistic, seed, column);
    for (const double value : distribution_values) {
        EXPECT_TRUE(std::isfinite(value));
    }

    const double source_mean = arithmetic_mean(source_values);
    const double source_standard_deviation =
        sample_standard_deviation(source_values);
    double observed = source_mean;
    if (statistic == fincore::bootstrap_statistic::sample_standard_deviation) {
        observed = source_standard_deviation;
    } else if (statistic == fincore::bootstrap_statistic::sharpe_ratio) {
        observed = source_mean / source_standard_deviation * std::sqrt(252.0);
    } else if (statistic == fincore::bootstrap_statistic::sortino_ratio) {
        auto downside_values = source_values;
        for (double &value : downside_values) {
            if (value > 0.0) {
                value = 0.0;
            }
        }
        observed = source_mean / sample_standard_deviation(downside_values) *
                   std::sqrt(252.0);
    }

    return {observed,
            sample_standard_deviation(distribution_values),
            {linear_percentile(distribution_values, alpha / 2.0),
             linear_percentile(distribution_values, 1.0 - alpha / 2.0)}};
}

void expect_statistic_near(
    const fincore::bootstrap_summary_statistic &actual,
    const fincore::bootstrap_summary_statistic &expected) {
    EXPECT_NEAR(actual.value, expected.value, tolerance);
    EXPECT_NEAR(actual.standard_error, expected.standard_error, tolerance);
    EXPECT_NEAR(actual.confidence_interval.lower,
                expected.confidence_interval.lower, tolerance);
    EXPECT_NEAR(actual.confidence_interval.upper,
                expected.confidence_interval.upper, tolerance);
}

void expect_summary_near(const fincore::bootstrap_summary_result &actual,
                         const fincore::bootstrap_summary_result &expected) {
    expect_statistic_near(actual.mean, expected.mean);
    expect_statistic_near(actual.sample_standard_deviation,
                          expected.sample_standard_deviation);
    expect_statistic_near(actual.sharpe_ratio, expected.sharpe_ratio);
    expect_statistic_near(actual.sortino_ratio, expected.sortino_ratio);
}

TEST(BootstrapSummary, PublishesExactContractAndDefaultCallExpression) {
    SUCCEED();
}

TEST(BootstrapSummary,
     MatchesEachPublicDistributionUsingIndependentPublishedFormulas) {
    const auto returns = finite_ratio_returns();
    constexpr std::size_t resample_count = 17U;
    constexpr double alpha = 0.2;
    constexpr std::uint64_t seed = 1U;
    const auto actual =
        fincore::bootstrap_summary(returns, resample_count, alpha, seed);
    const std::vector<double> source_values{-0.04, -0.03, -0.02, -0.01};

    // With no positive return the Sortino's clipped deviation set equals the
    // Sharpe's, so this loop cannot separate the two ratios; the mixed-sign case
    // in the next test is what does.
    const std::array statistics{
        fincore::bootstrap_statistic::mean,
        fincore::bootstrap_statistic::sample_standard_deviation,
        fincore::bootstrap_statistic::sharpe_ratio,
        fincore::bootstrap_statistic::sortino_ratio,
    };
    const std::array actual_statistics{
        &actual.mean,
        &actual.sample_standard_deviation,
        &actual.sharpe_ratio,
        &actual.sortino_ratio,
    };
    for (std::size_t index = 0U; index < statistics.size(); ++index) {
        const auto expected = expected_from_public_distribution(
            source_values, returns, resample_count, alpha, statistics[index],
            seed);
        expect_statistic_near(*actual_statistics[index], expected);
    }
}

TEST(BootstrapSummary,
     DistinguishesSharpeAndSortinoWithOwnPublicDistributionParity) {
    const auto returns = mixed_sign_ratio_returns();
    constexpr std::size_t resample_count = 17U;
    constexpr double alpha = 0.2;
    constexpr std::uint64_t seed = 1U;
    const std::vector<double> source_values{-0.04, -0.03,  -0.02,
                                            -0.01, -0.005, 0.02};
    const auto actual =
        fincore::bootstrap_summary(returns, resample_count, alpha, seed);

    for (const auto statistic :
         {fincore::bootstrap_statistic::mean,
          fincore::bootstrap_statistic::sample_standard_deviation,
          fincore::bootstrap_statistic::sharpe_ratio,
          fincore::bootstrap_statistic::sortino_ratio}) {
        const auto values = public_distribution_values(returns, resample_count,
                                                       statistic, seed);
        ASSERT_EQ(values.size(), resample_count);
        for (const double value : values) {
            EXPECT_TRUE(std::isfinite(value));
        }
    }

    const auto expected_sharpe = expected_from_public_distribution(
        source_values, returns, resample_count, alpha,
        fincore::bootstrap_statistic::sharpe_ratio, seed);
    const auto expected_sortino = expected_from_public_distribution(
        source_values, returns, resample_count, alpha,
        fincore::bootstrap_statistic::sortino_ratio, seed);
    expect_statistic_near(actual.sharpe_ratio, expected_sharpe);
    expect_statistic_near(actual.sortino_ratio, expected_sortino);
    EXPECT_GT(std::abs(actual.sharpe_ratio.value - actual.sortino_ratio.value),
              tolerance);
    EXPECT_GT(std::abs(actual.sharpe_ratio.standard_error -
                       actual.sortino_ratio.standard_error),
              tolerance);
}

TEST(BootstrapSummary, DropsNanForObservedValuesAndEveryPublicDistribution) {
    const auto clean = finite_ratio_returns();
    const auto nan_padded = fincore::make_series(
        {7, 8, 9, 10, 11}, "returns",
        {-0.04, std::numeric_limits<double>::quiet_NaN(), -0.03, -0.02, -0.01});
    const auto expected = fincore::bootstrap_summary(clean, 17U, 0.2, 1U);
    const auto actual = fincore::bootstrap_summary(nan_padded, 17U, 0.2, 1U);

    expect_summary_near(actual, expected);
    EXPECT_NEAR(actual.mean.value, -0.025, tolerance);
}

TEST(BootstrapSummary, ValidatesAlphaThenCountBeforeInputOrDistributionWork) {
    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{1, 2});

    try {
        static_cast<void>(fincore::bootstrap_summary(missing, 0U, 0.0, 1U));
        FAIL() << "invalid alpha must precede count and input validation";
    } catch (const fincore::input_error &error) {
        EXPECT_NE(std::string_view{error.what()}.find("alpha"),
                  std::string_view::npos);
    }

    for (const double alpha :
         {0.0, 1.0, std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()}) {
        EXPECT_THROW(static_cast<void>(fincore::bootstrap_summary(
                         finite_ratio_returns(), 2U, alpha, 1U)),
                     fincore::input_error);
    }

    // One resample would still give a valid distribution, but no standard error,
    // so the summary's own floor is two and the wording names resamples.
    for (const std::size_t count : {0U, 1U}) {
        try {
            static_cast<void>(
                fincore::bootstrap_summary(missing, count, 0.2, 1U));
            FAIL() << "invalid count must precede input validation";
        } catch (const fincore::input_error &error) {
            EXPECT_NE(std::string_view{error.what()}.find("resamples"),
                      std::string_view::npos);
        }
    }
}

TEST(BootstrapSummary, RetainsConfidenceIntervalFiniteDistributionDiagnostics) {
    const auto finite = finite_ratio_returns();
    try {
        static_cast<void>(fincore::bootstrap_confidence_interval(
            finite, 0U, 0.2, fincore::bootstrap_statistic::mean, 1U));
        FAIL() << "the Phase 21 empty-distribution diagnostic must be retained";
    } catch (const fincore::input_error &error) {
        EXPECT_STREQ(
            error.what(),
            "bootstrap confidence interval requires at least one resample");
    }

    const auto positive_constant =
        fincore::make_series({1, 2}, "returns", {0.01, 0.01});
    try {
        static_cast<void>(fincore::bootstrap_confidence_interval(
            positive_constant, 2U, 0.2,
            fincore::bootstrap_statistic::sharpe_ratio, 1U));
        FAIL() << "the Phase 21 nonfinite-distribution diagnostic must be "
                  "retained";
    } catch (const fincore::input_error &error) {
        EXPECT_STREQ(
            error.what(),
            "bootstrap confidence interval requires finite statistics");
    }
}

TEST(BootstrapSummary, RejectsDegenerateAndNonfiniteSummaryBoundaries) {
    const auto positive_constant =
        fincore::make_series({1, 2}, "returns", {0.01, 0.01});
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_summary(positive_constant,
                                                              2U, 0.2, 1U)),
                 fincore::input_error);

    const auto singleton = fincore::make_series({1}, "returns", {-0.01});
    EXPECT_THROW(
        static_cast<void>(fincore::bootstrap_summary(singleton, 2U, 0.2, 1U)),
        fincore::input_error);

    // Seed 2 produces a degenerate ratio resample for this otherwise finite
    // source; the summary boundary rejects that nonfinite distribution value.
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_summary(
                     finite_ratio_returns(), 17U, 0.2, 2U)),
                 fincore::input_error);

    const auto all_nan =
        fincore::make_series({1, 2}, "returns",
                             {std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(
        static_cast<void>(fincore::bootstrap_summary(all_nan, 2U, 0.2, 1U)),
        fincore::input_error);
    for (const double infinite : {std::numeric_limits<double>::infinity(),
                                  -std::numeric_limits<double>::infinity()}) {
        const auto invalid =
            fincore::make_series({1, 2}, "returns", {-0.01, infinite});
        EXPECT_THROW(
            static_cast<void>(fincore::bootstrap_summary(invalid, 2U, 0.2, 1U)),
            fincore::input_error);
    }
}

TEST(BootstrapSummary, PropagatesSelectorTypeAndValueValidation) {
    fincore::dataframe selected;
    selected.load_index(std::vector<fincore::timestamp>{5, 4, 3, 2});
    selected.load_column<double>("strategy", {-0.04, -0.03, -0.02, -0.01},
                                 hmdf::nan_policy::dont_pad_with_nans);
    const auto expected =
        fincore::bootstrap_summary(finite_ratio_returns(), 17U, 0.2, 1U);
    const auto actual =
        fincore::bootstrap_summary(selected, 17U, 0.2, 1U, "strategy");
    expect_summary_near(actual, expected);

    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{1, 2});
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_summary(missing, 2U, 0.2,
                                                              1U, "strategy")),
                 fincore::input_error);

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1, 2});
    wrong_type.load_column<int>("strategy", {1, 2},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_summary(
                     wrong_type, 2U, 0.2, 1U, "strategy")),
                 fincore::input_error);

    fincore::dataframe short_column;
    short_column.load_index(std::vector<fincore::timestamp>{1, 2});
    short_column.load_column<double>("strategy", {-0.01},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_summary(
                     short_column, 2U, 0.2, 1U, "strategy")),
                 fincore::input_error);

    constexpr std::array<char, 12U> nul_name{'s', 't', 'r',  'a', 't', 'e',
                                             'g', 'y', '\0', 'e', 'v', 'i'};
    const std::string_view embedded_nul{nul_name.data(), nul_name.size()};
    EXPECT_THROW(static_cast<void>(
                     fincore::bootstrap_summary(selected, 2U, 0.2, 1U, "")),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_summary(
                     selected, 2U, 0.2, 1U, embedded_nul)),
                 fincore::input_error);
}

TEST(BootstrapSummary, IgnoresRawIndexRepeatsNativeSeedAndOwnsResults) {
    const auto ordered = fincore::make_series({1, 2, 3, 4}, "returns",
                                              {-0.04, -0.03, -0.02, -0.01});
    const auto irregular = fincore::make_series({88, -4, 88, 0}, "returns",
                                                {-0.04, -0.03, -0.02, -0.01});
    const auto expected = fincore::bootstrap_summary(ordered, 17U, 0.2, 1U);
    const auto irregular_result =
        fincore::bootstrap_summary(irregular, 17U, 0.2, 1U);
    const auto repeated = fincore::bootstrap_summary(ordered, 17U, 0.2, 1U);
    expect_summary_near(irregular_result, expected);
    expect_summary_near(repeated, expected);
    // A native fixed seed is repeatable; this is deliberately not a NumPy-RNG
    // compatibility assertion.

    const auto owned = [] {
        std::array<char, 8U> selector{'s', 't', 'r', 'a', 't', 'e', 'g', 'y'};
        fincore::dataframe local;
        local.load_index(std::vector<fincore::timestamp>{5, 4, 3, 2});
        local.load_column<double>("strategy", {-0.04, -0.03, -0.02, -0.01},
                                  hmdf::nan_policy::dont_pad_with_nans);
        const auto result = fincore::bootstrap_summary(
            local, 17U, 0.2, 1U,
            std::string_view{selector.data(), selector.size()});
        local.get_column<double>("strategy")[0] = -0.99;
        selector[0] = 'x';
        return result;
    }();
    expect_summary_near(owned, expected);
}

} // namespace
