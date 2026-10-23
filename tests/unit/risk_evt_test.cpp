#include <fincore/risk/evt.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace {

fincore::dataframe evt_fixture() {
    std::vector<fincore::timestamp> index;
    index.reserve(500);
    std::vector<double> values;
    values.reserve(500);
    for (int position = 0; position < 500; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
        values.push_back(0.01 * std::sin(static_cast<double>(position) * 0.7) +
                         0.004 * std::cos(static_cast<double>(position) * 0.31));
    }
    for (const auto& shock :
         std::vector<std::pair<int, double>>{{20, -0.06}, {60, -0.09},
                                             {110, -0.07}, {180, -0.12},
                                             {250, -0.08}, {330, -0.10},
                                             {410, -0.05}, {470, -0.11}}) {
        values[static_cast<std::size_t>(shock.first)] = shock.second;
    }
    return fincore::make_series(index, "returns", values);
}

TEST(RiskEvt, HillEstimatorMatchesPythonOracleExactly) {
    const auto frame = evt_fixture();
    const auto lower = fincore::hill_estimator(
        frame, std::nullopt, fincore::evt_tail::lower);
    EXPECT_NEAR(lower.xi, 0.5949755669081104, 1e-12);
    EXPECT_EQ(lower.tail_observations.size(), 26U);

    const auto upper = fincore::hill_estimator(
        frame, std::nullopt, fincore::evt_tail::upper);
    EXPECT_NEAR(upper.xi, 0.07553762175129869, 1e-12);
    EXPECT_EQ(upper.tail_observations.size(), 25U);
}

TEST(RiskEvt, GpdPwmFitMatchesPythonOracleExactly) {
    const auto parameters = fincore::gpd_fit(
        evt_fixture(), std::nullopt, fincore::gpd_estimation_method::pwm,
        fincore::evt_tail::lower);
    EXPECT_NEAR(parameters.xi, 0.6969927826510689, 1e-12);
    EXPECT_NEAR(parameters.beta, 0.00682871573607405, 1e-12);
    EXPECT_NEAR(parameters.threshold, 0.012776700681180947, 1e-12);
    EXPECT_EQ(parameters.n_exceed, 26U);
}

TEST(RiskEvt, GpdMleReproducesSourceObjectiveAndBounds) {
    const auto parameters = fincore::gpd_fit(
        evt_fixture(), std::nullopt, fincore::gpd_estimation_method::mle,
        fincore::evt_tail::lower);
    // The source minimizes the raw log-likelihood, driving the shape to its
    // upper bound. Reproduced and recorded as source behaviour.
    EXPECT_NEAR(parameters.xi, 1.0, 2e-2);
    EXPECT_GT(parameters.beta, 0.0);
    EXPECT_NEAR(parameters.threshold, 0.012776700681180947, 1e-12);
    EXPECT_EQ(parameters.n_exceed, 26U);
}

TEST(RiskEvt, ValidatesTailThresholdMethodAndSelectors) {
    const auto frame = evt_fixture();
    EXPECT_THROW(static_cast<void>(fincore::hill_estimator(
                     frame, 0.5, fincore::evt_tail::lower)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::hill_estimator(
                     frame, 0.0, fincore::evt_tail::lower)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::gpd_fit(
                     frame, 0.07, fincore::gpd_estimation_method::mle,
                     fincore::evt_tail::lower)),
                 fincore::input_error);

    const auto no_losses = fincore::make_series(
        {1, 2, 3}, "returns", {0.01, 0.02, 0.03});
    EXPECT_THROW(static_cast<void>(fincore::gpd_fit(
                     no_losses, std::nullopt,
                     fincore::gpd_estimation_method::pwm,
                     fincore::evt_tail::lower)),
                 fincore::input_error);

    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::hill_estimator(
                     frame, std::nullopt, fincore::evt_tail::lower,
                     embedded_selector)),
                 fincore::input_error);
}

fincore::dataframe evt_cvar_fixture() {
    constexpr int count = 400;
    constexpr double amplitude = 0.02;
    std::vector<fincore::timestamp> index;
    index.reserve(count);
    std::vector<double> values;
    values.reserve(count);
    for (int position = 0; position < count; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
        values.push_back(amplitude * std::sin(static_cast<double>(position) * 0.9) +
                         0.003 * std::cos(static_cast<double>(position) * 0.4));
    }
    for (int shock = 0; shock < 2; ++shock) {
        values[static_cast<std::size_t>(20 + 40 * shock)] =
            -amplitude * (2.0 + 0.3 * shock);
    }
    return fincore::make_series(index, "returns", values);
}

TEST(RiskEvt, EvtVarAndCvarMatchPythonOracleWithinTolerance) {
    const auto frame = evt_cvar_fixture();
    const double var = fincore::evt_var(
        frame, 0.05, fincore::evt_model::gpd, fincore::evt_tail::lower);
    EXPECT_NEAR(var, -0.020959731329854844, 5e-4);
    const double cvar = fincore::evt_cvar(
        frame, 0.05, fincore::evt_model::gpd, fincore::evt_tail::lower);
    EXPECT_NEAR(cvar, -0.024413095253991088, 5e-4);
    EXPECT_LT(cvar, var);

    const double var10 = fincore::evt_var(
        frame, 0.1, fincore::evt_model::gpd, fincore::evt_tail::lower);
    EXPECT_NEAR(var10, -0.019012853078465397, 5e-4);
    const double cvar10 = fincore::evt_cvar(
        frame, 0.1, fincore::evt_model::gpd, fincore::evt_tail::lower);
    EXPECT_NEAR(cvar10, -0.021805352664204795, 5e-4);
}

TEST(RiskEvt, ExtremeRiskMatchesPythonOracleWithinTolerance) {
    const auto result = fincore::extreme_risk(
        evt_cvar_fixture(), 0.05, fincore::evt_tail::lower,
        fincore::evt_model::gpd);
    EXPECT_DOUBLE_EQ(result.alpha, 0.05);
    EXPECT_NEAR(result.var, -0.020959731329854844, 5e-4);
    EXPECT_NEAR(result.cvar, -0.024413095253991088, 5e-4);
    EXPECT_NEAR(result.tail_index, 0.7170359451104547, 5e-3);
    EXPECT_NEAR(result.threshold, 0.020912878988368272, 1e-6);
    EXPECT_EQ(result.n_exceedances, 21U);
}

TEST(RiskEvt, EvtModelsValidateAlphaAndUnsupportedGev) {
    const auto frame = evt_cvar_fixture();
    EXPECT_THROW(static_cast<void>(fincore::evt_var(
                     frame, 0.0, fincore::evt_model::gpd)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::evt_cvar(
                     frame, 1.0, fincore::evt_model::gpd)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::extreme_risk(
                     frame, 0.05, fincore::evt_tail::lower,
                     fincore::evt_model::gev)),
                 fincore::input_error);
}

}  // namespace
