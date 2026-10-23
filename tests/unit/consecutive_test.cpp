#include <fincore/calendar.hpp>
#include <fincore/error.hpp>
#include <fincore/metrics/consecutive.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <optional>
#include <type_traits>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double tolerance = 1.0e-12;

std::vector<fincore::timestamp> fixture_dates() {
    std::vector<fincore::timestamp> dates;
    dates.reserve(12U);
    for (unsigned day = 1U; day <= 12U; ++day) {
        dates.push_back(fincore::utc_timestamp(2021, 2, day));
    }
    return dates;
}

TEST(Consecutive, MatchesDailyPythonGoldensAndFirstDates) {
    const auto dates = fixture_dates();
    const auto returns = fincore::make_series(
        dates, "returns",
        {.01, .02, .03, 0.0, -.02, -.03, .01, .02, -.04, -.05, -.06,
         .01});

    const auto stats = fincore::daily_consecutive_stats(returns);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_up_days, 3.0);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_down_days, 3.0);
    EXPECT_NEAR(stats.max_consecutive_gain, .06, tolerance);
    EXPECT_NEAR(stats.max_consecutive_loss, -.15, tolerance);

    EXPECT_DOUBLE_EQ(fincore::max_consecutive_up_days(returns), 3.0);
    EXPECT_DOUBLE_EQ(fincore::max_consecutive_down_days(returns), 3.0);
    EXPECT_NEAR(fincore::max_consecutive_gain(returns), .06, tolerance);
    EXPECT_NEAR(fincore::max_consecutive_loss(returns), -.15, tolerance);
    EXPECT_NEAR(fincore::max_single_day_gain(returns), .03, tolerance);
    EXPECT_NEAR(fincore::max_single_day_loss(returns), -.06, tolerance);

    EXPECT_EQ(fincore::max_single_day_gain_date(returns),
              std::optional<fincore::timestamp>{dates[2]});
    EXPECT_EQ(fincore::max_single_day_loss_date(returns),
              std::optional<fincore::timestamp>{dates[10]});
    EXPECT_EQ(fincore::max_consecutive_up_start_date(returns),
              std::optional<fincore::timestamp>{dates[0]});
    EXPECT_EQ(fincore::max_consecutive_up_end_date(returns),
              std::optional<fincore::timestamp>{dates[2]});
    EXPECT_EQ(fincore::max_consecutive_down_start_date(returns),
              std::optional<fincore::timestamp>{dates[8]});
    EXPECT_EQ(fincore::max_consecutive_down_end_date(returns),
              std::optional<fincore::timestamp>{dates[10]});
}

TEST(Consecutive, ZerosAndNanBreakBothRunKinds) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9}, "returns",
        {.01, .02, nan, .03, 0.0, -.02, -.03, nan, -.04});

    const auto stats = fincore::daily_consecutive_stats(returns);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_up_days, 2.0);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_down_days, 2.0);
    EXPECT_NEAR(stats.max_consecutive_gain, .03, tolerance);
    EXPECT_NEAR(stats.max_consecutive_loss, -.05, tolerance);
    EXPECT_EQ(fincore::max_consecutive_up_start_date(returns),
              std::optional<fincore::timestamp>{1});
    EXPECT_EQ(fincore::max_consecutive_up_end_date(returns),
              std::optional<fincore::timestamp>{2});
    EXPECT_EQ(fincore::max_consecutive_down_start_date(returns),
              std::optional<fincore::timestamp>{6});
    EXPECT_EQ(fincore::max_consecutive_down_end_date(returns),
              std::optional<fincore::timestamp>{7});
    EXPECT_NEAR(fincore::max_single_day_gain(returns), .03, tolerance);
    EXPECT_NEAR(fincore::max_single_day_loss(returns), -.04, tolerance);
    EXPECT_EQ(fincore::max_single_day_gain_date(returns),
              std::optional<fincore::timestamp>{4});
    EXPECT_EQ(fincore::max_single_day_loss_date(returns),
              std::optional<fincore::timestamp>{9});
}

TEST(Consecutive, TreatsSignedInfinityAsOrdinaryCandidates) {
    const auto infinity = std::numeric_limits<double>::infinity();
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7}, "returns",
        {infinity, 1.0, 0.0, -infinity, -1.0, nan, 0.0});

    const auto stats = fincore::daily_consecutive_stats(returns);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_up_days, 2.0);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_down_days, 2.0);
    EXPECT_EQ(stats.max_consecutive_gain, infinity);
    EXPECT_EQ(stats.max_consecutive_loss, -infinity);
    EXPECT_EQ(fincore::max_single_day_gain(returns), infinity);
    EXPECT_EQ(fincore::max_single_day_loss(returns), -infinity);
    EXPECT_EQ(fincore::max_single_day_gain_date(returns),
              std::optional<fincore::timestamp>{1});
    EXPECT_EQ(fincore::max_single_day_loss_date(returns),
              std::optional<fincore::timestamp>{4});
    EXPECT_EQ(fincore::max_consecutive_up_start_date(returns),
              std::optional<fincore::timestamp>{1});
    EXPECT_EQ(fincore::max_consecutive_up_end_date(returns),
              std::optional<fincore::timestamp>{2});
    EXPECT_EQ(fincore::max_consecutive_down_start_date(returns),
              std::optional<fincore::timestamp>{4});
    EXPECT_EQ(fincore::max_consecutive_down_end_date(returns),
              std::optional<fincore::timestamp>{5});
}

TEST(Consecutive, UsesEarliestRunsAndSamplesForEqualTies) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {.01, .01, 0.0, .01, .01, 0.0, -.02, -.02, 0.0, -.02});

    EXPECT_DOUBLE_EQ(fincore::max_consecutive_up_days(returns), 2.0);
    EXPECT_DOUBLE_EQ(fincore::max_consecutive_down_days(returns), 2.0);
    EXPECT_NEAR(fincore::max_consecutive_gain(returns), .02, tolerance);
    EXPECT_NEAR(fincore::max_consecutive_loss(returns), -.04, tolerance);
    EXPECT_EQ(fincore::max_single_day_gain_date(returns),
              std::optional<fincore::timestamp>{1});
    EXPECT_EQ(fincore::max_single_day_loss_date(returns),
              std::optional<fincore::timestamp>{7});
    EXPECT_EQ(fincore::max_consecutive_up_start_date(returns),
              std::optional<fincore::timestamp>{1});
    EXPECT_EQ(fincore::max_consecutive_up_end_date(returns),
              std::optional<fincore::timestamp>{2});
    EXPECT_EQ(fincore::max_consecutive_down_start_date(returns),
              std::optional<fincore::timestamp>{7});
    EXPECT_EQ(fincore::max_consecutive_down_end_date(returns),
              std::optional<fincore::timestamp>{8});
}

TEST(Consecutive, EmptyInputsReturnNanAndNoDates) {
    const auto empty = fincore::make_series({}, "returns", {});

    const auto stats = fincore::daily_consecutive_stats(empty);
    EXPECT_TRUE(std::isnan(stats.max_consecutive_up_days));
    EXPECT_TRUE(std::isnan(stats.max_consecutive_down_days));
    EXPECT_TRUE(std::isnan(stats.max_consecutive_gain));
    EXPECT_TRUE(std::isnan(stats.max_consecutive_loss));
    EXPECT_TRUE(std::isnan(fincore::max_consecutive_up_days(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_consecutive_down_days(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_consecutive_gain(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_consecutive_loss(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_single_day_gain(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_single_day_loss(empty)));
    EXPECT_EQ(fincore::max_single_day_gain_date(empty), std::nullopt);
    EXPECT_EQ(fincore::max_single_day_loss_date(empty), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_up_start_date(empty), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_up_end_date(empty), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_down_start_date(empty), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_down_end_date(empty), std::nullopt);
}

TEST(Consecutive, ZeroOnlyInputsHaveNoRunsButKeepFirstSingleDayDates) {
    const auto returns =
        fincore::make_series({11, 12}, "returns", {0.0, 0.0});

    const auto stats = fincore::daily_consecutive_stats(returns);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_up_days, 0.0);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_down_days, 0.0);
    EXPECT_TRUE(std::isnan(stats.max_consecutive_gain));
    EXPECT_TRUE(std::isnan(stats.max_consecutive_loss));
    EXPECT_DOUBLE_EQ(fincore::max_single_day_gain(returns), 0.0);
    EXPECT_DOUBLE_EQ(fincore::max_single_day_loss(returns), 0.0);
    EXPECT_EQ(fincore::max_single_day_gain_date(returns),
              std::optional<fincore::timestamp>{11});
    EXPECT_EQ(fincore::max_single_day_loss_date(returns),
              std::optional<fincore::timestamp>{11});
    EXPECT_EQ(fincore::max_consecutive_up_start_date(returns), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_up_end_date(returns), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_down_start_date(returns),
              std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_down_end_date(returns),
              std::nullopt);
}

TEST(Consecutive, AllNanInputsHaveNoSingleDayOrRunDates) {
    const auto returns = fincore::make_series({1, 2}, "returns", {nan, nan});

    const auto stats = fincore::daily_consecutive_stats(returns);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_up_days, 0.0);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_down_days, 0.0);
    EXPECT_TRUE(std::isnan(stats.max_consecutive_gain));
    EXPECT_TRUE(std::isnan(stats.max_consecutive_loss));
    EXPECT_TRUE(std::isnan(fincore::max_single_day_gain(returns)));
    EXPECT_TRUE(std::isnan(fincore::max_single_day_loss(returns)));
    EXPECT_EQ(fincore::max_single_day_gain_date(returns), std::nullopt);
    EXPECT_EQ(fincore::max_single_day_loss_date(returns), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_up_start_date(returns), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_up_end_date(returns), std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_down_start_date(returns),
              std::nullopt);
    EXPECT_EQ(fincore::max_consecutive_down_end_date(returns),
              std::nullopt);
}

TEST(Consecutive, PreservesSuppliedRowOrderInsteadOfSortingTimestamps) {
    const auto returns = fincore::make_series(
        {30, 10, 40, 20}, "returns", {.01, .02, 0.0, .03});

    EXPECT_DOUBLE_EQ(fincore::max_consecutive_up_days(returns), 2.0);
    EXPECT_EQ(fincore::max_consecutive_up_start_date(returns),
              std::optional<fincore::timestamp>{30});
    EXPECT_EQ(fincore::max_consecutive_up_end_date(returns),
              std::optional<fincore::timestamp>{10});
}

TEST(Consecutive, SupportsNamedColumns) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4}, "strategy", {.01, .02, -.03, -.04});

    const auto stats = fincore::daily_consecutive_stats(returns, "strategy");
    EXPECT_DOUBLE_EQ(stats.max_consecutive_up_days, 2.0);
    EXPECT_DOUBLE_EQ(stats.max_consecutive_down_days, 2.0);
    EXPECT_NEAR(fincore::max_consecutive_gain(returns, "strategy"), .03,
                tolerance);
    EXPECT_NEAR(fincore::max_consecutive_loss(returns, "strategy"), -.07,
                tolerance);
    EXPECT_EQ(fincore::max_single_day_gain_date(returns, "strategy"),
              std::optional<fincore::timestamp>{2});
    EXPECT_EQ(fincore::max_single_day_loss_date(returns, "strategy"),
              std::optional<fincore::timestamp>{4});
    EXPECT_EQ(fincore::max_consecutive_up_start_date(returns, "strategy"),
              std::optional<fincore::timestamp>{1});
    EXPECT_EQ(fincore::max_consecutive_down_end_date(returns, "strategy"),
              std::optional<fincore::timestamp>{4});
}

TEST(Consecutive, RejectsMissingEmptyWrongTypedAndShortColumns) {
    fincore::dataframe missing;
    const auto empty = fincore::make_series({}, "returns", {});

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe short_column;
    short_column.load_index(std::vector<fincore::timestamp>{1, 2});
    short_column.load_column<double>("returns", std::vector<double>{.01},
                                     hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(fincore::daily_consecutive_stats(missing),
                 fincore::input_error);
    EXPECT_THROW(fincore::max_consecutive_up_days(empty, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::max_single_day_loss(wrong_type),
                 fincore::input_error);
    EXPECT_THROW(fincore::max_consecutive_down_end_date(short_column),
                 fincore::input_error);
}

static_assert(std::is_same_v<
              decltype(fincore::daily_consecutive_statistics::max_consecutive_up_days),
              double>);
static_assert(std::is_same_v<
              decltype(fincore::daily_consecutive_statistics::max_consecutive_down_days),
              double>);
static_assert(std::is_same_v<
              decltype(fincore::daily_consecutive_statistics::max_consecutive_gain),
              double>);
static_assert(std::is_same_v<
              decltype(fincore::daily_consecutive_statistics::max_consecutive_loss),
              double>);

std::vector<fincore::timestamp> daily_index_range(int year, int month, int day,
                                                 int count) {
    constexpr std::int64_t nanoseconds_per_day = 86'400'000'000'000LL;
    std::vector<fincore::timestamp> index;
    index.reserve(static_cast<std::size_t>(count));
    const fincore::timestamp start = fincore::utc_timestamp(year, month, day);
    for (int position = 0; position < count; ++position) {
        index.push_back(start + position * nanoseconds_per_day);
    }
    return index;
}

TEST(Consecutive, WeeklyAndMonthlyRunsMatchPythonOracle) {
    // 2020-11-01 through 2021-03-31 daily: each month has a constant sign.
    const int count = 151;
    std::vector<double> month_values;
    month_values.reserve(static_cast<std::size_t>(count));
    for (int position = 0; position < count; ++position) {
        const auto day = fincore::utc_timestamp(2020, 11, 1) +
                         position * 86'400'000'000'000LL;
        const int year = fincore::utc_year(day);
        const auto ymd = std::chrono::year_month_day{
            std::chrono::sys_days{std::chrono::days{day / 86'400'000'000'000LL}}};
        const int month_value = static_cast<int>(static_cast<unsigned>(ymd.month()));
        double value = 0.0;
        if (year == 2020 && month_value == 11) {
            value = 0.01;
        } else if (year == 2020 && month_value == 12) {
            value = -0.01;
        } else if (year == 2021 && month_value == 1) {
            value = 0.01;
        } else if (year == 2021 && month_value == 2) {
            value = 0.01;
        } else {
            value = -0.01;
        }
        month_values.push_back(value);
    }
    const auto monthly = fincore::make_series(
        daily_index_range(2020, 11, 1, count), "returns", month_values);

    EXPECT_DOUBLE_EQ(fincore::max_consecutive_up_months(monthly), 2.0);
    EXPECT_DOUBLE_EQ(fincore::max_consecutive_down_months(monthly), 1.0);
    EXPECT_DOUBLE_EQ(fincore::max_consecutive_up_weeks(monthly), 8.0);
    EXPECT_DOUBLE_EQ(fincore::max_consecutive_down_weeks(monthly), 5.0);

    constexpr int weekly_count = 42;  // 2021-01-04 .. 2021-02-14
    std::vector<double> weekly_values;
    weekly_values.reserve(static_cast<std::size_t>(weekly_count));
    for (int position = 0; position < weekly_count; ++position) {
        const int iso_week = 1 + position / 7;
        const bool positive =
            iso_week == 1 || iso_week == 2 || iso_week == 4;
        weekly_values.push_back(positive ? 0.01 : -0.01);
    }
    const auto weekly = fincore::make_series(
        daily_index_range(2021, 1, 4, weekly_count), "returns",
        weekly_values);
    EXPECT_DOUBLE_EQ(fincore::max_consecutive_up_weeks(weekly), 2.0);
    EXPECT_DOUBLE_EQ(fincore::max_consecutive_down_weeks(weekly), 2.0);
}

TEST(Consecutive, WeeklyAndMonthlyRunsHandleEmptyAndSelectors) {
    const auto empty = fincore::make_series({}, "returns", {});
    EXPECT_TRUE(std::isnan(fincore::max_consecutive_up_weeks(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_consecutive_down_weeks(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_consecutive_up_months(empty)));
    EXPECT_TRUE(std::isnan(fincore::max_consecutive_down_months(empty)));

    const auto strategy = fincore::make_series(
        daily_index_range(2021, 1, 4, 14), "strategy",
        {0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, -0.01, -0.01, -0.01,
         -0.01, -0.01, -0.01, -0.01});
    EXPECT_DOUBLE_EQ(fincore::max_consecutive_down_weeks(strategy, "strategy"),
                     1.0);

    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(
        static_cast<void>(
            fincore::max_consecutive_up_weeks(strategy, embedded_selector)),
        fincore::input_error);

    const auto owned = [] {
        const auto frame = fincore::make_series(
            daily_index_range(2021, 1, 4, 14), "returns",
            {0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, -0.01, -0.01, -0.01,
             -0.01, -0.01, -0.01, -0.01});
        return fincore::max_consecutive_up_weeks(frame);
    }();
    EXPECT_DOUBLE_EQ(owned, 1.0);
}

TEST(Consecutive, ConsecutiveStatsComposesAllSixRuns) {
    const std::vector<fincore::timestamp> index{
        fincore::utc_timestamp(2020, 1, 1), fincore::utc_timestamp(2020, 1, 2),
        fincore::utc_timestamp(2020, 1, 3), fincore::utc_timestamp(2020, 1, 4),
        fincore::utc_timestamp(2020, 1, 5), fincore::utc_timestamp(2020, 1, 6),
        fincore::utc_timestamp(2020, 1, 7), fincore::utc_timestamp(2020, 1, 8),
        fincore::utc_timestamp(2020, 1, 9),
        fincore::utc_timestamp(2020, 1, 10)};
    const auto returns = fincore::make_series(
        index, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto stats = fincore::consecutive_stats(returns);

    ASSERT_EQ(stats.size(), 6U);
    EXPECT_EQ(stats.at("max_consecutive_up_days"), 3.0);
    EXPECT_EQ(stats.at("max_consecutive_down_days"), 1.0);
    EXPECT_EQ(stats.at("max_consecutive_up_weeks"), 2.0);
    EXPECT_EQ(stats.at("max_consecutive_down_weeks"), 0.0);
    EXPECT_EQ(stats.at("max_consecutive_up_months"), 1.0);
    EXPECT_EQ(stats.at("max_consecutive_down_months"), 0.0);

    const auto empty = fincore::make_series({}, "returns", {});
    const auto empty_stats = fincore::consecutive_stats(empty);
    EXPECT_TRUE(std::isnan(empty_stats.at("max_consecutive_up_days")));

    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(
        static_cast<void>(
            fincore::consecutive_stats(returns, embedded_selector)),
        fincore::input_error);
}

}  // namespace
