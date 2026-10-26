#include <gtest/gtest.h>

#include <fincore/calendar.hpp>

#include <limits>

TEST(Calendar, ConvertsEpochAndLeapDayAtUtcMidnight) {
    EXPECT_EQ(fincore::utc_timestamp(1970, 1, 1), 0);
    EXPECT_EQ(fincore::utc_timestamp(2020, 2, 29), 1582934400000000000LL);
}

TEST(Calendar, DefaultsToUtcNewYearMidnight) {
    EXPECT_EQ(fincore::utc_timestamp(2021), fincore::utc_timestamp(2021, 1, 1));
}

TEST(Calendar, PreservesSubDayNanosecondPrecision) {
    constexpr fincore::timestamp new_year_2021 = 1609459200000000000LL;
    constexpr fincore::timestamp offset = 45296123456789LL;

    EXPECT_EQ(fincore::utc_timestamp(2021, 1, 1, 12, 34, 56, 123456789),
              new_year_2021 + offset);
}

TEST(Calendar, ExtractsUtcYear) {
    // One nanosecond before the epoch is still 1969, so the extraction floors
    // toward earlier time instead of truncating the division toward zero.
    EXPECT_EQ(fincore::utc_year(-1), 1969);
    EXPECT_EQ(fincore::utc_year(fincore::utc_timestamp(2021, 1, 1)), 2021);
}

TEST(Calendar, PreservesExactSignedNanosecondLimits) {
    // The bounds are the first and last instants a signed 64-bit nanosecond
    // count can hold: each is asserted as accepted exactly and as rejected one
    // nanosecond beyond, so the comparison cannot drift by a single step.
    constexpr auto minimum = std::numeric_limits<fincore::timestamp>::min();
    constexpr auto maximum = std::numeric_limits<fincore::timestamp>::max();

    EXPECT_EQ(fincore::utc_timestamp(1677, 9, 21, 0, 12, 43, 145224192),
              minimum);
    EXPECT_THROW(fincore::utc_timestamp(1677, 9, 21, 0, 12, 43, 145224191),
                 fincore::input_error);
    EXPECT_EQ(fincore::utc_timestamp(2262, 4, 11, 23, 47, 16, 854775807),
              maximum);
    EXPECT_THROW(fincore::utc_timestamp(2262, 4, 11, 23, 47, 16, 854775808),
                 fincore::input_error);
}

TEST(Calendar, RejectsInvalidOrUnrepresentableComponents) {
    EXPECT_THROW(fincore::utc_timestamp(2021, 2, 29), fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(1970, 257, 1), fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(1970, 1, 257), fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(2021, 1, 1, 24),
                 fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(2021, 1, 1, 0, 60),
                 fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(2021, 1, 1, 0, 0, 60),
                 fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(2021, 1, 1, 0, 0, 0, 1000000000),
                 fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(2263, 1, 1), fincore::input_error);
}
