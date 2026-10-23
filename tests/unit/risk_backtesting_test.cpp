#include <fincore/calendar.hpp>
#include <fincore/risk/backtesting.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

std::vector<fincore::timestamp> backtest_index(int count) {
    std::vector<fincore::timestamp> index;
    index.reserve(static_cast<std::size_t>(count));
    for (int position = 0; position < count; ++position) {
        index.push_back(fincore::utc_timestamp(2021, 1, 1) +
                        position * 86'400'000'000'000LL);
    }
    return index;
}

fincore::dataframe backtest_realized(int count) {
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(count));
    for (int position = 0; position < count; ++position) {
        values.push_back(0.01 * std::sin(static_cast<double>(position) * 0.7));
    }
    return fincore::make_series(backtest_index(count), "realized", values);
}

fincore::dataframe backtest_forecast(int count, double threshold) {
    return fincore::make_series(backtest_index(count), "forecast",
                                std::vector<double>(static_cast<std::size_t>(count),
                                                    threshold));
}

TEST(RiskBacktesting, KupiecAndChristoffersenMatchPythonOracle) {
    EXPECT_NEAR(fincore::kupiec_lr(250, 0, 0.99), 5.025167926750753, 1e-12);
    EXPECT_NEAR(fincore::kupiec_lr(250, 3, 0.99), 0.09494012266442442,
                1e-12);
    EXPECT_NEAR(fincore::kupiec_lr(10, 10, 0.99), 92.10340371976181, 1e-12);
    EXPECT_DOUBLE_EQ(fincore::kupiec_lr(0, 0, 0.99), 0.0);

    const std::vector<int> exceptions{0, 1, 0, 0, 0, 1, 1, 0, 0, 0,
                                      0, 0, 1, 0, 0, 0, 0, 0, 0, 1};
    EXPECT_NEAR(fincore::christoffersen_lr(exceptions),
                0.004560555262997923, 1e-12);
    const std::vector<int> single{1};
    EXPECT_DOUBLE_EQ(fincore::christoffersen_lr(single), 0.0);
}

TEST(RiskBacktesting, VarBacktestMatchesPythonOracleAndStatuses) {
    const auto result = fincore::backtest_var(
        backtest_forecast(250, -0.012), backtest_realized(250), 0.99, 0.05);
    EXPECT_EQ(result.method, fincore::risk_backtest_method::var);
    EXPECT_EQ(result.observations, 250U);
    EXPECT_EQ(result.exceptions, 0U);
    EXPECT_NEAR(result.expected_exceptions, 2.500000000000002, 1e-12);
    EXPECT_DOUBLE_EQ(result.exception_rate, 0.0);
    EXPECT_NEAR(result.kupiec_lr, 5.025167926750753, 1e-12);
    EXPECT_NEAR(result.kupiec_pvalue, 0.02498150305344931, 1e-12);
    EXPECT_DOUBLE_EQ(result.christoffersen_lr, 0.0);
    EXPECT_DOUBLE_EQ(result.christoffersen_pvalue, 1.0);
    EXPECT_EQ(result.status, fincore::risk_backtest_status::inconclusive);
    EXPECT_DOUBLE_EQ(result.diagnostics.at("small_sample"), 1.0);
    EXPECT_EQ(result.inputs_digest.size(), 64U);

    const auto second = fincore::backtest_var(
        backtest_forecast(250, -0.012), backtest_realized(250), 0.99, 0.05);
    EXPECT_EQ(result.inputs_digest, second.inputs_digest);

    const auto failing = fincore::backtest_var(
        backtest_forecast(250, -0.012), backtest_realized(250), 0.9, 0.05);
    EXPECT_EQ(failing.status, fincore::risk_backtest_status::fail);
    EXPECT_EQ(failing.exceptions, 0U);
    EXPECT_NEAR(failing.kupiec_lr, 52.68025782891318, 1e-12);
    EXPECT_NEAR(failing.kupiec_pvalue, 3.925168885725146e-13, 1e-20);

    const auto short_result = fincore::backtest_var(
        backtest_forecast(2, -0.012), backtest_realized(2), 0.99, 0.05);
    EXPECT_EQ(short_result.status,
              fincore::risk_backtest_status::inconclusive);

    EXPECT_THROW(static_cast<void>(fincore::backtest_var(
                     backtest_forecast(10, -0.012), backtest_realized(10),
                     1.0)),
                 fincore::input_error);
}

TEST(RiskBacktesting, EsBacktestMatchesDeterministicOracleFields) {
    const auto result = fincore::backtest_es(
        backtest_forecast(250, -0.012), backtest_realized(250), 0.975, 100U,
        42U);
    EXPECT_EQ(result.method, fincore::risk_backtest_method::es);
    EXPECT_EQ(result.status, fincore::risk_backtest_status::experimental);
    EXPECT_EQ(result.exceptions, 7U);
    EXPECT_NEAR(result.diagnostics.at("realized_es"),
                -0.009935211388974519, 1e-12);
    EXPECT_NEAR(result.diagnostics.at("forecast_es"), -0.012, 1e-12);
    EXPECT_TRUE(std::isfinite(result.diagnostics.at("z_score")));

    const auto second = fincore::backtest_es(
        backtest_forecast(250, -0.012), backtest_realized(250), 0.975, 100U,
        42U);
    EXPECT_DOUBLE_EQ(result.diagnostics.at("z_score"),
                     second.diagnostics.at("z_score"));
    EXPECT_EQ(result.inputs_digest, second.inputs_digest);
}

}  // namespace
