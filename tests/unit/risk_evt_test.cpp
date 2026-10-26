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
    // The prefix before the NUL is the column this frame actually has, so a
    // selector that stopped at the terminator would succeed; the throw is what
    // proves the embedded NUL is rejected rather than truncated away.
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

TEST(RiskEvt, EvtModelsValidateAlphaAndBlockSize) {
    const auto frame = evt_cvar_fixture();
    EXPECT_THROW(static_cast<void>(fincore::evt_var(
                     frame, 0.0, fincore::evt_model::gpd)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::evt_cvar(
                     frame, 1.0, fincore::evt_model::gpd)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::evt_var(
                     frame, 0.0, fincore::evt_model::gev)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::evt_cvar(
                     frame, 1.0, fincore::evt_model::gev)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::gev_fit(
                     frame, 0U, fincore::evt_tail::lower)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::gev_fit(
                     frame, 10000U, fincore::evt_tail::lower)),
                 fincore::input_error);
    const auto flat = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3, 4}, "returns",
        std::vector<double>{0.01, 0.01, 0.01, 0.01});
    EXPECT_THROW(static_cast<void>(fincore::gev_fit(
                     flat, 2U, fincore::evt_tail::lower)),
                 fincore::input_error);
}

namespace {

fincore::dataframe gev_fixture() {
    std::vector<fincore::timestamp> index;
    index.reserve(1200);
    std::vector<double> values;
    values.reserve(1200);
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

fincore::dataframe gev_bounded_fixture() {
    std::vector<fincore::timestamp> index;
    index.reserve(400);
    std::vector<double> values;
    values.reserve(400);
    for (int position = 0; position < 400; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
        const double linear =
            -0.02 + 0.04 * static_cast<double>(position) / 399.0;
        values.push_back(linear * linear * linear);
    }
    return fincore::make_series(index, "returns", values);
}

}  // namespace

TEST(RiskEvt, GevFitMatchesPythonOracle) {
    const auto frame = gev_fixture();

    const auto lower20 =
        fincore::gev_fit(frame, 20U, fincore::evt_tail::lower);
    EXPECT_NEAR(lower20.xi, 0.7422165388985796, 1e-12);
    EXPECT_NEAR(lower20.mu, 0.019478399594106605, 1e-14);
    EXPECT_NEAR(lower20.sigma, 0.003635672307968767, 1e-14);
    EXPECT_EQ(lower20.n_blocks, 60U);

    const auto upper20 =
        fincore::gev_fit(frame, 20U, fincore::evt_tail::upper);
    EXPECT_NEAR(upper20.xi, 0.7458259400937222, 1e-12);
    EXPECT_NEAR(upper20.mu, 0.01930859111583602, 1e-14);
    EXPECT_NEAR(upper20.sigma, 0.003717379847844264, 1e-14);

    const auto lower40 =
        fincore::gev_fit(frame, 40U, fincore::evt_tail::lower);
    EXPECT_NEAR(lower40.xi, 0.739946573888107, 1e-12);
    EXPECT_NEAR(lower40.mu, 0.02324936792257617, 1e-14);
    EXPECT_NEAR(lower40.sigma, 0.003302944806895195, 1e-14);
    EXPECT_EQ(lower40.n_blocks, 30U);

    const auto upper40 =
        fincore::gev_fit(frame, 40U, fincore::evt_tail::upper);
    EXPECT_NEAR(upper40.xi, 0.7434406425221287, 1e-12);
    EXPECT_NEAR(upper40.mu, 0.023363420083466202, 1e-14);
    EXPECT_NEAR(upper40.sigma, 0.0031492320730879706, 1e-14);

    const auto automatic = fincore::gev_fit(frame);
    EXPECT_NEAR(automatic.xi, 0.7390702939614882, 1e-12);
    EXPECT_NEAR(automatic.mu, 0.022447653077878327, 1e-14);
    EXPECT_NEAR(automatic.sigma, 0.0050653905775546165, 1e-14);
    EXPECT_EQ(automatic.n_blocks, 35U);
}

TEST(RiskEvt, GevVarAndCvarMatchPythonOracle) {
    const auto frame = gev_fixture();
    EXPECT_NEAR(fincore::evt_var(frame, 0.05, fincore::evt_model::gev,
                                 fincore::evt_tail::lower, std::nullopt,
                                 20U),
                -0.0589887427734803, 1e-11);
    EXPECT_NEAR(fincore::evt_cvar(frame, 0.05, fincore::evt_model::gev,
                                  fincore::evt_tail::lower, std::nullopt,
                                  20U),
                -0.1894758180300359, 1e-11);
    EXPECT_NEAR(fincore::evt_var(frame, 0.1, fincore::evt_model::gev,
                                 fincore::evt_tail::lower, std::nullopt,
                                 20U),
                -0.040607877877553836, 1e-11);
    EXPECT_NEAR(fincore::evt_cvar(frame, 0.1, fincore::evt_model::gev,
                                  fincore::evt_tail::lower, std::nullopt,
                                  20U),
                -0.11872905204186214, 1e-11);
    EXPECT_NEAR(fincore::evt_var(frame, 0.05, fincore::evt_model::gev,
                                 fincore::evt_tail::upper, std::nullopt,
                                 20U),
                0.05999841670244999, 1e-11);
    EXPECT_NEAR(fincore::evt_cvar(frame, 0.05, fincore::evt_model::gev,
                                  fincore::evt_tail::upper, std::nullopt,
                                  20U),
                0.19677910566036916, 1e-11);
    EXPECT_NEAR(fincore::evt_var(frame, 0.05, fincore::evt_model::gev,
                                 fincore::evt_tail::lower, std::nullopt,
                                 40U),
                -0.0589820102613349, 1e-11);
    EXPECT_NEAR(fincore::evt_cvar(frame, 0.05, fincore::evt_model::gev,
                                  fincore::evt_tail::lower, std::nullopt,
                                  40U),
                -0.1756985937778102, 1e-11);
    EXPECT_NEAR(fincore::evt_cvar(frame, 0.1, fincore::evt_model::gev,
                                  fincore::evt_tail::upper, std::nullopt,
                                  40U),
                0.10987980436883901, 1e-11);
    EXPECT_NEAR(fincore::evt_var(frame, 0.05, fincore::evt_model::gev),
                -0.07715171961133763, 1e-11);
    EXPECT_NEAR(fincore::evt_cvar(frame, 0.05, fincore::evt_model::gev),
                -0.25508073875774867, 1e-11);
}

TEST(RiskEvt, GevExtremeRiskReportsLocationScaleAndBlocks) {
    const auto result = fincore::extreme_risk(
        gev_fixture(), 0.05, fincore::evt_tail::lower,
        fincore::evt_model::gev, std::nullopt, 20U);
    EXPECT_DOUBLE_EQ(result.alpha, 0.05);
    EXPECT_NEAR(result.var, -0.0589887427734803, 1e-11);
    EXPECT_NEAR(result.cvar, -0.1894758180300359, 1e-11);
    EXPECT_NEAR(result.tail_index, 0.7422165388985796, 1e-12);
    EXPECT_NEAR(*result.location, 0.019478399594106605, 1e-14);
    EXPECT_NEAR(*result.scale, 0.003635672307968767, 1e-14);
    EXPECT_EQ(*result.n_blocks, 60U);
    // GEV is a block-maxima model, so the peak-over-threshold fields stay unset
    // (NaN threshold, zero exceedances) where the GPD cases above fill them in.
    EXPECT_TRUE(std::isnan(result.threshold));
    EXPECT_EQ(result.n_exceedances, 0U);

    const auto upper = fincore::extreme_risk(
        gev_fixture(), 0.05, fincore::evt_tail::upper,
        fincore::evt_model::gev, std::nullopt, 20U);
    EXPECT_NEAR(upper.var, 0.05999841670244999, 1e-11);
    EXPECT_NEAR(upper.cvar, 0.19677910566036916, 1e-11);
    EXPECT_NEAR(upper.tail_index, 0.7458259400937222, 1e-12);
    EXPECT_NEAR(*upper.location, 0.01930859111583602, 1e-14);
    EXPECT_NEAR(*upper.scale, 0.003717379847844264, 1e-14);
}

TEST(RiskEvt, GevBoundedShapesMatchPythonOracle) {
    const auto frame = gev_bounded_fixture();
    const auto block10 =
        fincore::gev_fit(frame, 10U, fincore::evt_tail::lower);
    EXPECT_NEAR(block10.xi, -0.3296905627790604, 1e-10);
    EXPECT_NEAR(block10.mu, 1.0302439265082289e-06, 1e-15);
    EXPECT_NEAR(block10.sigma, 3.324470463056271e-06, 1e-12);
    EXPECT_EQ(block10.n_blocks, 40U);
    EXPECT_NEAR(fincore::evt_var(frame, 0.05, fincore::evt_model::gev,
                                 fincore::evt_tail::lower, std::nullopt,
                                 10U),
                -7.32650498499936e-06, 1e-12);

    const auto block20 =
        fincore::gev_fit(frame, 20U, fincore::evt_tail::lower);
    EXPECT_NEAR(block20.xi, 0.01106175003969376, 1e-10);
    EXPECT_NEAR(fincore::evt_var(frame, 0.05, fincore::evt_model::gev,
                                 fincore::evt_tail::lower, std::nullopt,
                                 20U),
                -1.4505465400897338e-05, 1e-12);

    const auto block40 =
        fincore::gev_fit(frame, 40U, fincore::evt_tail::lower);
    EXPECT_NEAR(block40.xi, 0.6394385204703907, 1e-11);
    EXPECT_NEAR(block40.sigma, 5.382188123834289e-06, 1e-12);
    EXPECT_NEAR(fincore::evt_var(frame, 0.05, fincore::evt_model::gev,
                                 fincore::evt_tail::lower, std::nullopt,
                                 40U),
                -4.901264065514572e-05, 1e-12);
}

}  // namespace
