#include <fincore/calendar.hpp>
#include <fincore/portfolio/capacity.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<fincore::timestamp> capacity_index() {
    return {fincore::utc_timestamp(2021, 1, 1),
            fincore::utc_timestamp(2021, 1, 2),
            fincore::utc_timestamp(2021, 1, 3),
            fincore::utc_timestamp(2021, 1, 4),
            fincore::utc_timestamp(2021, 1, 5),
            fincore::utc_timestamp(2021, 1, 6),
            fincore::utc_timestamp(2021, 1, 7)};
}

fincore::dataframe capacity_frame(std::vector<double> first,
                                  std::vector<double> second) {
    fincore::dataframe frame;
    frame.load_index(capacity_index());
    frame.load_column<double>("AAA", std::move(first),
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("BBB", std::move(second),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe single_asset_frame(std::vector<double> values) {
    return fincore::make_series(capacity_index(), "AAA", std::move(values));
}

fincore::capacity_assessment assessment_after_input_mutation_and_destruction() {
    auto positions = capacity_frame(
        {100.0, 120.0, 90.0, 150.0, 160.0, 140.0, 170.0},
        {50.0, 40.0, 60.0, 55.0, 70.0, 80.0, 75.0});
    positions.load_column<double>("cash", std::vector<double>(7, 1000.0),
                                  hmdf::nan_policy::dont_pad_with_nans);
    auto prices = capacity_frame(
        {10.0, 11.0, 12.0, 11.5, 13.0, 12.5, 13.5},
        {20.0, 19.0, 21.0, 22.0, 20.0, 23.0, 22.0});
    auto volumes = capacity_frame(
        {1000.0, 1200.0, 900.0, 1100.0, 1300.0, 1000.0, 1500.0},
        {500.0, 600.0, 550.0, 700.0, 650.0, 800.0, 750.0});
    std::vector<fincore::trade_record> trades{
        {fincore::utc_timestamp(2021, 1, 5), "AAA", 20.0},
        {fincore::utc_timestamp(2021, 1, 5), "BBB", -15.0}};

    fincore::capacity_config config;
    config.mean_volume_window = 2U;
    config.last_n_days = 2U;
    auto assessment =
        fincore::assess_liquidity(positions, trades, prices, volumes, config);

    auto& aaa_positions = positions.get_column<double>("AAA");
    std::fill(aaa_positions.begin(), aaa_positions.end(), -1.0);
    auto& aaa_prices = prices.get_column<double>("AAA");
    std::fill(aaa_prices.begin(), aaa_prices.end(), -1.0);
    auto& aaa_volumes = volumes.get_column<double>("AAA");
    std::fill(aaa_volumes.begin(), aaa_volumes.end(), 0.0);
    trades.clear();
    return assessment;
}

TEST(Capacity, AssessLiquidityComposesAcceptedKernels) {
    auto positions = capacity_frame(
        {100.0, 120.0, 90.0, 150.0, 160.0, 140.0, 170.0},
        {50.0, 40.0, 60.0, 55.0, 70.0, 80.0, 75.0});
    // "cash" is the default cash column name, so it widens the allocation
    // denominator the goldens below were computed against while being excluded
    // from the per-asset output columns.
    positions.load_column<double>("cash", std::vector<double>(7, 1000.0),
                                  hmdf::nan_policy::dont_pad_with_nans);
    const auto prices = capacity_frame(
        {10.0, 11.0, 12.0, 11.5, 13.0, 12.5, 13.5},
        {20.0, 19.0, 21.0, 22.0, 20.0, 23.0, 22.0});
    const auto volumes = capacity_frame(
        {1000.0, 1200.0, 900.0, 1100.0, 1300.0, 1000.0, 1500.0},
        {500.0, 600.0, 550.0, 700.0, 650.0, 800.0, 750.0});
    const std::vector<fincore::trade_record> trades{
        {fincore::utc_timestamp(2021, 1, 2), "AAA", 10.0},
        {fincore::utc_timestamp(2021, 1, 2), "AAA", -30.0},
        {fincore::utc_timestamp(2021, 1, 3), "BBB", 40.0},
        {fincore::utc_timestamp(2021, 1, 5), "AAA", 20.0},
        {fincore::utc_timestamp(2021, 1, 5), "BBB", -15.0}};

    fincore::capacity_config config;
    config.mean_volume_window = 2U;
    config.last_n_days = 2U;
    const auto assessment =
        fincore::assess_liquidity(positions, trades, prices, volumes, config);

    ASSERT_EQ(assessment.liquidation_days.get_index().size(), 5U);
    EXPECT_NEAR(assessment.liquidation_days.get_column<double>("AAA")[2],
                55.47177006951307, 1e-12);
    ASSERT_EQ(assessment.ticker_maximums.size(), 2U);
    EXPECT_EQ(assessment.ticker_maximums[0].symbol, "AAA");
    EXPECT_NEAR(assessment.ticker_maximums[0].days_to_liquidate,
                55.47177006951307, 1e-12);
    EXPECT_NEAR(assessment.ticker_maximums[1].days_to_liquidate,
                23.089355806972986, 1e-12);
    EXPECT_NEAR(assessment.ticker_maximums[1].position_allocation_pct,
                6.557377049180328, 1e-12);
    ASSERT_EQ(assessment.low_liquidity_transactions.size(), 2U);
    EXPECT_EQ(assessment.low_liquidity_transactions[0].date,
              fincore::utc_timestamp(2021, 1, 5));
    EXPECT_NEAR(
        assessment.low_liquidity_transactions[0].max_percent_bar_consumed,
        1.5384615384615385, 1e-12);
    EXPECT_FALSE(assessment.liquidation_days.has_column("cash"));
}

TEST(Capacity, VolumeBoundariesMatchTheDeclaredRollingContract) {
    auto positions = single_asset_frame(std::vector<double>(7, 100.0));
    positions.load_column<double>("cash", std::vector<double>(7, 0.0),
                                  hmdf::nan_policy::dont_pad_with_nans);
    const auto prices = single_asset_frame(std::vector<double>(7, 10.0));
    constexpr double capital_base = 1000.0;
    constexpr double max_bar_consumption = 0.5;
    constexpr std::size_t window = 2U;

    // A zero rolling mean is folded into the same NaN marker as a missing volume,
    // so the figure never prints as infinite days, while the negative mean below
    // stays an ordinary divisor and yields a signed day count.
    const auto zero_volume = single_asset_frame(
        {0.0, 0.0, 100.0, 100.0, 100.0, 100.0, 100.0});
    const auto zero_days = fincore::days_to_liquidate_positions(
        positions, prices, zero_volume, max_bar_consumption, capital_base,
        window);
    ASSERT_EQ(zero_days.get_index().size(), 5U);
    EXPECT_TRUE(std::isnan(zero_days.get_column<double>("AAA")[0]));
    EXPECT_NEAR(zero_days.get_column<double>("AAA")[1], 4.0, 1e-12);

    const auto negative_volume = single_asset_frame(
        {-10.0, -10.0, 100.0, 100.0, 100.0, 100.0, 100.0});
    const auto negative_days = fincore::days_to_liquidate_positions(
        positions, prices, negative_volume, max_bar_consumption, capital_base,
        window);
    EXPECT_NEAR(negative_days.get_column<double>("AAA")[0], -20.0, 1e-12);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto missing_volume = single_asset_frame(
        {nan, 100.0, 100.0, 100.0, 100.0, 100.0, 100.0});
    const auto missing_days = fincore::days_to_liquidate_positions(
        positions, prices, missing_volume, max_bar_consumption, capital_base,
        window);
    EXPECT_TRUE(std::isnan(missing_days.get_column<double>("AAA")[0]));
    EXPECT_NEAR(missing_days.get_column<double>("AAA")[1], 2.0, 1e-12);
}

TEST(Capacity, RejectsMissingMarketColumnsForEveryPositionAsset) {
    const auto positions = capacity_frame(std::vector<double>(7, 100.0),
                                          std::vector<double>(7, 50.0));
    const auto prices_missing_bbb =
        single_asset_frame(std::vector<double>(7, 10.0));
    const auto prices = capacity_frame(std::vector<double>(7, 10.0),
                                       std::vector<double>(7, 20.0));
    const auto volumes_missing_bbb =
        single_asset_frame(std::vector<double>(7, 1000.0));
    const auto volumes = capacity_frame(std::vector<double>(7, 1000.0),
                                        std::vector<double>(7, 500.0));

    EXPECT_ANY_THROW(static_cast<void>(fincore::days_to_liquidate_positions(
        positions, prices_missing_bbb, volumes, 0.2, 1'000'000.0, 2U)));
    EXPECT_ANY_THROW(static_cast<void>(fincore::days_to_liquidate_positions(
        positions, prices, volumes_missing_bbb, 0.2, 1'000'000.0, 2U)));
}

TEST(Capacity, AssessmentOwnsOutputsAfterInputMutationAndDestruction) {
    const auto assessment = assessment_after_input_mutation_and_destruction();

    ASSERT_EQ(assessment.liquidation_days.get_index().size(), 5U);
    EXPECT_NEAR(assessment.liquidation_days.get_column<double>("AAA")[2],
                55.47177006951307, 1e-12);
    ASSERT_EQ(assessment.ticker_maximums.size(), 2U);
    EXPECT_NEAR(assessment.ticker_maximums[0].days_to_liquidate,
                55.47177006951307, 1e-12);
    ASSERT_EQ(assessment.low_liquidity_transactions.size(), 2U);
    EXPECT_EQ(assessment.low_liquidity_transactions[0].date,
              fincore::utc_timestamp(2021, 1, 5));
    EXPECT_NEAR(
        assessment.low_liquidity_transactions[0].max_percent_bar_consumed,
        1.5384615384615385, 1e-12);
}

TEST(Capacity, ValidatesConfigurationLikePythonDataclass) {
    const auto positions = capacity_frame(std::vector<double>(7, 100.0),
                                          std::vector<double>(7, 50.0));
    const auto prices = capacity_frame(std::vector<double>(7, 10.0),
                                       std::vector<double>(7, 20.0));
    const auto volumes = capacity_frame(std::vector<double>(7, 1000.0),
                                        std::vector<double>(7, 500.0));
    const std::vector<fincore::trade_record> no_trades;

    fincore::capacity_config bad_bar;
    bad_bar.max_bar_consumption = 0.0;
    EXPECT_THROW(static_cast<void>(fincore::assess_liquidity(
                     positions, no_trades, prices, volumes, bad_bar)),
                 fincore::input_error);

    fincore::capacity_config bad_capital;
    bad_capital.capital_base = 0.0;
    EXPECT_THROW(static_cast<void>(fincore::assess_liquidity(
                     positions, no_trades, prices, volumes, bad_capital)),
                 fincore::input_error);

    fincore::capacity_config bad_window;
    bad_window.mean_volume_window = 0U;
    EXPECT_THROW(static_cast<void>(fincore::assess_liquidity(
                     positions, no_trades, prices, volumes, bad_window)),
                 fincore::input_error);

    fincore::capacity_config bad_days;
    bad_days.last_n_days = 0U;
    EXPECT_THROW(static_cast<void>(fincore::assess_liquidity(
                     positions, no_trades, prices, volumes, bad_days)),
                 fincore::input_error);
}

}  // namespace
