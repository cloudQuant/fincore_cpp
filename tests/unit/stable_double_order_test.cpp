#include <fincore/detail/stable_double_order.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

namespace {

void expect_matches_stable_reference(const std::vector<double>& values,
                                     std::vector<std::size_t>& scratch) {
    std::vector<std::size_t> actual(values.size());
    std::iota(actual.begin(), actual.end(), 0U);
    // Shuffle the input index order while keeping an independent reference
    // over the same indices, so stability is checked from the supplied order.
    std::mt19937 random{0x51a7U};
    std::shuffle(actual.begin(), actual.end(), random);
    auto expected = actual;
    std::stable_sort(
        expected.begin(), expected.end(),
        [&values](const auto left, const auto right) {
            return values[left] < values[right];
        });
    fincore::detail::stable_sort_indices_by_double_value(
        actual, scratch,
        [&values](const std::size_t index) { return values[index]; });
    EXPECT_EQ(actual, expected);
}

void expect_matches_stable_reference(const std::vector<double>& values) {
    std::vector<std::size_t> scratch;
    expect_matches_stable_reference(values, scratch);
}

}  // namespace

TEST(StableDoubleOrderTest, MatchesStableSortAtAndAroundRadixThreshold) {
    const std::vector<double> values{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::denorm_min(),
        -0.0,
        0.0,
        std::numeric_limits<double>::denorm_min(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::infinity(),
        -1.25,
        1.25,
        -1.25,
        1.25,
    };
    constexpr auto threshold =
        fincore::detail::stable_double_order_radix_threshold;
    const std::vector<std::size_t> counts{
        0U, 1U, threshold - 1U, threshold, threshold + 1U};
    for (const std::size_t count : counts) {
        std::vector<double> sample;
        sample.reserve(count);
        for (std::size_t index = 0U; index < count; ++index) {
            sample.push_back(values[index % values.size()]);
        }
        expect_matches_stable_reference(sample);
    }
}

TEST(StableDoubleOrderTest, MatchesStableSortForNonNanBitPatterns) {
    std::vector<double> values{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::denorm_min(),
        -0.0,
        0.0,
        std::numeric_limits<double>::denorm_min(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::infinity(),
    };
    std::mt19937_64 random{0x9f38d15bU};
    values.reserve(8192U);
    while (values.size() < 8192U) {
        if (values.size() % 17U == 0U) {
            values.push_back(values[values.size() % 8U]);
            continue;
        }
        auto bits = static_cast<std::uint64_t>(random());
        constexpr std::uint64_t exponent_mask =
            std::uint64_t{0x7ffU} << 52U;
        if ((bits & exponent_mask) == exponent_mask) {
            bits = (bits & ~exponent_mask) |
                   (std::uint64_t{0x7feU} << 52U);
        }
        values.push_back(std::bit_cast<double>(bits));
    }
    expect_matches_stable_reference(values);
}

TEST(StableDoubleOrderTest, SkipsIdentityPassesAndCopiesBackOddPasses) {
    std::vector<double> values;
    values.reserve(2048U);
    for (std::size_t index = 0U; index < 2048U; ++index) {
        const auto bits =
            std::uint64_t{0x3ff0000000000000ULL} | (index % 256U);
        values.push_back(std::bit_cast<double>(bits));
    }
    expect_matches_stable_reference(values);
}

TEST(StableDoubleOrderTest, ReusesScratchAcrossDifferentInputLengths) {
    const std::vector<double> patterns{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::denorm_min(),
        -0.0,
        0.0,
        std::numeric_limits<double>::denorm_min(),
        -1.25,
        1.25,
        -1.25,
        1.25,
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::infinity(),
    };
    const std::vector<std::size_t> counts{2048U, 1024U, 17U, 3073U};
    std::vector<std::size_t> scratch;
    for (const auto count : counts) {
        std::vector<double> values;
        values.reserve(count);
        for (std::size_t index = 0U; index < count; ++index) {
            values.push_back(patterns[index % patterns.size()]);
        }
        expect_matches_stable_reference(values, scratch);
    }
}

