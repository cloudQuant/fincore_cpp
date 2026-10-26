#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/exceptions.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

TEST(FactorCalendar, SelectsLabelsMatchingThePinnedForwardGrammar) {
    // The non-ASCII labels pin the code-point grammar: U+0661 and U+FF11 count
    // as decimal digits, U+00B2 is a number but not a digit, and U+017F is
    // accepted wherever the grammar wants an s.
    const std::vector<std::string> columns{
        "1D", "10d", "2h", "30m", "15s", "1ms", "1us", "1ns",
        "1D2h3m4s5ms6us7ns", "1D1D", "1MS2US", "0D", "bad", "",
        "1M", "1Y", "1W", "D", "1", "1D", "\xD9\xA1" "D",
        "\xEF\xBC\x91" "D", "1\xC5\xBF", "\xC2\xB2" "D"};

    EXPECT_EQ(fincore::get_forward_returns_columns(columns),
              (std::vector<std::string>{
                  "1D", "10d", "2h", "30m", "15s", "1ms", "1us", "1ns",
                  "1D2h3m4s5ms6us7ns", "1D1D", "1MS2US", "0D", "1M",
                  "1D", "\xD9\xA1" "D", "\xEF\xBC\x91" "D",
                  "1\xC5\xBF"}));
}

TEST(FactorCalendar, ExactDayFilterKeepsRepeatedDayComponentsOnly) {
    const std::vector<std::string> columns{
        "1D", "10d", "2D1D", "1d2d", "1h", "1D1h", "1ms", "bad",
        "1D", "\xD9\xA1" "d"};

    EXPECT_EQ(fincore::get_forward_returns_columns(columns, true),
              (std::vector<std::string>{"1D", "10d", "2D1D", "1d2d",
                                        "1D", "\xD9\xA1" "d"}));
}

TEST(FactorCalendar, BackshiftsRowsByCompleteDateLevelPositions) {
    const auto first_date = fincore::utc_timestamp(2022, 1, 3);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const std::vector<fincore::timestamp> date_levels{
        first_date, first_date + day, first_date + 2 * day,
        first_date + 3 * day, first_date + 4 * day,
        first_date + 5 * day};

    // Date levels one and three hold no rows at all, so a shifted row can land
    // on a level the series never used rather than on an earlier present date.
    fincore::factor_series returns;
    returns.dates = {date_levels[0], date_levels[2], date_levels[2],
                     date_levels[4], date_levels[5]};
    returns.assets = {"A", "B", "C", "A", "C"};
    returns.values = {1.0, std::numeric_limits<double>::quiet_NaN(), -2.0,
                      5.0, 9.0};

    const auto shifted =
        fincore::backshift_returns_series(returns, date_levels, 1);
    ASSERT_EQ(shifted.dates.size(), 4U);
    EXPECT_EQ(shifted.dates,
              (std::vector<fincore::timestamp>{
                  date_levels[1], date_levels[1], date_levels[3],
                  date_levels[4]}));
    EXPECT_EQ(shifted.assets,
              (std::vector<std::string>{"B", "C", "A", "C"}));
    ASSERT_EQ(shifted.values.size(), 4U);
    EXPECT_TRUE(std::isnan(shifted.values[0]));
    EXPECT_DOUBLE_EQ(shifted.values[1], -2.0);
    EXPECT_DOUBLE_EQ(shifted.values[2], 5.0);
    EXPECT_DOUBLE_EQ(shifted.values[3], 9.0);
}

TEST(FactorCalendar, BackshiftReturnsEmptyWhenDateLevelCountIsNotGreaterThanN) {
    fincore::factor_series returns;
    returns.dates = {fincore::utc_timestamp(2022, 1, 3)};
    returns.assets = {"A"};
    returns.values = {std::numeric_limits<double>::quiet_NaN()};
    const std::vector<fincore::timestamp> date_levels{
        fincore::utc_timestamp(2022, 1, 3),
        fincore::utc_timestamp(2022, 1, 4)};

    for (const int shift : {2, 3}) {
        const auto shifted =
            fincore::backshift_returns_series(returns, date_levels, shift);
        EXPECT_TRUE(shifted.dates.empty());
        EXPECT_TRUE(shifted.assets.empty());
        EXPECT_TRUE(shifted.values.empty());
    }
}

TEST(FactorCalendar, RejectsInvalidNAndMalformedDateLevels) {
    fincore::factor_series returns;
    returns.dates = {fincore::utc_timestamp(2022, 1, 3)};
    returns.assets = {"A"};
    returns.values = {1.0};
    const auto first = fincore::utc_timestamp(2022, 1, 3);
    const auto second = fincore::utc_timestamp(2022, 1, 4);

    EXPECT_THROW(fincore::backshift_returns_series(returns, {first, second}, 0),
                 fincore::factor_data_error);
    EXPECT_THROW(fincore::backshift_returns_series(returns, {first, second}, -1),
                 fincore::factor_data_error);
    EXPECT_THROW(fincore::backshift_returns_series(returns, {second, first}, 1),
                 fincore::factor_data_error);
    EXPECT_THROW(fincore::backshift_returns_series(returns, {first, first}, 1),
                 fincore::factor_data_error);
    EXPECT_THROW(fincore::backshift_returns_series(returns, {second}, 1),
                 fincore::factor_data_error);

    auto misaligned = returns;
    misaligned.values.clear();
    EXPECT_THROW(
        fincore::backshift_returns_series(misaligned, {first, second}, 1),
        fincore::factor_data_error);
}

}  // namespace
