#include <fincore/calendar.hpp>
#include <fincore/error.hpp>
#include <fincore/metrics/consecutive.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
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

}  // namespace
