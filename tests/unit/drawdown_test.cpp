#include <fincore/calendar.hpp>
#include <fincore/metrics/drawdown.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

namespace {

TEST(Drawdown, UsesPeakToTroughCompoundedReturns) {
    const auto returns =
        fincore::make_series({1, 2, 3}, "returns", {0.10, -0.20, 0.05});

    EXPECT_NEAR(fincore::max_drawdown(returns), -0.20, 1e-12);
}

TEST(Drawdown, PreservesReturnsIndexInDrawdownSeries) {
    const auto returns =
        fincore::make_series({10, 20, 30}, "returns", {0.10, -0.20, 0.05});

    const auto drawdown = fincore::drawdown_series(returns);

    EXPECT_EQ(drawdown.get_index(), returns.get_index());
    const auto& values =
        drawdown.get_column<double>(fincore::returns_column.data());
    ASSERT_EQ(values.size(), 3U);
    EXPECT_NEAR(values[0], 0.0, 1e-12);
    EXPECT_NEAR(values[1], -0.20, 1e-12);
    EXPECT_NEAR(values[2], -0.16, 1e-12);
}

TEST(Drawdown, ReturnsNanForEmptyInput) {
    const auto returns = fincore::make_series({}, "returns", {});

    EXPECT_TRUE(std::isnan(fincore::max_drawdown(returns)));
}

TEST(Drawdown, TreatsNanReturnAsZeroWhenCalculatingWealth) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.10, std::numeric_limits<double>::quiet_NaN(), -0.10});

    EXPECT_NEAR(fincore::max_drawdown(returns), -0.10, 1e-12);
}

TEST(Drawdown, ReportsDetailedEpisodesInChronologicalFrameOrder) {
    const std::vector<fincore::timestamp> dates{
        1577836800000000000LL, 1577923200000000000LL,
        1578009600000000000LL, 1578096000000000000LL,
        1578182400000000000LL, 1578268800000000000LL,
        1578355200000000000LL, 1578441600000000000LL,
        1578528000000000000LL, 1578614400000000000LL};
    const auto returns = fincore::make_series(
        dates, "returns",
        {0.10, -0.10, -0.10, 0.25, -0.05, -0.05, 0.11, -0.20, 0.30,
         -0.10});

    const auto values = fincore::get_all_drawdowns(returns);
    ASSERT_EQ(values.size(), 4U);
    EXPECT_NEAR(values[0], -0.19, 1e-12);
    EXPECT_NEAR(values[1], -0.0975, 1e-12);
    EXPECT_NEAR(values[2], -0.20, 1e-12);
    EXPECT_NEAR(values[3], -0.10, 1e-12);

    const auto periods = fincore::get_all_drawdowns_detailed(returns);
    ASSERT_EQ(periods.size(), 4U);
    EXPECT_EQ(periods[0].start, dates[1]);
    EXPECT_EQ(periods[0].valley, dates[2]);
    EXPECT_EQ(periods[0].recovery,
              std::optional<fincore::timestamp>{dates[3]});
    EXPECT_NEAR(periods[0].value, -0.19, 1e-12);
    EXPECT_EQ(periods[0].duration_periods, 1U);
    EXPECT_EQ(periods[0].recovery_duration_periods,
              std::optional<std::size_t>{1U});

    EXPECT_EQ(periods[1].start, dates[4]);
    EXPECT_EQ(periods[1].valley, dates[5]);
    EXPECT_EQ(periods[1].recovery,
              std::optional<fincore::timestamp>{dates[6]});
    EXPECT_NEAR(periods[1].value, -0.0975, 1e-12);
    EXPECT_EQ(periods[1].duration_periods, 1U);
    EXPECT_EQ(periods[1].recovery_duration_periods,
              std::optional<std::size_t>{1U});

    EXPECT_EQ(periods[2].start, dates[7]);
    EXPECT_EQ(periods[2].valley, dates[7]);
    EXPECT_EQ(periods[2].recovery,
              std::optional<fincore::timestamp>{dates[8]});
    EXPECT_NEAR(periods[2].value, -0.20, 1e-12);
    EXPECT_EQ(periods[2].duration_periods, 0U);
    EXPECT_EQ(periods[2].recovery_duration_periods,
              std::optional<std::size_t>{1U});

    EXPECT_EQ(periods[3].start, dates[9]);
    EXPECT_EQ(periods[3].valley, dates[9]);
    EXPECT_EQ(periods[3].recovery, std::nullopt);
    EXPECT_NEAR(periods[3].value, -0.10, 1e-12);
    EXPECT_EQ(periods[3].duration_periods, 0U);
    EXPECT_EQ(periods[3].recovery_duration_periods, std::nullopt);

    const auto window = fincore::max_drawdown_window(returns);
    ASSERT_TRUE(window.has_value());
    EXPECT_EQ(window->peak, dates[6]);
    EXPECT_EQ(window->valley, dates[7]);
    EXPECT_EQ(window->recovery,
              std::optional<fincore::timestamp>{dates[8]});
}

TEST(Drawdown, UsesEarliestTroughAndEpisodeWhenDrawdownsTie) {
    // Labels are deliberately out of ascending order: the scan walks frame
    // positions, so "earliest" here is the first row, not the smallest label.
    const auto returns = fincore::make_series(
        {30, 10, 20, 40, 50, 60}, "returns",
        {0.0, -0.20, 0.0, 0.25, -0.20, 0.25});

    const auto periods = fincore::get_all_drawdowns_detailed(returns);
    ASSERT_EQ(periods.size(), 2U);
    EXPECT_EQ(periods[0].start, 10);
    EXPECT_EQ(periods[0].valley, 10);
    EXPECT_EQ(periods[0].recovery,
              std::optional<fincore::timestamp>{40});
    EXPECT_EQ(periods[1].start, 50);

    const auto window = fincore::max_drawdown_window(returns);
    ASSERT_TRUE(window.has_value());
    EXPECT_EQ(window->peak, 30);
    EXPECT_EQ(window->valley, 10);
    EXPECT_EQ(window->recovery,
              std::optional<fincore::timestamp>{40});
}

TEST(Drawdown, TreatsNanAsZeroAndKeepsTheFirstEqualValley) {
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto returns =
        fincore::make_series({1, 2, 3, 4}, "returns", {0.10, -0.20, nan, 0.25});

    const auto periods = fincore::get_all_drawdowns_detailed(returns);
    ASSERT_EQ(periods.size(), 1U);
    EXPECT_EQ(periods[0].start, 2);
    EXPECT_EQ(periods[0].valley, 2);
    EXPECT_EQ(periods[0].recovery,
              std::optional<fincore::timestamp>{4});
    EXPECT_NEAR(periods[0].value, -0.20, 1e-12);
    EXPECT_EQ(periods[0].duration_periods, 0U);
    EXPECT_EQ(periods[0].recovery_duration_periods,
              std::optional<std::size_t>{2U});
}

TEST(Drawdown, SupportsNamedColumns) {
    const auto returns =
        fincore::make_series({1, 2, 3}, "strategy", {0.10, -0.20, 0.25});

    const auto values = fincore::get_all_drawdowns(returns, "strategy");
    const auto periods =
        fincore::get_all_drawdowns_detailed(returns, "strategy");
    const auto window = fincore::max_drawdown_window(returns, "strategy");

    ASSERT_EQ(values.size(), 1U);
    EXPECT_NEAR(values[0], -0.20, 1e-12);
    ASSERT_EQ(periods.size(), 1U);
    EXPECT_EQ(periods[0].start, 2);
    ASSERT_TRUE(window.has_value());
    EXPECT_EQ(window->peak, 1);
}

TEST(Drawdown, ReturnsEmptyDetailedResultsWithoutASyntheticInitialPeak) {
    const auto empty = fincore::make_series({}, "returns", {});
    const auto initial_loss = fincore::make_series({1}, "returns", {-0.01});

    EXPECT_TRUE(fincore::get_all_drawdowns(empty).empty());
    EXPECT_TRUE(fincore::get_all_drawdowns_detailed(empty).empty());
    EXPECT_EQ(fincore::max_drawdown_window(empty), std::nullopt);
    EXPECT_NEAR(fincore::max_drawdown(initial_loss), -0.01, 1e-12);
    EXPECT_TRUE(fincore::get_all_drawdowns_detailed(initial_loss).empty());
    EXPECT_EQ(fincore::max_drawdown_window(initial_loss), std::nullopt);
}

TEST(Drawdown, DoesNotInvertUnderwaterSignWhenInitialWealthIsNegative) {
    const auto returns = fincore::make_series(
        {1577836800000000000LL, 1577923200000000000LL,
         1578009600000000000LL},
        "returns", {-1.5, 0.0, -0.2});

    EXPECT_TRUE(fincore::get_all_drawdowns(returns).empty());
    EXPECT_TRUE(fincore::get_all_drawdowns_detailed(returns).empty());
    EXPECT_EQ(fincore::max_drawdown_window(returns), std::nullopt);
}

TEST(Drawdown, RejectsInvalidColumnsForDetailedApis) {
    fincore::dataframe missing;
    const auto empty = fincore::make_series({}, "returns", {});
    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{});
    wrong_type.load_column<int>("returns", std::vector<int>{},
                                hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(fincore::get_all_drawdowns(missing),
                 fincore::input_error);
    EXPECT_THROW(fincore::get_all_drawdowns_detailed(empty, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::max_drawdown_window(wrong_type),
                 fincore::input_error);
}

TEST(Drawdown, MatchesPythonDurationAndSeverityRankGoldens) {
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    std::vector<fincore::timestamp> dates;
    for (std::int64_t position = 0; position < 10; ++position) {
        dates.push_back(1'577'836'800'000'000'000LL + position * day);
    }
    const auto returns = fincore::make_series(
        dates, "returns",
        {0.10, -0.10, -0.10, 0.25, -0.05, -0.05, 0.11, -0.20, 0.30,
         -0.10});

    EXPECT_DOUBLE_EQ(fincore::max_drawdown_days(returns), 1.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_weeks(returns), 1.0 / 7.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_months(returns), 1.0 / 30.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_recovery_days(returns), 1.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_recovery_weeks(returns),
                     1.0 / 7.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_recovery_months(returns),
                     1.0 / 30.0);
    EXPECT_NEAR(fincore::second_max_drawdown(returns), -0.19, 1e-12);
    EXPECT_NEAR(fincore::third_max_drawdown(returns), -0.10, 1e-12);
    EXPECT_DOUBLE_EQ(fincore::second_max_drawdown_days(returns), 1.0);
    EXPECT_DOUBLE_EQ(fincore::second_max_drawdown_recovery_days(returns),
                     1.0);
    EXPECT_DOUBLE_EQ(fincore::third_max_drawdown_days(returns), 0.0);
    EXPECT_TRUE(
        std::isnan(fincore::third_max_drawdown_recovery_days(returns)));
}

TEST(Drawdown, ReturnsNanForEmptyDurationAndRankInputsAfterValidation) {
    const auto empty = fincore::make_series({}, "returns", {});

    EXPECT_TRUE(std::isnan(fincore::max_drawdown_days(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_drawdown_weeks(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_drawdown_months(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_drawdown_recovery_days(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_drawdown_recovery_weeks(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_drawdown_recovery_months(empty)));
    EXPECT_TRUE(std::isnan(fincore::second_max_drawdown(empty)));
    EXPECT_TRUE(std::isnan(fincore::third_max_drawdown(empty)));
    EXPECT_TRUE(std::isnan(fincore::second_max_drawdown_days(empty)));
    EXPECT_TRUE(
        std::isnan(fincore::second_max_drawdown_recovery_days(empty)));
    EXPECT_TRUE(std::isnan(fincore::third_max_drawdown_days(empty)));
    EXPECT_TRUE(
        std::isnan(fincore::third_max_drawdown_recovery_days(empty)));
    EXPECT_THROW(fincore::max_drawdown_days(empty, "missing"),
                 fincore::input_error);
    EXPECT_THROW(fincore::second_max_drawdown(empty, "missing"),
                 fincore::input_error);
}

TEST(Drawdown, ReturnsZeroDurationsWhenAPathNeverGoesUnderwater) {
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const auto positive = fincore::make_series(
        {0, day, 2 * day}, "returns", {0.10, 0.0, 0.05});
    const auto initial_loss =
        fincore::make_series({0}, "returns", {-0.01});

    EXPECT_DOUBLE_EQ(fincore::max_drawdown_days(positive), 0.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_recovery_days(positive), 0.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_weeks(positive), 0.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_recovery_months(positive), 0.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_days(initial_loss), 0.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_recovery_days(initial_loss), 0.0);
}

TEST(Drawdown, UsesElapsedUtcDaysAndReportsOpenRecoveryAsNan) {
    constexpr fincore::timestamp hour = 3'600'000'000'000LL;
    const auto recovered = fincore::make_series(
        {0, 47 * hour, 70 * hour}, "returns", {0.0, -0.20, 0.25});
    const auto open =
        fincore::make_series({0, 10 * 24 * hour}, "returns", {0.0, -0.20});

    EXPECT_DOUBLE_EQ(fincore::max_drawdown_days(recovered), 1.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_weeks(recovered), 1.0 / 7.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_months(recovered), 1.0 / 30.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_recovery_days(recovered), 0.0);
    EXPECT_DOUBLE_EQ(fincore::max_drawdown_days(open), 10.0);
    EXPECT_TRUE(std::isnan(fincore::max_drawdown_recovery_days(open)));
    EXPECT_TRUE(std::isnan(fincore::max_drawdown_recovery_weeks(open)));
    EXPECT_TRUE(std::isnan(fincore::max_drawdown_recovery_months(open)));
}

TEST(Drawdown, ComputesElapsedDaysWithoutTimestampSubtractionOverflow) {
    const auto returns = fincore::make_series(
        {std::numeric_limits<fincore::timestamp>::min(),
         std::numeric_limits<fincore::timestamp>::max()},
        "returns", {0.0, -0.20});

    EXPECT_DOUBLE_EQ(fincore::max_drawdown_days(returns), 213503.0);
}

TEST(Drawdown, UsesUtcElapsedDaysForRankedEpisodeDurations) {
    constexpr fincore::timestamp hour = 3'600'000'000'000LL;
    const auto returns = fincore::make_series(
        {0, 24 * hour, 48 * hour, 72 * hour, 119 * hour, 168 * hour,
         192 * hour, 216 * hour},
        "returns", {0.0, -0.50, 1.0, -0.20, -0.0625, 1.0 / 3.0, -0.10,
                    1.0 / 9.0});

    EXPECT_NEAR(fincore::second_max_drawdown(returns), -0.25, 1e-12);
    EXPECT_DOUBLE_EQ(fincore::second_max_drawdown_days(returns), 1.0);
    EXPECT_DOUBLE_EQ(fincore::second_max_drawdown_recovery_days(returns),
                     2.0);
    EXPECT_DOUBLE_EQ(fincore::third_max_drawdown_days(returns), 0.0);
    EXPECT_DOUBLE_EQ(fincore::third_max_drawdown_recovery_days(returns),
                     1.0);
}

TEST(Drawdown, KeepsChronologicalOrderWhenSeverityRanksTie) {
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const auto returns = fincore::make_series(
        {0, day, 2 * day, 3 * day, 8 * day, 9 * day}, "returns",
        {0.0, -0.20, 0.25, -0.20, 0.25, -0.10});

    EXPECT_NEAR(fincore::second_max_drawdown(returns), -0.20, 1e-12);
    EXPECT_DOUBLE_EQ(fincore::second_max_drawdown_days(returns), 0.0);
    EXPECT_DOUBLE_EQ(fincore::second_max_drawdown_recovery_days(returns),
                     5.0);
    EXPECT_NEAR(fincore::third_max_drawdown(returns), -0.10, 1e-12);
}

static_assert(std::is_same_v<decltype(fincore::drawdown_period::start),
                             fincore::timestamp>);
static_assert(std::is_same_v<decltype(fincore::drawdown_period::valley),
                             fincore::timestamp>);
static_assert(
    std::is_same_v<decltype(fincore::drawdown_period::recovery),
                   std::optional<fincore::timestamp>>);
static_assert(std::is_same_v<decltype(fincore::drawdown_period::value), double>);
static_assert(
    std::is_same_v<decltype(fincore::drawdown_period::duration_periods),
                   std::size_t>);
static_assert(std::is_same_v<
              decltype(fincore::drawdown_period::recovery_duration_periods),
              std::optional<std::size_t>>);
static_assert(std::is_same_v<decltype(fincore::drawdown_window::peak),
                             fincore::timestamp>);
static_assert(std::is_same_v<decltype(fincore::drawdown_window::valley),
                             fincore::timestamp>);
static_assert(std::is_same_v<decltype(fincore::drawdown_window::recovery),
                             std::optional<fincore::timestamp>>);

std::vector<fincore::timestamp> business_day_index() {
    return {
        fincore::utc_timestamp(2021, 1, 4),  fincore::utc_timestamp(2021, 1, 5),
        fincore::utc_timestamp(2021, 1, 6),  fincore::utc_timestamp(2021, 1, 7),
        fincore::utc_timestamp(2021, 1, 8),  fincore::utc_timestamp(2021, 1, 11),
        fincore::utc_timestamp(2021, 1, 12), fincore::utc_timestamp(2021, 1, 13),
        fincore::utc_timestamp(2021, 1, 14), fincore::utc_timestamp(2021, 1, 15),
        fincore::utc_timestamp(2021, 1, 18), fincore::utc_timestamp(2021, 1, 19),
    };
}

fincore::dataframe drawdown_fixture() {
    return fincore::make_series(
        business_day_index(), "returns",
        {0.02, -0.05, 0.01, 0.06, 0.01, -0.03, -0.04, 0.05, 0.02, 0.03, -0.02,
         0.01});
}

TEST(Drawdown, TopDrawdownsMatchPythonOracleOrderAndRecovery) {
    const auto windows = fincore::get_top_drawdowns(drawdown_fixture(), 3);

    ASSERT_EQ(windows.size(), 3U);
    EXPECT_EQ(windows[0].peak, fincore::utc_timestamp(2021, 1, 8));
    EXPECT_EQ(windows[0].valley, fincore::utc_timestamp(2021, 1, 12));
    ASSERT_TRUE(windows[0].recovery.has_value());
    EXPECT_EQ(*windows[0].recovery, fincore::utc_timestamp(2021, 1, 15));
    EXPECT_EQ(windows[1].peak, fincore::utc_timestamp(2021, 1, 4));
    EXPECT_EQ(windows[1].valley, fincore::utc_timestamp(2021, 1, 5));
    ASSERT_TRUE(windows[1].recovery.has_value());
    EXPECT_EQ(*windows[1].recovery, fincore::utc_timestamp(2021, 1, 7));
    EXPECT_EQ(windows[2].peak, fincore::utc_timestamp(2021, 1, 15));
    EXPECT_EQ(windows[2].valley, fincore::utc_timestamp(2021, 1, 18));
    EXPECT_FALSE(windows[2].recovery.has_value());

    // Ten asked for but three found: this API stops at the episodes that
    // exist, unlike gen_drawdown_table, which always returns the asked size.
    EXPECT_EQ(fincore::get_top_drawdowns(drawdown_fixture(), 10).size(), 3U);
}

TEST(Drawdown, DrawdownTableMatchesPythonOracleAndPadsRows) {
    const auto table = fincore::gen_drawdown_table(drawdown_fixture(), 3);

    ASSERT_EQ(table.size(), 3U);
    EXPECT_NEAR(table[0].net_drawdown_percent, 6.880000000000005, 1e-12);
    EXPECT_EQ(table[0].peak, fincore::utc_timestamp(2021, 1, 8));
    EXPECT_EQ(table[0].valley, fincore::utc_timestamp(2021, 1, 12));
    ASSERT_TRUE(table[0].recovery.has_value());
    ASSERT_TRUE(table[0].duration_business_days.has_value());
    EXPECT_EQ(*table[0].duration_business_days, 6U);
    EXPECT_NEAR(table[1].net_drawdown_percent, 5.000000000000004, 1e-12);
    ASSERT_TRUE(table[1].duration_business_days.has_value());
    EXPECT_EQ(*table[1].duration_business_days, 4U);
    EXPECT_NEAR(table[2].net_drawdown_percent, 2.0000000000000093, 1e-12);
    EXPECT_FALSE(table[2].recovery.has_value());
    EXPECT_FALSE(table[2].duration_business_days.has_value());

    const auto monotone = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2021, 1, 4),
                                        fincore::utc_timestamp(2021, 1, 5),
                                        fincore::utc_timestamp(2021, 1, 6)},
        "returns", {0.01, 0.02, 0.03});
    const auto padded = fincore::gen_drawdown_table(monotone, 3);
    ASSERT_EQ(padded.size(), 3U);
    for (const auto& row : padded) {
        EXPECT_TRUE(std::isnan(row.net_drawdown_percent));
        EXPECT_FALSE(row.peak.has_value());
        EXPECT_FALSE(row.valley.has_value());
        EXPECT_FALSE(row.recovery.has_value());
        EXPECT_FALSE(row.duration_business_days.has_value());
    }

    const auto unrecovered = fincore::make_series(
        std::vector<fincore::timestamp>{
            fincore::utc_timestamp(2021, 1, 4),
            fincore::utc_timestamp(2021, 1, 5),
            fincore::utc_timestamp(2021, 1, 6),
            fincore::utc_timestamp(2021, 1, 7)},
        "returns", {0.02, -0.10, 0.01, -0.02});
    const auto unrecovered_table =
        fincore::gen_drawdown_table(unrecovered, 2);
    ASSERT_EQ(unrecovered_table.size(), 2U);
    EXPECT_NEAR(unrecovered_table[0].net_drawdown_percent, 10.918, 1e-12);
    EXPECT_FALSE(unrecovered_table[0].recovery.has_value());
    EXPECT_FALSE(unrecovered_table[0].duration_business_days.has_value());
    EXPECT_TRUE(std::isnan(unrecovered_table[1].net_drawdown_percent));
}

TEST(Drawdown, MaxDrawdownPeriodMatchesPythonOracle) {
    const auto period = fincore::max_drawdown_period(drawdown_fixture());
    ASSERT_TRUE(period.first.has_value());
    ASSERT_TRUE(period.second.has_value());
    EXPECT_EQ(*period.first, fincore::utc_timestamp(2021, 1, 8));
    EXPECT_EQ(*period.second, fincore::utc_timestamp(2021, 1, 12));

    const auto monotone = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2021, 1, 4),
                                        fincore::utc_timestamp(2021, 1, 5),
                                        fincore::utc_timestamp(2021, 1, 6)},
        "returns", {0.01, 0.02, 0.03});
    const auto flat_period = fincore::max_drawdown_period(monotone);
    ASSERT_TRUE(flat_period.first.has_value());
    EXPECT_EQ(*flat_period.first, fincore::utc_timestamp(2021, 1, 4));
    EXPECT_EQ(*flat_period.second, fincore::utc_timestamp(2021, 1, 4));

    const auto empty = fincore::make_series({}, "returns", {});
    const auto empty_period = fincore::max_drawdown_period(empty);
    EXPECT_FALSE(empty_period.first.has_value());
    EXPECT_FALSE(empty_period.second.has_value());
}

TEST(Drawdown, DrawdownRankApisValidateSelectorsAndOwnTheirResults) {
    const auto strategy = fincore::make_series(
        business_day_index(), "strategy",
        {0.02, -0.05, 0.01, 0.06, 0.01, -0.03, -0.04, 0.05, 0.02, 0.03, -0.02,
         0.01});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};

    EXPECT_EQ(fincore::get_top_drawdowns(strategy, 3, bounded_selector).size(),
              3U);
    EXPECT_THROW(static_cast<void>(
                     fincore::get_top_drawdowns(strategy, 3,
                                                embedded_selector)),
                 fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::gen_drawdown_table(strategy, 3, "")),
        fincore::input_error);

    const auto owned = [] {
        const auto frame = drawdown_fixture();
        return fincore::get_top_drawdowns(frame, 3);
    }();
    ASSERT_EQ(owned.size(), 3U);
    EXPECT_EQ(owned[0].valley, fincore::utc_timestamp(2021, 1, 12));
}

}  // namespace
