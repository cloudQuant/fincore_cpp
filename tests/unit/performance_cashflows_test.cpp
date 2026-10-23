#include <fincore/calendar.hpp>
#include <fincore/performance/cashflows.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<fincore::timestamp> cashflow_dates() {
    return {fincore::utc_timestamp(2021, 1, 1),
            fincore::utc_timestamp(2021, 1, 2),
            fincore::utc_timestamp(2021, 1, 3),
            fincore::utc_timestamp(2021, 1, 4),
            fincore::utc_timestamp(2021, 1, 5)};
}

fincore::dataframe valuations_fixture() {
    return fincore::make_series(cashflow_dates(), "value",
                                {100.0, 110.0, 105.0, 120.0, 125.0});
}

fincore::dataframe ledger_fixture() {
    return fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 3),
         fincore::utc_timestamp(2021, 1, 4)},
        "amount", {10.0, -5.0, 7.0});
}

fincore::dataframe ledger_timing(double start, double end, double start_value) {
    auto frame = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 3),
         fincore::utc_timestamp(2021, 1, 4)},
        "amount", {start, end, start_value});
    frame.load_column<double>(
        "timing", std::vector<double>{0.0, 0.0, 1.0},
        hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(PerformanceCashflows, MatchesPythonOracleForLedgerPaths) {
    const auto valuations = valuations_fixture();
    const auto ledger = ledger_timing(10.0, -5.0, 7.0);

    const auto returns =
        fincore::cashflow_adjusted_returns(valuations, ledger);
    ASSERT_EQ(returns.get_index().size(), 4U);
    const auto& values = returns.get_column<double>("returns");
    EXPECT_NEAR(values[0], 0.0, 1e-12);
    EXPECT_NEAR(values[1], 0.0, 1e-12);
    EXPECT_NEAR(values[2], 0.0714285714285714, 1e-12);
    EXPECT_NEAR(values[3], 0.04166666666666674, 1e-12);
    EXPECT_NEAR(fincore::cashflow_adjusted_twr(valuations, ledger),
                0.1160714285714286, 1e-12);
}

TEST(PerformanceCashflows, GrossFeeTreatmentMatchesPythonOracle) {
    const auto valuations = valuations_fixture();
    const auto ledger = ledger_timing(10.0, -5.0, 7.0);
    const auto fees = fincore::make_series(
        cashflow_dates(), "fees", {0.0, 0.5, 0.0, 0.0, 0.0});

    const auto returns = fincore::cashflow_adjusted_returns(
        valuations, ledger, fees, fincore::fee_treatment::gross);
    const auto& values = returns.get_column<double>("returns");
    EXPECT_NEAR(values[0], 0.004999999999999893, 1e-12);
    EXPECT_NEAR(values[1], 0.0, 1e-12);
    EXPECT_NEAR(fincore::cashflow_adjusted_twr(
                    valuations, ledger, fees,
                    fincore::fee_treatment::gross),
                0.12165178571428581, 1e-12);

    EXPECT_NEAR(fincore::cashflow_adjusted_twr(
                    valuations, ledger, fees,
                    fincore::fee_treatment::net),
                0.1160714285714286, 1e-12);
}

fincore::dataframe uniform_ledger(double timing) {
    auto frame = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 3),
         fincore::utc_timestamp(2021, 1, 4)},
        "amount", {10.0, -5.0, 7.0});
    frame.load_column<double>(
        "timing", std::vector<double>{timing, timing, timing},
        hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(PerformanceCashflows, UniformSeriesTimingAndSameDayAggregation) {
    const auto valuations = valuations_fixture();

    const auto end_returns = fincore::cashflow_adjusted_returns(
        valuations, uniform_ledger(0.0));
    const auto& end_values = end_returns.get_column<double>("returns");
    EXPECT_NEAR(end_values[2], 0.07619047619047614, 1e-12);
    EXPECT_NEAR(fincore::cashflow_adjusted_twr(valuations,
                                               uniform_ledger(0.0)),
                0.12103174603174605, 1e-12);

    const auto start_returns = fincore::cashflow_adjusted_returns(
        valuations, uniform_ledger(1.0));
    const auto& start_values = start_returns.get_column<double>("returns");
    EXPECT_NEAR(start_values[2], 0.0714285714285714, 1e-12);
    EXPECT_NEAR(fincore::cashflow_adjusted_twr(valuations,
                                               uniform_ledger(1.0)),
                0.1160714285714286, 1e-12);

    auto same_day = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 2)},
        "amount", {6.0, 4.0});
    same_day.load_column<double>("timing", std::vector<double>{0.0, 0.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
    const auto aggregated =
        fincore::cashflow_adjusted_returns(valuations, same_day);
    const auto& aggregated_values =
        aggregated.get_column<double>("returns");
    EXPECT_NEAR(aggregated_values[0], 0.0, 1e-12);
    EXPECT_NEAR(fincore::cashflow_adjusted_twr(valuations, same_day),
                0.13636363636363646, 1e-12);
}

TEST(PerformanceCashflows, PreservesTerminalLossAndValidationErrors) {
    const auto terminal_zero = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 1),
         fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 3)},
        "value", {100.0, 110.0, 0.0});
    const auto returns = fincore::cashflow_adjusted_returns(terminal_zero);
    const auto& values = returns.get_column<double>("returns");
    EXPECT_NEAR(values[0], 0.10000000000000009, 1e-12);
    EXPECT_DOUBLE_EQ(values[1], -1.0);
    EXPECT_NEAR(fincore::cashflow_adjusted_twr(terminal_zero,
                                               std::nullopt, std::nullopt,
                                               fincore::fee_treatment::net),
                -1.0, 1e-12);

    auto first_flow = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 1)}, "amount", {1.0});
    first_flow.load_column<double>("timing", std::vector<double>{0.0},
                                   hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::cashflow_adjusted_returns(
                     valuations_fixture(), first_flow)),
                 fincore::input_error);

    const auto first_fee = fincore::make_series(
        cashflow_dates(), "fees", {0.5, 0.0, 0.0, 0.0, 0.0});
    EXPECT_THROW(static_cast<void>(fincore::cashflow_adjusted_returns(
                     valuations_fixture(), std::nullopt, first_fee)),
                 fincore::input_error);

    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::cashflow_adjusted_returns(
                     valuations_fixture(), ledger_fixture(), std::nullopt,
                     fincore::fee_treatment::net, embedded_selector)),
                 fincore::input_error);
}

}  // namespace
