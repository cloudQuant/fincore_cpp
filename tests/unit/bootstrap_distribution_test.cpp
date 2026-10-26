#include <fincore/simulation/bootstrap.hpp>

#include "../fixtures/bootstrap_fixed_indices.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace {

using bootstrap_function = fincore::dataframe (*) (
    const fincore::dataframe&, std::size_t, fincore::bootstrap_statistic,
    std::optional<std::uint64_t>, std::string_view);
using bootstrap_draw_function = fincore::dataframe (*) (
    const fincore::dataframe&,
    std::span<const std::vector<std::size_t>>,
    fincore::bootstrap_statistic,
    std::string_view);

static_assert(std::is_same_v<
              decltype(static_cast<bootstrap_function>(
                  &fincore::bootstrap_distribution)),
              bootstrap_function>);
static_assert(std::is_same_v<
              decltype(static_cast<bootstrap_draw_function>(
                  &fincore::bootstrap_distribution)),
              bootstrap_draw_function>);
static_assert(std::is_same_v<decltype(fincore::bootstrap_statistic_column),
                             const std::string_view>);

constexpr double tolerance = 1.0e-12;

void expect_matches_python_fixture(
    const double actual,
    const bootstrap_fixed_indices_fixture::expected_number& expected) {
    using bootstrap_fixed_indices_fixture::expected_class;
    switch (expected.value_class) {
    case expected_class::nan:
        EXPECT_TRUE(std::isnan(actual));
        return;
    case expected_class::positive_infinity:
        EXPECT_EQ(actual, std::numeric_limits<double>::infinity());
        return;
    case expected_class::negative_infinity:
        EXPECT_EQ(actual, -std::numeric_limits<double>::infinity());
        return;
    case expected_class::finite:
        ASSERT_TRUE(std::isfinite(actual));
        EXPECT_LE(std::abs(actual - expected.value),
                  bootstrap_fixed_indices_fixture::absolute_tolerance +
                      bootstrap_fixed_indices_fixture::relative_tolerance *
                          std::abs(expected.value));
        return;
    }
    FAIL() << "unknown fixture value class";
}

void expect_schema(const fincore::dataframe& actual, const std::size_t row_count) {
    std::vector<fincore::timestamp> expected_index;
    expected_index.reserve(row_count);
    for (std::size_t row = 0U; row < row_count; ++row) {
        expected_index.push_back(static_cast<fincore::timestamp>(row));
    }
    EXPECT_EQ(actual.get_index(), expected_index);
    EXPECT_EQ(actual.shape().first, row_count);
    EXPECT_EQ(actual.shape().second, 1U);
    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_EQ(std::get<0>(columns[0]), fincore::bootstrap_statistic_column.data());
    EXPECT_EQ(std::get<1>(columns[0]), row_count);
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));
}

const std::vector<double>& bootstrap_values(const fincore::dataframe& result) {
    return result.get_column<double>(fincore::bootstrap_statistic_column.data());
}

fincore::dataframe nonconstant_returns() {
    return fincore::make_series({9, -4, 9, 2}, "returns", {-0.02, 0.0, 0.01, 0.03});
}

TEST(BootstrapDistribution, PublishesExactTypedOrdinalSchema) {
    const auto actual = fincore::bootstrap_distribution(
        nonconstant_returns(), 5U, fincore::bootstrap_statistic::mean, 42U);

    expect_schema(actual, 5U);
}

TEST(BootstrapDistribution, MatchesDeterministicConstantAndNaNDropSourceCases) {
    const auto constant = fincore::make_series({2, -3, 2}, "returns",
                                                {0.01, 0.01, 0.01});
    for (const auto statistic : {fincore::bootstrap_statistic::mean,
                                 fincore::bootstrap_statistic::median,
                                 fincore::bootstrap_statistic::minimum,
                                 fincore::bootstrap_statistic::maximum}) {
        const auto result = fincore::bootstrap_distribution(constant, 4U, statistic, 42U);
        expect_schema(result, 4U);
        for (const double value : bootstrap_values(result)) {
            EXPECT_DOUBLE_EQ(value, 0.01);
        }
    }

    const auto standard_deviation = fincore::bootstrap_distribution(
        constant, 4U, fincore::bootstrap_statistic::sample_standard_deviation, 42U);
    for (const double value : bootstrap_values(standard_deviation)) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }

    const auto nan_drop = fincore::make_series(
        {7, 8, 9}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN(), 0.01});
    const auto result = fincore::bootstrap_distribution(
        nan_drop, 3U, fincore::bootstrap_statistic::mean, 42U);
    EXPECT_EQ(bootstrap_values(result), (std::vector<double>{0.01, 0.01, 0.01}));
}

TEST(BootstrapDistribution, PreservesDegenerateSingletonAndZeroCountSemantics) {
    const auto singleton = fincore::make_series({999}, "returns", {0.01});
    for (const auto statistic : {fincore::bootstrap_statistic::sample_standard_deviation,
                                 fincore::bootstrap_statistic::sharpe_ratio,
                                 fincore::bootstrap_statistic::sortino_ratio}) {
        const auto result = fincore::bootstrap_distribution(singleton, 3U, statistic, 42U);
        expect_schema(result, 3U);
        for (const double value : bootstrap_values(result)) {
            EXPECT_TRUE(std::isnan(value));
        }
    }

    const auto zero_mean_constant =
        fincore::make_series({1, 2}, "returns", {0.0, 0.0});
    const auto nonzero_mean_constant =
        fincore::make_series({1, 2}, "returns", {0.01, 0.01});
    for (const auto statistic : {fincore::bootstrap_statistic::sharpe_ratio,
                                 fincore::bootstrap_statistic::sortino_ratio}) {
        const auto zero_mean = fincore::bootstrap_distribution(
            zero_mean_constant, 3U, statistic, 42U);
        for (const double value : bootstrap_values(zero_mean)) {
            EXPECT_TRUE(std::isnan(value));
        }

        const auto nonzero_mean = fincore::bootstrap_distribution(
            nonzero_mean_constant, 3U, statistic, 42U);
        for (const double value : bootstrap_values(nonzero_mean)) {
            EXPECT_EQ(value, std::numeric_limits<double>::infinity());
        }
    }

    const auto zero = fincore::bootstrap_distribution(
        singleton, 0U, fincore::bootstrap_statistic::mean, 42U);
    expect_schema(zero, 0U);
    EXPECT_TRUE(bootstrap_values(zero).empty());
}

TEST(BootstrapDistribution, ComputesEveryStatisticForAnEmpiricallyKnownMixedSample) {
    // Empirically, the native engine with seed 2 draws {-0.01, 0.03} from this
    // two-value input for its first resample. This is native-seed evidence only,
    // not a claim of NumPy stream parity.
    const auto values = fincore::make_series({7, -9}, "returns", {-0.01, 0.03});
    const auto mean = fincore::bootstrap_distribution(
        values, 1U, fincore::bootstrap_statistic::mean, 2U);
    const auto standard_deviation = fincore::bootstrap_distribution(
        values, 1U, fincore::bootstrap_statistic::sample_standard_deviation, 2U);
    const auto median = fincore::bootstrap_distribution(
        values, 1U, fincore::bootstrap_statistic::median, 2U);
    const auto sharpe = fincore::bootstrap_distribution(
        values, 1U, fincore::bootstrap_statistic::sharpe_ratio, 2U);
    const auto sortino = fincore::bootstrap_distribution(
        values, 1U, fincore::bootstrap_statistic::sortino_ratio, 2U);
    const auto minimum = fincore::bootstrap_distribution(
        values, 1U, fincore::bootstrap_statistic::minimum, 2U);
    const auto maximum = fincore::bootstrap_distribution(
        values, 1U, fincore::bootstrap_statistic::maximum, 2U);

    constexpr double expected_mean = 0.01;
    const double expected_sample_standard_deviation = std::sqrt(0.0008);
    constexpr double expected_median = 0.01;
    const double expected_sharpe =
        expected_mean / expected_sample_standard_deviation * std::sqrt(252.0);
    // The strictly positive +0.03 becomes zero, so the downside sample is
    // {-0.01, 0.0}; its ddof=1 variance is 0.00005.
    const double expected_sortino =
        expected_mean / std::sqrt(0.00005) * std::sqrt(252.0);

    ASSERT_EQ(bootstrap_values(mean).size(), 1U);
    ASSERT_EQ(bootstrap_values(standard_deviation).size(), 1U);
    ASSERT_EQ(bootstrap_values(median).size(), 1U);
    ASSERT_EQ(bootstrap_values(sharpe).size(), 1U);
    ASSERT_EQ(bootstrap_values(sortino).size(), 1U);
    ASSERT_EQ(bootstrap_values(minimum).size(), 1U);
    ASSERT_EQ(bootstrap_values(maximum).size(), 1U);
    EXPECT_NEAR(bootstrap_values(mean)[0], expected_mean, tolerance);
    EXPECT_NEAR(bootstrap_values(standard_deviation)[0],
                expected_sample_standard_deviation, tolerance);
    EXPECT_NE(bootstrap_values(standard_deviation)[0], 0.02);
    EXPECT_NEAR(bootstrap_values(median)[0], expected_median, tolerance);
    EXPECT_NEAR(bootstrap_values(sharpe)[0], expected_sharpe, tolerance);
    EXPECT_NEAR(bootstrap_values(sortino)[0], expected_sortino, tolerance);
    EXPECT_NEAR(bootstrap_values(minimum)[0], -0.01, tolerance);
    EXPECT_NEAR(bootstrap_values(maximum)[0], 0.03, tolerance);
}

TEST(BootstrapDistribution, RepeatsNativeSeedButDoesNotRequireNumPyParity) {
    const auto first = fincore::bootstrap_distribution(
        nonconstant_returns(), 17U, fincore::bootstrap_statistic::mean, 91U);
    const auto repeated = fincore::bootstrap_distribution(
        nonconstant_returns(), 17U, fincore::bootstrap_statistic::mean, 91U);
    const auto distinct = fincore::bootstrap_distribution(
        nonconstant_returns(), 17U, fincore::bootstrap_statistic::mean, 92U);

    EXPECT_EQ(bootstrap_values(first), bootstrap_values(repeated));
    EXPECT_NE(bootstrap_values(first), bootstrap_values(distinct));
}

TEST(BootstrapDistribution, UsesExplicitDrawsOverNaNCleanedPositions) {
    const auto returns = fincore::make_series(
        {10, 20, 30, 40}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.0, -0.02});
    // Positions address the NaN-cleaned population {0.01, 0.0, -0.02}, not the
    // frame rows, so each draw has length 3 and index 2 reads the fourth
    // timestamp's return.
    const std::vector<std::vector<std::size_t>> draws{
        {0U, 0U, 0U}, {2U, 2U, 2U}, {0U, 1U, 2U},
        {1U, 1U, 1U}, {2U, 0U, 2U}};

    const auto input = fincore::bootstrap_distribution(
        returns, std::span<const std::vector<std::size_t>>{draws},
        fincore::bootstrap_statistic::mean);
    expect_schema(input, draws.size());
    const auto& means = bootstrap_values(input);
    ASSERT_EQ(means.size(), draws.size());
    EXPECT_DOUBLE_EQ(means[0], 0.01);
    EXPECT_DOUBLE_EQ(means[1], -0.02);
    EXPECT_NEAR(means[2], -1.0 / 300.0, tolerance);
    EXPECT_DOUBLE_EQ(means[3], 0.0);
    EXPECT_DOUBLE_EQ(means[4], -0.01);

    const auto standard_deviations = fincore::bootstrap_distribution(
        returns, std::span<const std::vector<std::size_t>>{draws},
        fincore::bootstrap_statistic::sample_standard_deviation);
    const auto& deviations = bootstrap_values(standard_deviations);
    ASSERT_EQ(deviations.size(), draws.size());
    EXPECT_DOUBLE_EQ(deviations[0], 0.0);
    EXPECT_DOUBLE_EQ(deviations[1], 0.0);
    EXPECT_NEAR(deviations[2], std::sqrt(0.00023333333333333334), tolerance);
    EXPECT_DOUBLE_EQ(deviations[3], 0.0);
    EXPECT_NEAR(deviations[4], std::sqrt(0.0003), tolerance);
}

TEST(BootstrapDistribution, ExplicitDrawsPreserveNoDrawAndValidateShapeIndicesAndSelector) {
    const auto returns = nonconstant_returns();
    const std::vector<std::vector<std::size_t>> no_draws;
    const auto empty = fincore::bootstrap_distribution(
        returns, std::span<const std::vector<std::size_t>>{no_draws},
        fincore::bootstrap_statistic::mean);
    expect_schema(empty, 0U);
    EXPECT_TRUE(bootstrap_values(empty).empty());

    const std::vector<std::vector<std::size_t>> short_draw{{0U, 1U}};
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_distribution(
                     returns, std::span<const std::vector<std::size_t>>{short_draw},
                     fincore::bootstrap_statistic::mean)),
                 fincore::input_error);
    const std::vector<std::vector<std::size_t>> out_of_range{{0U, 1U, 2U, 4U}};
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_distribution(
                     returns, std::span<const std::vector<std::size_t>>{out_of_range},
                     fincore::bootstrap_statistic::mean)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_distribution(
                     returns, std::span<const std::vector<std::size_t>>{no_draws},
                     static_cast<fincore::bootstrap_statistic>(99))),
                 fincore::input_error);

    const auto empty_input = fincore::make_series({}, "returns", {});
    EXPECT_THROW(static_cast<void>(fincore::bootstrap_distribution(
                     empty_input, std::span<const std::vector<std::size_t>>{no_draws})),
                 fincore::input_error);
}

TEST(BootstrapDistribution, ExplicitDrawsMatchPinnedPythonStatisticsAndClasses) {
    using namespace bootstrap_fixed_indices_fixture;
    const auto returns =
        fincore::make_series({10, 20, 30, 40}, "returns", simulation_returns);
    const std::array<fincore::bootstrap_statistic, 7> statistics{
        fincore::bootstrap_statistic::mean,
        fincore::bootstrap_statistic::sample_standard_deviation,
        fincore::bootstrap_statistic::median,
        fincore::bootstrap_statistic::sharpe_ratio,
        fincore::bootstrap_statistic::sortino_ratio,
        fincore::bootstrap_statistic::minimum,
        fincore::bootstrap_statistic::maximum,
    };

    for (std::size_t statistic = 0; statistic < statistics.size(); ++statistic) {
        const auto actual = fincore::bootstrap_distribution(
            returns, std::span<const std::vector<std::size_t>>{simulation_draws},
            statistics[statistic]);
        expect_schema(actual, simulation_draws.size());
        const auto& values = bootstrap_values(actual);
        ASSERT_EQ(values.size(), simulation_draws.size());
        ASSERT_EQ(values.size(), simulation_samples[statistic].size());
        for (std::size_t draw = 0; draw < values.size(); ++draw) {
            expect_matches_python_fixture(values[draw],
                                          simulation_samples[statistic][draw]);
        }
    }
}

TEST(BootstrapDistribution, ProducesFiniteUnseededMeanOutput) {
    const auto result = fincore::bootstrap_distribution(
        nonconstant_returns(), 7U, fincore::bootstrap_statistic::mean);
    expect_schema(result, 7U);
    for (const double value : bootstrap_values(result)) {
        EXPECT_TRUE(std::isfinite(value));
    }
}

TEST(BootstrapDistribution, IgnoresRawInputIndexAndOwnsProducedValues) {
    const auto ordered = fincore::make_series({1, 2, 3, 4}, "returns",
                                               {-0.02, 0.0, 0.01, 0.03});
    const auto irregular = fincore::make_series({88, -4, 88, 0}, "returns",
                                                 {-0.02, 0.0, 0.01, 0.03});
    const auto expected = fincore::bootstrap_distribution(
        ordered, 9U, fincore::bootstrap_statistic::mean, 3U);
    const auto actual = fincore::bootstrap_distribution(
        irregular, 9U, fincore::bootstrap_statistic::mean, 3U);
    EXPECT_EQ(bootstrap_values(actual), bootstrap_values(expected));

    const auto retained = [] {
        auto local = fincore::make_series({5, 4, 3}, "returns", {0.01, 0.01, 0.01});
        return fincore::bootstrap_distribution(local, 4U,
                                               fincore::bootstrap_statistic::mean, 42U);
    }();
    EXPECT_EQ(bootstrap_values(retained), (std::vector<double>{0.01, 0.01, 0.01, 0.01}));
}

TEST(BootstrapDistribution, RejectsInvalidColumnsValuesStatisticsAndOutputBounds) {
    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{1, 2});
    EXPECT_THROW(fincore::bootstrap_distribution(missing), fincore::input_error);

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1, 2});
    wrong_type.load_column<int>("returns", {1, 2}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::bootstrap_distribution(wrong_type), fincore::input_error);

    fincore::dataframe short_column;
    short_column.load_index(std::vector<fincore::timestamp>{1, 2});
    short_column.load_column<double>("returns", {0.01},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::bootstrap_distribution(short_column), fincore::input_error);

    const auto empty = fincore::make_series({}, "returns", {});
    EXPECT_THROW(fincore::bootstrap_distribution(empty), fincore::input_error);
    const auto all_nan = fincore::make_series(
        {1, 2}, "returns", {std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(fincore::bootstrap_distribution(all_nan), fincore::input_error);
    for (const double infinite : {std::numeric_limits<double>::infinity(),
                                  -std::numeric_limits<double>::infinity()}) {
        const auto invalid = fincore::make_series({1, 2}, "returns", {0.01, infinite});
        EXPECT_THROW(fincore::bootstrap_distribution(invalid), fincore::input_error);
    }

    constexpr std::array<char, 12U> nul_name{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l'};
    const std::string_view embedded_nul{nul_name.data(), nul_name.size()};
    EXPECT_THROW(fincore::bootstrap_distribution(nonconstant_returns(), 1U,
                                                 fincore::bootstrap_statistic::mean,
                                                 1U, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::bootstrap_distribution(nonconstant_returns(), 1U,
                                                 fincore::bootstrap_statistic::mean,
                                                 1U, embedded_nul),
                 fincore::input_error);
    EXPECT_THROW(fincore::bootstrap_distribution(
                     nonconstant_returns(), 1U,
                     static_cast<fincore::bootstrap_statistic>(99), 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::bootstrap_distribution(
                     nonconstant_returns(), std::numeric_limits<std::size_t>::max(),
                     fincore::bootstrap_statistic::mean, 1U),
                 fincore::input_error);

    const double maximum = std::numeric_limits<double>::max();
    const auto arithmetic_overflow =
        fincore::make_series({1, 2}, "returns", {maximum, -maximum});
    // Only a draw mixing the two extremes overflows the running variance, so any
    // seed that does is enough: a resample of one repeated value stays finite.
    bool rejected_overflowing_resample = false;
    for (std::uint64_t seed = 1U; seed <= 64U; ++seed) {
        try {
            static_cast<void>(fincore::bootstrap_distribution(
                arithmetic_overflow, 1U,
                fincore::bootstrap_statistic::sample_standard_deviation, seed));
        } catch (const fincore::input_error&) {
            rejected_overflowing_resample = true;
            break;
        }
    }
    EXPECT_TRUE(rejected_overflowing_resample);
}

}  // namespace
