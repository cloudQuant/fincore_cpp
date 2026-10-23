#include <fincore/calendar.hpp>
#include <fincore/risk/walk_forward.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

fincore::dataframe walk_forward_fixture(int count) {
    std::vector<fincore::timestamp> index;
    index.reserve(static_cast<std::size_t>(count));
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(count));
    for (int position = 0; position < count; ++position) {
        index.push_back(fincore::utc_timestamp(2021, 1, 1) +
                        position * 86'400'000'000'000LL);
        values.push_back(0.01 * std::sin(static_cast<double>(position) * 0.7));
    }
    return fincore::make_series(index, "returns", values);
}

TEST(RiskWalkForward, NormalDistributionMatchesPythonOracle) {
    fincore::risk_model_spec spec;
    spec.distribution = "normal";
    spec.window = 50U;
    spec.refit_cadence = 5U;
    spec.confidence_level = 0.99;
    const auto result =
        fincore::walk_forward_var(walk_forward_fixture(300), spec);

    EXPECT_EQ(result.status, fincore::walk_forward_status::ok);
    ASSERT_EQ(result.forecast.size(), 250U);
    EXPECT_EQ(result.index.front(), fincore::utc_timestamp(2021, 2, 20));
    EXPECT_EQ(result.index.back(), fincore::utc_timestamp(2021, 10, 27));
    EXPECT_NEAR(result.forecast.front(), -0.015891935566622144, 1e-12);
    EXPECT_NEAR(result.forecast.back(), -0.016807926786139734, 1e-12);
    EXPECT_NEAR(result.realized.front(), -0.00428182669496151, 1e-12);
    ASSERT_EQ(result.refit_timestamps.size(), 50U);
    EXPECT_EQ(result.refit_timestamps.front(),
              fincore::utc_timestamp(2021, 2, 20));
    EXPECT_EQ(result.refit_timestamps.back(),
              fincore::utc_timestamp(2021, 10, 23));
    ASSERT_EQ(result.fits.size(), 50U);
    EXPECT_EQ(result.fits.front().n_observations, 50U);
    EXPECT_TRUE(result.has_backtest);
    EXPECT_EQ(result.backtest.status,
              fincore::risk_backtest_status::inconclusive);
    EXPECT_EQ(result.backtest.observations, 250U);
    EXPECT_EQ(result.backtest.exceptions, 0U);
    EXPECT_NEAR(result.backtest.kupiec_lr, 5.025167926750753, 1e-12);
    EXPECT_EQ(result.inputs_digest.size(), 64U);
    EXPECT_EQ(result.n_observations, 300U);
}

TEST(RiskWalkForward, HistoricalDistributionMatchesPythonOracle) {
    fincore::risk_model_spec spec;
    spec.distribution = "historical";
    spec.window = 100U;
    spec.refit_cadence = 10U;
    spec.confidence_level = 0.95;
    const auto result =
        fincore::walk_forward_var(walk_forward_fixture(300), spec);

    EXPECT_EQ(result.status, fincore::walk_forward_status::ok);
    ASSERT_EQ(result.forecast.size(), 200U);
    EXPECT_EQ(result.index.front(), fincore::utc_timestamp(2021, 4, 11));
    EXPECT_EQ(result.index.back(), fincore::utc_timestamp(2021, 10, 27));
    EXPECT_NEAR(result.forecast.front(), -0.009674773367508039, 1e-12);
    EXPECT_NEAR(result.forecast.back(), -0.009982395371701296, 1e-12);
    EXPECT_NEAR(result.realized.front(), 0.007738906815578891, 1e-12);
    ASSERT_EQ(result.refit_timestamps.size(), 20U);
    EXPECT_EQ(result.refit_timestamps.front(),
              fincore::utc_timestamp(2021, 4, 11));
    EXPECT_EQ(result.refit_timestamps.back(),
              fincore::utc_timestamp(2021, 10, 18));
    EXPECT_EQ(result.backtest.status, fincore::risk_backtest_status::fail);
    EXPECT_EQ(result.backtest.observations, 200U);
    EXPECT_EQ(result.backtest.exceptions, 19U);
    EXPECT_NEAR(result.backtest.kupiec_lr, 6.823658870683893, 1e-12);
}

TEST(RiskWalkForward, ReportsUnsupportedInsufficientAndValidatesSpec) {
    fincore::risk_model_spec unsupported;
    unsupported.distribution = "normal";
    unsupported.window = 50U;
    unsupported.forecast_target = fincore::risk_forecast_target::es;
    auto result = fincore::walk_forward_var(walk_forward_fixture(300),
                                            unsupported);
    EXPECT_EQ(result.status, fincore::walk_forward_status::unsupported);
    EXPECT_EQ(result.unsupported_reason,
              "walk_forward_var currently supports forecast_target='var' only");

    fincore::risk_model_spec insufficient;
    insufficient.distribution = "normal";
    insufficient.window = 50U;
    result = fincore::walk_forward_var(walk_forward_fixture(50), insufficient);
    EXPECT_EQ(result.status, fincore::walk_forward_status::insufficient_data);
    EXPECT_FALSE(result.has_backtest);

    fincore::risk_model_spec rank_unsupported;
    rank_unsupported.distribution = "historical";
    rank_unsupported.window = 10U;
    rank_unsupported.confidence_level = 0.99;
    result = fincore::walk_forward_var(walk_forward_fixture(300),
                                       rank_unsupported);
    EXPECT_EQ(result.status, fincore::walk_forward_status::unsupported);
    EXPECT_EQ(result.unsupported_reason,
              "historical Weibull quantile requires 1 / (window + 1) <= 1 - confidence_level <= window / (window + 1)");

    fincore::risk_model_spec bad_window;
    bad_window.window = 1U;
    EXPECT_THROW(static_cast<void>(fincore::walk_forward_var(
                     walk_forward_fixture(300), bad_window)),
                 fincore::input_error);

    fincore::risk_model_spec bad_confidence;
    bad_confidence.confidence_level = 1.0;
    EXPECT_THROW(static_cast<void>(fincore::walk_forward_var(
                     walk_forward_fixture(300), bad_confidence)),
                 fincore::input_error);
}

TEST(RiskWalkForward, ReportsHorizonAsStructuredUnsupportedState) {
    fincore::risk_model_spec spec;
    spec.distribution = "normal";
    spec.window = 50U;
    spec.horizon = 2U;
    const auto result =
        fincore::walk_forward_var(walk_forward_fixture(300), spec);
    EXPECT_EQ(result.status, fincore::walk_forward_status::unsupported);
    EXPECT_EQ(result.unsupported_reason,
              "walk_forward_var currently supports horizon=1 only");
    EXPECT_EQ(result.n_observations, 300U);
    EXPECT_TRUE(result.forecast.empty());

    fincore::risk_model_spec zero_horizon;
    zero_horizon.horizon = 0U;
    EXPECT_THROW(static_cast<void>(fincore::walk_forward_var(
                     walk_forward_fixture(300), zero_horizon)),
                 fincore::input_error);
}

}  // namespace
