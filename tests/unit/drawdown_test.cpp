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

}  // namespace
