#include <fincore/risk/models.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

fincore::dataframe sine_fixture() {
    std::vector<fincore::timestamp> index;
    std::vector<double> values;
    for (int position = 0; position < 300; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
        values.push_back(
            0.01 * std::sin((position + 1) * 0.7) +
            0.003 * std::cos((position + 1) * 0.13) +
            (position % 37 == 0 ? -0.04 : 0.0));
    }
    return fincore::make_series(index, "returns", values);
}

fincore::dataframe heavy_fixture() {
    std::vector<fincore::timestamp> index;
    std::vector<double> values;
    for (int position = 0; position < 1200; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
        const double raw =
            static_cast<double>((position * 7919) % 1000) / 1000.0;
        const double sign = raw >= 0.5 ? 1.0 : -1.0;
        const double magnitude =
            std::pow(std::max(std::fabs(raw - 0.5) * 2.0, 1e-3), -0.25);
        values.push_back(sign * magnitude * 0.01);
    }
    return fincore::make_series(index, "returns", values);
}

TEST(RiskModels, HistoricalForecastMatchesPythonOracle) {
    const auto frame = sine_fixture();
    fincore::risk_forecast_config config;
    config.method = fincore::risk_forecast_method::historical;

    const auto value_at_risk = fincore::forecast_var(frame, config);
    EXPECT_EQ(value_at_risk.method,
              fincore::risk_forecast_method::historical);
    EXPECT_EQ(value_at_risk.status, fincore::risk_estimate_status::ok);
    EXPECT_NEAR(value_at_risk.estimate, -0.012382826870905906, 1e-15);
    EXPECT_EQ(value_at_risk.sign_convention, "losses_negative");
    EXPECT_EQ(value_at_risk.confidence_level, 0.95);
    EXPECT_EQ(value_at_risk.horizon, 1U);
    EXPECT_EQ(value_at_risk.forecast_timestamp, 300);
    EXPECT_TRUE(value_at_risk.has_forecast_timestamp);
    EXPECT_EQ(value_at_risk.diagnostics.at("n_observations"), 300.0);
    EXPECT_EQ(value_at_risk.inputs_digest.size(), 64U);
    EXPECT_TRUE(value_at_risk.kernel.empty());

    const auto expected_shortfall = fincore::forecast_es(frame, config);
    EXPECT_NEAR(expected_shortfall.estimate, -0.02864328433487541, 1e-15);
    EXPECT_EQ(expected_shortfall.diagnostics.at("n_tail_observations"),
              15.0);

    config.confidence_level = 0.99;
    EXPECT_NEAR(fincore::forecast_var(frame, config).estimate,
                -0.04327816187664599, 1e-15);
    const auto tail_99 = fincore::forecast_es(frame, config);
    EXPECT_NEAR(tail_99.estimate, -0.04765377824534192, 1e-15);
    EXPECT_EQ(tail_99.diagnostics.at("n_tail_observations"), 3.0);

    const auto repeated = fincore::forecast_var(frame, config);
    EXPECT_EQ(repeated.inputs_digest,
              fincore::forecast_var(frame, config).inputs_digest);
}

TEST(RiskModels, EvtForecastMatchesPythonOracle) {
    const auto frame = heavy_fixture();
    fincore::risk_forecast_config config;
    config.method = fincore::risk_forecast_method::evt;
    const auto value_at_risk = fincore::forecast_var(frame, config);
    EXPECT_EQ(value_at_risk.status, fincore::risk_estimate_status::ok);
    EXPECT_EQ(value_at_risk.kernel, "evt_var");
    EXPECT_NEAR(value_at_risk.estimate, -0.017763810864321817, 1e-6);
    EXPECT_NEAR(value_at_risk.diagnostics.at("alpha"), 0.05,
                1e-15);
    const auto expected_shortfall = fincore::forecast_es(frame, config);
    EXPECT_EQ(expected_shortfall.kernel, "evt_cvar");
    EXPECT_NEAR(expected_shortfall.estimate, -0.023324642899925165, 1e-6);

    config.confidence_level = 0.99;
    EXPECT_NEAR(fincore::forecast_var(frame, config).estimate,
                -0.026566608938194285, 1e-6);
    EXPECT_NEAR(fincore::forecast_es(frame, config).estimate,
                -0.0332668744363668, 1e-6);
}

TEST(RiskModels, GarchForecastMatchesPythonOracleWithinBudget) {
    const auto frame = sine_fixture();
    fincore::risk_forecast_config config;
    config.method = fincore::risk_forecast_method::garch;
    const auto value_at_risk = fincore::forecast_var(frame, config);
    EXPECT_EQ(value_at_risk.status, fincore::risk_estimate_status::ok);
    EXPECT_EQ(value_at_risk.kernel, "conditional_var");
    EXPECT_NEAR(value_at_risk.estimate, -0.016328834797036625, 2e-5);
    EXPECT_EQ(value_at_risk.diagnostics.at("horizon"), 1.0);
    const auto expected_shortfall = fincore::forecast_es(frame, config);
    EXPECT_EQ(expected_shortfall.kernel, "conditional_es");
    EXPECT_NEAR(expected_shortfall.estimate, -0.020477017599398892, 2e-5);

    config.confidence_level = 0.99;
    EXPECT_NEAR(fincore::forecast_var(frame, config).estimate,
                -0.02309418266356838, 2e-5);
    EXPECT_NEAR(fincore::forecast_es(frame, config).estimate,
                -0.02645818569485599, 2e-5);
}

TEST(RiskModels, InsufficientDataAndNaNDropping) {
    const auto single = fincore::make_series(
        std::vector<fincore::timestamp>{1}, "returns",
        std::vector<double>{0.02});
    fincore::risk_forecast_config config;
    const auto result = fincore::forecast_var(single, config);
    EXPECT_EQ(result.status,
              fincore::risk_estimate_status::insufficient_data);
    EXPECT_NEAR(result.estimate, 0.02, 1e-15);
    EXPECT_EQ(result.forecast_timestamp, 1);

    const auto with_nan = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3}, "returns",
        std::vector<double>{0.01,
                            std::numeric_limits<double>::quiet_NaN(),
                            0.02});
    const auto clean = fincore::forecast_var(with_nan, config);
    EXPECT_EQ(clean.status, fincore::risk_estimate_status::ok);
    EXPECT_EQ(clean.diagnostics.at("n_observations"), 2.0);
    EXPECT_EQ(clean.forecast_timestamp, 3);

    const auto empty = fincore::make_series(
        std::vector<fincore::timestamp>{}, "returns",
        std::vector<double>{});
    EXPECT_THROW(static_cast<void>(fincore::forecast_var(empty, config)),
                 fincore::input_error);
}

TEST(RiskModels, RejectsInvalidInputs) {
    const auto frame = sine_fixture();
    fincore::risk_forecast_config config;
    config.confidence_level = 1.0;
    EXPECT_THROW(static_cast<void>(fincore::forecast_var(frame, config)),
                 fincore::input_error);
    config.confidence_level = 0.0;
    EXPECT_THROW(static_cast<void>(fincore::forecast_es(frame, config)),
                 fincore::input_error);
    config.confidence_level = 0.95;
    config.horizon = 0U;
    EXPECT_THROW(static_cast<void>(fincore::forecast_var(frame, config)),
                 fincore::input_error);
    config.horizon = 1U;
    config.method = static_cast<fincore::risk_forecast_method>(99);
    EXPECT_THROW(static_cast<void>(fincore::forecast_var(frame, config)),
                 fincore::input_error);
    config.method = fincore::risk_forecast_method::historical;
    EXPECT_THROW(static_cast<void>(fincore::forecast_var(frame, config,
                                                         "missing")),
                 fincore::input_error);

    const auto duplicate = fincore::make_series(
        std::vector<fincore::timestamp>{1, 1}, "returns",
        std::vector<double>{0.01, 0.02});
    EXPECT_THROW(static_cast<void>(fincore::forecast_var(duplicate, config)),
                 fincore::input_error);
    const auto unsorted = fincore::make_series(
        std::vector<fincore::timestamp>{2, 1}, "returns",
        std::vector<double>{0.01, 0.02});
    EXPECT_THROW(static_cast<void>(fincore::forecast_var(unsorted, config)),
                 fincore::input_error);
}

TEST(RiskModels, DigestIsInputSensitiveAndOwned) {
    const auto frame = sine_fixture();
    const auto first = fincore::forecast_var(frame);
    const auto second = fincore::forecast_var(frame);
    EXPECT_EQ(first.inputs_digest, second.inputs_digest);

    auto values = std::vector<double>{0.01, -0.02, 0.03, -0.01};
    const auto series = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3, 4}, "returns", values);
    const auto original = fincore::forecast_var(series);
    values[1] = -0.021;
    const auto changed = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3, 4}, "returns", values);
    EXPECT_NE(original.inputs_digest,
              fincore::forecast_var(changed).inputs_digest);
    EXPECT_EQ(original.inputs_digest.size(), 64U);
}

}  // namespace
