#include <gtest/gtest.h>

#include <fincore/calendar.hpp>

TEST(Calendar, ConvertsEpochAndLeapDayAtUtcMidnight) {
    EXPECT_EQ(fincore::utc_timestamp(1970, 1, 1), 0);
    EXPECT_EQ(fincore::utc_timestamp(2020, 2, 29), 1582934400000000000LL);
}

TEST(Calendar, PreservesSubDayNanosecondPrecision) {
    constexpr fincore::timestamp new_year_2021 = 1609459200000000000LL;
    constexpr fincore::timestamp offset = 45296123456789LL;

    EXPECT_EQ(fincore::utc_timestamp(2021, 1, 1, 12, 34, 56, 123456789),
              new_year_2021 + offset);
}

TEST(Calendar, ExtractsUtcYear) {
    EXPECT_EQ(fincore::utc_year(-1), 1969);
    EXPECT_EQ(fincore::utc_year(fincore::utc_timestamp(2021, 1, 1)), 2021);
}

TEST(Calendar, RejectsInvalidOrUnrepresentableComponents) {
    EXPECT_THROW(fincore::utc_timestamp(2021, 2, 29), fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(2021, 1, 1, 0, 0, 60),
                 fincore::input_error);
    EXPECT_THROW(fincore::utc_timestamp(2263, 1, 1), fincore::input_error);
}
