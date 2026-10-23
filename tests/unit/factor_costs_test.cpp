#include <fincore/calendar.hpp>
#include <fincore/factor/costs.hpp>
#include <fincore/factor/exceptions.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

fincore::dataframe volume_fixture() {
    std::vector<fincore::timestamp> index;
    for (int position = 0; position < 3; ++position) {
        index.push_back(fincore::utc_timestamp(2022, 1, 3) +
                        position * 86'400'000'000'000LL);
    }
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>("A", std::vector<double>{1e6, 1e6, 1e6},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("B", std::vector<double>{2e6, 2e6, 2e6},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

std::vector<fincore::factor_weight_row> long_weights() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    return {{start, "A", 0.6}, {start, "B", 0.4},
            {start + day, "A", 0.5}, {start + day, "B", 0.5},
            {start + 2 * day, "A", 0.0}, {start + 2 * day, "B", 1.0}};
}

std::vector<fincore::factor_weight_row> short_weights() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    return {{start, "A", -0.5}, {start, "B", 0.5},
            {start + day, "A", -0.5}, {start + day, "B", 0.5},
            {start + 2 * day, "A", -0.5}, {start + 2 * day, "B", 0.5}};
}

fincore::factor_cost_model cost_model() {
    return {2.0, 0.1, 0.1, 0.5};
}

TEST(FactorCosts, CapacityMatchesOracle) {
    const auto capacity = fincore::estimate_factor_capacity(
        long_weights(), volume_fixture(), 0.1);
    EXPECT_NEAR(capacity.maximum_portfolio_value, 166666.6666666667, 1e-9);
    ASSERT_EQ(capacity.maximum_portfolio_value_by_date.size(), 3U);
    EXPECT_NEAR(capacity.maximum_portfolio_value_by_date[0],
                166666.6666666667, 1e-9);
    EXPECT_NEAR(capacity.maximum_portfolio_value_by_date[1],
                1000000.0000000002, 1e-9);
    EXPECT_NEAR(capacity.maximum_portfolio_value_by_date[2], 200000.0, 1e-9);
    EXPECT_EQ(capacity.binding_date, fincore::utc_timestamp(2022, 1, 3));
    EXPECT_EQ(capacity.binding_asset, "A");
    ASSERT_EQ(capacity.trade_weights.size(), 6U);
    EXPECT_NEAR(capacity.trade_weights[0], 0.6, 1e-15);
    EXPECT_NEAR(capacity.trade_weights[1], 0.4, 1e-15);
    EXPECT_NEAR(capacity.trade_weights[2], 0.1, 1e-15);
    EXPECT_NEAR(capacity.trade_weights[3], 0.1, 1e-15);
    EXPECT_NEAR(capacity.trade_weights[4], 0.5, 1e-15);
    EXPECT_NEAR(capacity.trade_weights[5], 0.5, 1e-15);
}

TEST(FactorCosts, CostLedgerMatchesOracle) {
    const std::vector<double> gross{0.01, -0.005, 0.002};
    const auto result = fincore::apply_factor_costs(
        gross, long_weights(), volume_fixture(), 100000.0, cost_model());
    ASSERT_EQ(result.net_returns.size(), 3U);
    EXPECT_NEAR(result.net_returns[0], -0.010553792706191447, 1e-15);
    EXPECT_NEAR(result.net_returns[1], -0.006747106781186547, 1e-15);
    EXPECT_NEAR(result.net_returns[2], -0.017286034037919895, 1e-15);
    EXPECT_NEAR(result.turnover[0], 0.5, 1e-15);
    EXPECT_NEAR(result.turnover[1], 0.09999999999999998, 1e-15);
    EXPECT_NEAR(result.turnover[2], 0.5, 1e-15);
    EXPECT_NEAR(result.spread_cost[0], 0.0002, 1e-15);
    EXPECT_NEAR(result.spread_cost[1], 3.999999999999999e-05, 1e-15);
    EXPECT_NEAR(result.impact_cost[0], 0.020353792706191448, 1e-15);
    EXPECT_NEAR(result.impact_cost[1], 0.0017071067811865472, 1e-15);
    EXPECT_NEAR(result.impact_cost[2], 0.019086034037919898, 1e-15);
    EXPECT_DOUBLE_EQ(result.borrow_cost[0], 0.0);
    EXPECT_NEAR(result.total_cost[0], 0.020553792706191447, 1e-15);
    EXPECT_NEAR(result.net_returns[0],
                result.gross_returns[0] - result.total_cost[0], 1e-18);
    ASSERT_EQ(result.participation.size(), 6U);
    EXPECT_NEAR(result.participation[0], 0.06, 1e-15);
    EXPECT_NEAR(result.participation[1], 0.02, 1e-15);
    EXPECT_NEAR(result.participation[2], 0.01, 1e-15);
    EXPECT_NEAR(result.participation[3], 0.005, 1e-15);
    EXPECT_NEAR(result.capacity.maximum_portfolio_value,
                166666.6666666667, 1e-9);
}

TEST(FactorCosts, ShortBorrowLedgerMatchesOracle) {
    const std::vector<double> gross{0.01, -0.005, 0.002};
    fincore::dataframe rates;
    auto rate_index = volume_fixture().get_index();
    std::vector<fincore::timestamp> rate_dates(rate_index.begin(),
                                               rate_index.end());
    rates.load_index(std::move(rate_dates));
    rates.load_column<double>("A", std::vector<double>{0.001, 0.001, 0.001},
                              hmdf::nan_policy::dont_pad_with_nans);
    rates.load_column<double>("B", std::vector<double>{0.0, 0.0, 0.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe available;
    auto available_index = volume_fixture().get_index();
    std::vector<fincore::timestamp> available_dates(available_index.begin(),
                                                    available_index.end());
    available.load_index(std::move(available_dates));
    available.load_column<double>("A", std::vector<double>{1.0, 1.0, 1.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    available.load_column<double>("B", std::vector<double>{1.0, 1.0, 1.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    const auto result = fincore::apply_factor_costs(
        gross, short_weights(), volume_fixture(), 100000.0, cost_model(),
        &rates, &available);
    EXPECT_NEAR(result.borrow_cost[0], 0.0005, 1e-15);
    EXPECT_NEAR(result.borrow_cost[1], 0.0005, 1e-15);
    EXPECT_NEAR(result.net_returns[0], -0.009786034037919897, 1e-15);
    EXPECT_NEAR(result.net_returns[1], -0.0055, 1e-15);
    EXPECT_NEAR(result.net_returns[2], 0.0015, 1e-15);
    EXPECT_NEAR(result.total_cost[0], 0.019786034037919897, 1e-15);
}

TEST(FactorCosts, RejectsInvalidLedgers) {
    const std::vector<double> gross{0.01, -0.005, 0.002};
    EXPECT_THROW(static_cast<void>(fincore::apply_factor_costs(
                     std::vector<double>{0.01, -0.005}, long_weights(),
                     volume_fixture(), 100000.0, cost_model())),
                 fincore::factor_data_error);
    EXPECT_THROW(static_cast<void>(fincore::apply_factor_costs(
                     gross, long_weights(), volume_fixture(), 1000000.0,
                     cost_model())),
                 fincore::factor_data_error);
    EXPECT_THROW(static_cast<void>(fincore::apply_factor_costs(
                     gross, short_weights(), volume_fixture(), 100000.0,
                     cost_model())),
                 fincore::factor_data_error);
    EXPECT_THROW(static_cast<void>(fincore::apply_factor_costs(
                     gross, long_weights(), volume_fixture(), 100000.0,
                     fincore::factor_cost_model{2.0, 0.1, 0.0, 0.5})),
                 fincore::factor_data_error);
    auto bad_weights = long_weights();
    bad_weights[0].weight = 0.7;
    EXPECT_THROW(static_cast<void>(fincore::estimate_factor_capacity(
                     bad_weights, volume_fixture(), 0.1)),
                 fincore::factor_data_error);
}

}  // namespace
