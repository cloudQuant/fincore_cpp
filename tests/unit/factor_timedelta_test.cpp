#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/timedelta.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr fincore::timestamp day = fincore::nanoseconds_per_day;

fincore::timestamp at(const int year, const unsigned month, const unsigned day_of_month,
                      const unsigned hour = 0U) {
    return fincore::utc_timestamp(year, month, day_of_month, hour);
}

TEST(FactorTimedelta, InfersWeekdaysAndMissingSessionHolidays) {
    const std::vector<fincore::timestamp> factor_dates{
        at(2022, 1, 3), at(2022, 1, 4), at(2022, 1, 10), at(2022, 1, 13)};
    const std::vector<fincore::timestamp> price_dates{
        at(2022, 1, 10), at(2022, 1, 11)};

    const auto calendar =
        fincore::infer_trading_calendar(factor_dates, price_dates);
    EXPECT_TRUE(calendar.weekdays[0]);
    EXPECT_TRUE(calendar.weekdays[1]);
    EXPECT_FALSE(calendar.weekdays[2]);
    EXPECT_TRUE(calendar.weekdays[3]);
    EXPECT_FALSE(calendar.weekdays[4]);
    ASSERT_EQ(calendar.holidays.size(), 1U);
    EXPECT_EQ(calendar.holidays[0], at(2022, 1, 6) / day);
    EXPECT_TRUE(calendar.is_session(at(2022, 1, 10)));
    EXPECT_FALSE(calendar.is_session(at(2022, 1, 6)));
    EXPECT_FALSE(calendar.is_session(at(2022, 1, 7)));

    EXPECT_THROW(fincore::infer_trading_calendar({}, {}),
                 fincore::factor_data_error);
}

TEST(FactorTimedelta, AddsWeekdaysAndPreservesTheRemainder) {
    const auto calendar = fincore::business_day_calendar();
    const auto saturday = at(2022, 1, 1, 10U);

    EXPECT_EQ(fincore::add_custom_calendar_timedelta(saturday, day, calendar),
              at(2022, 1, 3, 10U));
    EXPECT_EQ(fincore::add_custom_calendar_timedelta(saturday, 0, calendar),
              at(2022, 1, 3, 10U));
    EXPECT_EQ(fincore::add_custom_calendar_timedelta(saturday, -day, calendar),
              at(2021, 12, 31, 10U));
    EXPECT_EQ(fincore::add_custom_calendar_timedelta(
                  saturday, 2 * day, calendar),
              at(2022, 1, 4, 10U));
    EXPECT_EQ(fincore::add_custom_calendar_timedelta(
                  saturday, -2 * day, calendar),
              at(2021, 12, 30, 10U));
    // A negative partial hour normalizes to -1 day + 23h.
    EXPECT_EQ(fincore::add_custom_calendar_timedelta(
                  saturday, -3'600'000'000'000LL, calendar),
              at(2022, 1, 1, 9U));

    const std::vector<fincore::timestamp> values{saturday, at(2022, 1, 4, 10U)};
    const auto shifted = fincore::add_custom_calendar_timedelta(
        values, day, calendar);
    ASSERT_EQ(shifted.size(), 2U);
    EXPECT_EQ(shifted[0], at(2022, 1, 3, 10U));
    EXPECT_EQ(shifted[1], at(2022, 1, 5, 10U));

    const auto all_days = fincore::all_days_calendar();
    EXPECT_EQ(fincore::add_custom_calendar_timedelta(
                  at(2022, 1, 7, 10U), day, all_days),
              at(2022, 1, 8, 10U));
}

TEST(FactorTimedelta, DifferencesUseHalfOpenTradingDayCounts) {
    const auto calendar = fincore::business_day_calendar();

    EXPECT_EQ(fincore::diff_custom_calendar_timedeltas(
                  at(2022, 1, 1, 10U), at(2022, 1, 3, 10U), calendar),
              0);
    EXPECT_EQ(fincore::diff_custom_calendar_timedeltas(
                  at(2022, 1, 3, 10U), at(2022, 1, 1, 10U), calendar),
              -day);
    EXPECT_EQ(fincore::diff_custom_calendar_timedeltas(
                  at(2022, 1, 1, 10U), at(2022, 1, 1, 12U), calendar),
              2 * 3'600'000'000'000LL);
    EXPECT_EQ(fincore::diff_custom_calendar_timedeltas(
                  at(2022, 1, 3, 10U), at(2022, 1, 4, 12U), calendar),
              day + 2 * 3'600'000'000'000LL);

    fincore::trading_calendar with_holiday = calendar;
    with_holiday.holidays.push_back(at(2022, 1, 4) / day);
    EXPECT_EQ(fincore::diff_custom_calendar_timedeltas(
                  at(2022, 1, 3, 10U), at(2022, 1, 5, 10U), with_holiday),
              day);
}

TEST(FactorTimedelta, FormatsAndParsesPinnedDurationStrings) {
    EXPECT_EQ(fincore::timedelta_to_string(day), "1D");
    EXPECT_EQ(fincore::timedelta_to_string(-day), "-1D");
    EXPECT_EQ(fincore::timedelta_to_string(-3'600'000'000'000LL), "-1D23h");
    EXPECT_EQ(fincore::timedelta_to_string(0), "");
    EXPECT_EQ(fincore::timedelta_to_string(
                  day + 2 * 3'600'000'000'000LL + 3 * 60'000'000'000LL +
                      4'000'000'000LL + 5'000'000LL + 6'000LL + 7LL),
              "1D2h3m4s5ms6us7ns");

    EXPECT_EQ(fincore::timedelta_strings_to_integers(
                  {"1D", "-1D", "-1h", "1D2h", "2h30m"}),
              (std::vector<int>{1, -1, -1, 1, 0}));
    EXPECT_EQ(fincore::parse_timedelta_nanoseconds("1D2h"), day + 2 * 3'600'000'000'000LL);
    EXPECT_THROW(fincore::parse_timedelta_nanoseconds(""),
                 fincore::factor_data_error);
    EXPECT_THROW(fincore::parse_timedelta_nanoseconds("1X"),
                 fincore::factor_data_error);
    EXPECT_THROW(fincore::parse_timedelta_nanoseconds("D"),
                 fincore::factor_data_error);
}

}  // namespace
