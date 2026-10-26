#include <fincore/simulation/paths.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace {

using latin_hypercube_sampling_function = fincore::dataframe (*) (
    std::size_t, std::size_t, std::optional<std::uint64_t>);

static_assert(std::is_same_v<decltype(&fincore::latin_hypercube_sampling),
                             latin_hypercube_sampling_function>);

std::string dimension_name(const std::size_t dimension) {
    return std::string{fincore::latin_hypercube_dimension_column_prefix} +
           std::to_string(dimension);
}

void expect_schema(const fincore::dataframe& samples,
                   const std::size_t sample_count,
                   const std::size_t dimension_count) {
    std::vector<fincore::timestamp> expected_index;
    expected_index.reserve(sample_count);
    for (std::size_t sample = 0U; sample < sample_count; ++sample) {
        expected_index.push_back(static_cast<fincore::timestamp>(sample));
    }

    EXPECT_EQ(samples.get_index(), expected_index);
    EXPECT_EQ(samples.shape().first, sample_count);
    EXPECT_EQ(samples.shape().second, dimension_count);

    const auto columns = samples.get_columns_info<double>();
    ASSERT_EQ(columns.size(), dimension_count);
    for (std::size_t dimension = 0U; dimension < dimension_count; ++dimension) {
        const std::string expected_name = dimension_name(dimension);
        EXPECT_EQ(std::get<0>(columns[dimension]), expected_name.c_str());
        EXPECT_EQ(std::get<1>(columns[dimension]), sample_count);
        EXPECT_EQ(std::get<2>(columns[dimension]), typeid(double));
    }
}

void expect_open_unit_strata(const fincore::dataframe& samples,
                             const std::size_t sample_count,
                             const std::size_t dimension_count) {
    ASSERT_GT(sample_count, 0U);
    const double denominator = static_cast<double>(sample_count);
    for (std::size_t dimension = 0U; dimension < dimension_count; ++dimension) {
        std::vector<bool> seen_strata(sample_count, false);
        const auto& values = samples.get_column<double>(dimension_name(dimension).c_str());
        ASSERT_EQ(values.size(), sample_count);
        for (const double value : values) {
            ASSERT_TRUE(std::isfinite(value));
            ASSERT_GE(value, 0.0);
            ASSERT_LT(value, 1.0);
            // Multiplying back into the unit interval is the generator's own rejection
            // rule, so this also catches a jittered value that rounding slid into the
            // neighbouring stratum.
            const double raw_stratum = std::floor(value * denominator);
            ASSERT_GE(raw_stratum, 0.0);
            ASSERT_LT(raw_stratum, denominator);
            const auto stratum = static_cast<std::size_t>(raw_stratum);
            EXPECT_FALSE(seen_strata[stratum]);
            seen_strata[stratum] = true;
        }
        for (const bool seen : seen_strata) {
            EXPECT_TRUE(seen);
        }
    }
}

TEST(LatinHypercubeSampling, PublishesExactContractAndDefaultCallExpression) {
    EXPECT_EQ(fincore::latin_hypercube_dimension_column_prefix, "lhs_dimension_");

    const auto samples = fincore::latin_hypercube_sampling(1U, 1U);
    expect_schema(samples, 1U, 1U);
    expect_open_unit_strata(samples, 1U, 1U);
}

TEST(LatinHypercubeSampling, PreservesSourceSingletonMultiDimensionShape) {
    const auto samples = fincore::latin_hypercube_sampling(1U, 3U, 42U);

    expect_schema(samples, 1U, 3U);
    expect_open_unit_strata(samples, 1U, 3U);
}

TEST(LatinHypercubeSampling, PreservesTypedSchemaOrdinalIndexAndDimensionOrder) {
    const auto samples = fincore::latin_hypercube_sampling(4U, 3U, 42U);

    expect_schema(samples, 4U, 3U);
    expect_open_unit_strata(samples, 4U, 3U);
}

TEST(LatinHypercubeSampling, PreservesEverySourceZeroShape) {
    const auto rows_only = fincore::latin_hypercube_sampling(4U, 0U, 42U);
    expect_schema(rows_only, 4U, 0U);

    const auto typed_empty_columns =
        fincore::latin_hypercube_sampling(0U, 3U, 42U);
    expect_schema(typed_empty_columns, 0U, 3U);
    for (std::size_t dimension = 0U; dimension < 3U; ++dimension) {
        EXPECT_TRUE(
            typed_empty_columns.get_column<double>(dimension_name(dimension).c_str()).empty());
    }

    const auto empty = fincore::latin_hypercube_sampling(0U, 0U, 42U);
    expect_schema(empty, 0U, 0U);
}

TEST(LatinHypercubeSampling, RepeatsNativeSeedAndDifferentSeedsDiverge) {
    const auto first = fincore::latin_hypercube_sampling(6U, 3U, 917U);
    const auto repeated = fincore::latin_hypercube_sampling(6U, 3U, 917U);
    const auto distinct = fincore::latin_hypercube_sampling(6U, 3U, 918U);

    expect_schema(first, 6U, 3U);
    expect_schema(repeated, 6U, 3U);
    expect_schema(distinct, 6U, 3U);

    bool diverged = false;
    for (std::size_t dimension = 0U; dimension < 3U; ++dimension) {
        const std::string name = dimension_name(dimension);
        EXPECT_EQ(first.get_column<double>(name.c_str()),
                  repeated.get_column<double>(name.c_str()));
        diverged = diverged || first.get_column<double>(name.c_str()) !=
                                  distinct.get_column<double>(name.c_str());
    }
    EXPECT_TRUE(diverged);
}

TEST(LatinHypercubeSampling, BuildsFiniteUnseededStructuralOutput) {
    const auto samples = fincore::latin_hypercube_sampling(5U, 2U);

    expect_schema(samples, 5U, 2U);
    expect_open_unit_strata(samples, 5U, 2U);
}

TEST(LatinHypercubeSampling, OwnsResultIndexLabelsAndValues) {
    const auto retained = [] {
        return fincore::latin_hypercube_sampling(4U, 2U, 42U);
    }();
    const auto expected_index = retained.get_index();
    const auto expected_first = retained.get_column<double>("lhs_dimension_0");
    const auto expected_second = retained.get_column<double>("lhs_dimension_1");

    const auto unrelated = fincore::latin_hypercube_sampling(3U, 1U, 7U);

    expect_schema(retained, 4U, 2U);
    EXPECT_EQ(retained.get_index(), expected_index);
    EXPECT_EQ(retained.get_column<double>("lhs_dimension_0"), expected_first);
    EXPECT_EQ(retained.get_column<double>("lhs_dimension_1"), expected_second);
    expect_schema(unrelated, 3U, 1U);
}

TEST(LatinHypercubeSampling, RejectsUnrepresentableOutputCountsBeforeAllocation) {
    constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();

    EXPECT_THROW(fincore::latin_hypercube_sampling(maximum, 0U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::latin_hypercube_sampling(0U, maximum, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::latin_hypercube_sampling(2U, maximum, 1U),
                 fincore::input_error);

    constexpr auto double_digits =
        static_cast<unsigned>(std::numeric_limits<double>::digits);
    constexpr auto size_digits =
        static_cast<unsigned>(std::numeric_limits<std::size_t>::digits);
    // One past the largest count whose strata can still each hold a distinct double, which
    // mirrors the implementation's own precision guard; where the two significand widths
    // agree that guard has nothing left to reject and the branch compiles out.
    if constexpr (double_digits < size_digits) {
        constexpr std::size_t maximum_precise_sample_count =
            std::size_t{1} << double_digits;
        try {
            static_cast<void>(fincore::latin_hypercube_sampling(
                maximum_precise_sample_count + 1U, 1U, 1U));
            FAIL() << "expected the double-precision guard to reject the sample count";
        } catch (const fincore::input_error& error) {
            EXPECT_STREQ(error.what(),
                         "Latin-hypercube sample count exceeds double precision");
        }
    }
}

TEST(LatinHypercubeSampling, RejectsCellOverflowAfterValidShapeCapacityChecks) {
    constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();
    const std::size_t dimension_capacity = std::vector<std::string>{}.max_size();
    ASSERT_GT(dimension_capacity, 1U);

    const std::size_t sample_count = maximum / dimension_capacity + 1U;
    ASSERT_LT(sample_count, maximum);
    ASSERT_LE(sample_count - 1U,
              static_cast<std::size_t>(std::numeric_limits<fincore::timestamp>::max()));
    ASSERT_LE(sample_count, std::vector<fincore::timestamp>{}.max_size());
    ASSERT_LE(sample_count, std::vector<std::size_t>{}.max_size());
    ASSERT_LE(sample_count, std::vector<double>{}.max_size());

    try {
        static_cast<void>(
            fincore::latin_hypercube_sampling(sample_count, dimension_capacity, 1U));
        FAIL() << "expected the cell-count guard to reject the shape";
    } catch (const fincore::input_error& error) {
        EXPECT_STREQ(error.what(),
                     "Latin-hypercube output cell count is not representable");
    }
}

}  // namespace
