#include <fincore/simulation/bootstrap.hpp>

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

static_assert(std::is_same_v<decltype(&fincore::bootstrap_distribution),
                             bootstrap_function>);
static_assert(std::is_same_v<decltype(fincore::bootstrap_statistic_column),
                             const std::string_view>);

constexpr double tolerance = 1.0e-12;

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
    for (const auto statistic : {fincore::bootstrap_statistic::sharpe_ratio,
                                 fincore::bootstrap_statistic::sortino_ratio}) {
        const auto result = fincore::bootstrap_distribution(constant, 4U, statistic, 42U);
        for (const double value : bootstrap_values(result)) {
            EXPECT_EQ(value, std::numeric_limits<double>::infinity());
        }
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

    const auto zero = fincore::bootstrap_distribution(
        singleton, 0U, fincore::bootstrap_statistic::mean, 42U);
    expect_schema(zero, 0U);
    EXPECT_TRUE(bootstrap_values(zero).empty());
}

TEST(BootstrapDistribution, SupportsEveryStatisticWithRowWiseRelationships) {
    const auto values = nonconstant_returns();
    const auto mean = fincore::bootstrap_distribution(
        values, 11U, fincore::bootstrap_statistic::mean, 81U);
    const auto standard_deviation = fincore::bootstrap_distribution(
        values, 11U, fincore::bootstrap_statistic::sample_standard_deviation, 81U);
    const auto median = fincore::bootstrap_distribution(
        values, 11U, fincore::bootstrap_statistic::median, 81U);
    const auto sharpe = fincore::bootstrap_distribution(
        values, 11U, fincore::bootstrap_statistic::sharpe_ratio, 81U);
    const auto sortino = fincore::bootstrap_distribution(
        values, 11U, fincore::bootstrap_statistic::sortino_ratio, 81U);
    const auto minimum = fincore::bootstrap_distribution(
        values, 11U, fincore::bootstrap_statistic::minimum, 81U);
    const auto maximum = fincore::bootstrap_distribution(
        values, 11U, fincore::bootstrap_statistic::maximum, 81U);

    for (std::size_t row = 0U; row < 11U; ++row) {
        const double mean_value = bootstrap_values(mean)[row];
        const double deviation_value = bootstrap_values(standard_deviation)[row];
        const double median_value = bootstrap_values(median)[row];
        const double minimum_value = bootstrap_values(minimum)[row];
        const double maximum_value = bootstrap_values(maximum)[row];
        EXPECT_LE(minimum_value, median_value);
        EXPECT_LE(median_value, maximum_value);
        EXPECT_LE(minimum_value, mean_value);
        EXPECT_LE(mean_value, maximum_value);
        EXPECT_GE(deviation_value, 0.0);
        if (deviation_value == 0.0) {
            EXPECT_TRUE(std::isinf(bootstrap_values(sharpe)[row]) ||
                        std::isnan(bootstrap_values(sharpe)[row]));
        } else {
            EXPECT_NEAR(bootstrap_values(sharpe)[row],
                        mean_value / deviation_value * std::sqrt(252.0), tolerance);
        }
        EXPECT_TRUE(std::isfinite(bootstrap_values(sortino)[row]) ||
                    std::isinf(bootstrap_values(sortino)[row]) ||
                    std::isnan(bootstrap_values(sortino)[row]));
    }
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
