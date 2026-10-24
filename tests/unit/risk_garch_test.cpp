#include <fincore/calendar.hpp>
#include <fincore/risk/garch.hpp>
#include "../../src/risk/garch_internal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

std::vector<double> garch_fixture() {
    constexpr int count = 300;
    std::vector<double> shocks;
    shocks.reserve(count);
    for (int index = 0; index < count; ++index) {
        shocks.push_back(std::sin(static_cast<double>(index) * 1.7) +
                         0.3 * std::cos(static_cast<double>(index) * 0.9));
    }
    double shock_mean = 0.0;
    for (const double value : shocks) {
        shock_mean += value;
    }
    shock_mean /= static_cast<double>(count);
    double shock_variance = 0.0;
    for (const double value : shocks) {
        shock_variance += (value - shock_mean) * (value - shock_mean);
    }
    shock_variance /= static_cast<double>(count);

    std::vector<double> values;
    values.reserve(count);
    double variance =
        5e-5 + 0.1 * shock_variance + 0.8 * shock_variance;
    for (int index = 0; index < count; ++index) {
        const double eps = shocks[index] * std::sqrt(variance);
        values.push_back(eps);
        variance = 5e-5 + 0.1 * eps * eps + 0.8 * variance;
    }
    return values;
}

fincore::dataframe garch_frame() {
    std::vector<fincore::timestamp> index;
    index.reserve(300);
    for (int position = 0; position < 300; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
    }
    return fincore::make_series(index, "returns", garch_fixture());
}

fincore::dataframe returns_frame(const std::vector<double>& values) {
    std::vector<fincore::timestamp> index;
    index.reserve(values.size());
    for (std::size_t position = 0; position < values.size(); ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1U));
    }
    return fincore::make_series(index, "returns", values);
}

void expect_fixture_close(const double actual, const double expected) {
    constexpr double absolute_tolerance = 1e-12;
    constexpr double relative_tolerance = 1e-10;
    EXPECT_LE(std::abs(actual - expected),
              absolute_tolerance + relative_tolerance * std::abs(expected));
}

template <typename Result>
void expect_finite_converged_diagnostics(const Result& result) {
    ASSERT_TRUE(result.converged);
    ASSERT_TRUE(std::isfinite(result.log_likelihood));
    ASSERT_EQ(result.conditional_variances.size(), result.residuals.size());
    for (std::size_t position = 0;
         position < result.conditional_variances.size(); ++position) {
        EXPECT_GT(result.conditional_variances[position], 0.0);
        EXPECT_TRUE(std::isfinite(result.conditional_variances[position]));
        EXPECT_TRUE(std::isfinite(result.residuals[position]));
    }
}

template <std::size_t Size>
void expect_path_matches(const std::vector<double>& actual,
                         const std::array<double, Size>& expected) {
    ASSERT_EQ(actual.size(), Size);
    for (std::size_t position = 0; position < Size; ++position) {
        expect_fixture_close(actual[position], expected[position]);
    }
}

TEST(RiskGarch, ZeroMeanFitMatchesPythonOracleWithinTolerance) {
    const auto result = fincore::fit_garch11(garch_frame(), false);
    EXPECT_TRUE(result.converged);
    expect_finite_converged_diagnostics(result);
    EXPECT_DOUBLE_EQ(result.params.mu, 0.0);
    // SciPy's L-BFGS-B returned a zero-mean point with log-likelihood
    // 747.2805538792478. The observed native local fit has a higher likelihood;
    // this comparison does not establish global optimality.
    EXPECT_GE(result.log_likelihood, 747.2805538792478);
    EXPECT_NEAR(result.log_likelihood, 747.72860692539825, 1e-4);
    EXPECT_NEAR(result.params.omega, 2.7385584800554214e-05, 1e-7);
    EXPECT_NEAR(result.params.alpha, 0.25613095868607894, 1e-4);
    EXPECT_NEAR(result.params.beta, 0.65806200582598207, 1e-4);
    EXPECT_NEAR(result.conditional_variances[0], 0.006338728834392523,
                1e-12);
    // Closed-form recursion verified in Python at the native optimum.
    EXPECT_NEAR(result.conditional_variances.back(), 0.00020259002605075416,
                1e-12);
    const auto forecast = fincore::forecast_garch11(result, 3U);
    ASSERT_EQ(forecast.size(), 3U);
    EXPECT_NEAR(forecast[0], 0.00017830257310753238, 1e-12);
    EXPECT_NEAR(forecast[2], 0.00020143745105132634, 1e-12);
}

TEST(RiskGarch, ConstantMeanFitMatchesPythonOracleAndIsDeterministic) {
    const auto first = fincore::fit_garch11(garch_frame(), true);
    const auto second = fincore::fit_garch11(garch_frame(), true);
    expect_finite_converged_diagnostics(first);
    expect_finite_converged_diagnostics(second);
    EXPECT_NEAR(first.params.mu, -0.00010120350935733376, 1e-5);
    EXPECT_NEAR(first.params.omega, 2.7373511565588752e-05, 1e-7);
    EXPECT_NEAR(first.params.alpha, 0.2561245263089078, 1e-4);
    EXPECT_NEAR(first.params.beta, 0.6580996343039742, 1e-4);
    EXPECT_NEAR(first.log_likelihood, 747.7378806410663, 1e-4);
    EXPECT_EQ(first.params.omega, second.params.omega);
    EXPECT_EQ(first.params.alpha, second.params.alpha);
    EXPECT_EQ(first.params.beta, second.params.beta);
    EXPECT_EQ(first.log_likelihood, second.log_likelihood);
}

TEST(RiskGarch, ConditionalVarianceAndLikelihoodMatchPythonExactly) {
    const auto values = garch_fixture();
    const auto variance = fincore::garch11_log_likelihood(
        values, 1.969069393424767e-05, 0.28572917912639345,
        0.6745868900271427);
    EXPECT_NEAR(variance, 747.2805538792478, 1e-9);
}

TEST(RiskGarch, FixedParameterKernelsMatchPinnedPythonAndDecimalReferences) {
    const std::vector<double> observations{
        0.03, -0.02, 0.015, -0.04, 0.01, 0.025, -0.035, 0.005};
    constexpr std::array<double, 8> garch_python{
        0.0006359375, 0.0007913125, 0.0008493975, 0.0008725421,
        0.0010551319959999999, 0.00101390031696, 0.0010455642408896,
        0.001141628823076096};
    constexpr std::array<double, 8> garch_decimal{
        0.0006359375, 0.0007913125, 0.0008493975, 0.0008725421,
        0.001055131996, 0.00101390031696, 0.0010455642408896,
        0.001141628823076096};
    constexpr std::array<double, 8> egarch_python{
        0.0006359375, 0.0018420670562673872, 0.005045592740486643,
        0.010200734528895975, 0.021091491816108185, 0.03416307548209074,
        0.05164532903841664, 0.07692316231927335};
    constexpr std::array<double, 8> egarch_decimal{
        0.0006359375, 0.0018420670562673868, 0.005045592740486641,
        0.01020073452889597, 0.021091491816108175, 0.03416307548209073,
        0.051645329038416614, 0.07692316231927332};
    constexpr std::array<double, 8> gjr_python{
        0.0006359375, 0.00074259375, 0.0008295193749999999,
        0.0008318443374999999, 0.00113556480975, 0.001048317959215,
        0.0010257552898190999, 0.001204058914466134};
    constexpr std::array<double, 8> gjr_decimal{
        0.0006359375, 0.00074259375, 0.000829519375, 0.0008318443375,
        0.00113556480975, 0.001048317959215, 0.0010257552898191,
        0.001204058914466134};

    const auto garch_path =
        fincore::risk::internal::garch11_conditional_variances(
            observations, 0.0002, 0.12, 0.76);
    expect_path_matches(garch_path, garch_python);
    expect_path_matches(garch_path, garch_decimal);
    expect_fixture_close(fincore::garch11_log_likelihood(
                             observations, 0.0002, 0.12, 0.76),
                         15.634736318382965);
    expect_fixture_close(fincore::risk::internal::garch11_log_likelihood(
                             observations, 0.0002, 0.12, 0.76),
                         15.634736318382966);

    const fincore::egarch_parameters egarch_params{
        -0.1, 0.2, -0.15, 0.85};
    const auto egarch_path =
        fincore::risk::internal::egarch_conditional_variances(
            observations, egarch_params);
    expect_path_matches(egarch_path, egarch_python);
    expect_path_matches(egarch_path, egarch_decimal);
    expect_fixture_close(fincore::risk::internal::egarch_log_likelihood(
                             observations, egarch_params),
                         7.802194927683928);

    const fincore::gjr_garch_parameters gjr_params{
        0.0002, 0.08, 0.12, 0.74};
    const auto gjr_path =
        fincore::risk::internal::gjr_garch_conditional_variances(
            observations, gjr_params);
    expect_path_matches(gjr_path, gjr_python);
    expect_path_matches(gjr_path, gjr_decimal);
    expect_fixture_close(fincore::risk::internal::gjr_garch_log_likelihood(
                             observations, gjr_params),
                         15.56986849073046);
}

TEST(RiskGarch, EgarchFitMatchesPythonOracleWithinTolerance) {
    const auto result = fincore::fit_egarch(garch_frame());
    const auto repeated = fincore::fit_egarch(garch_frame());
    expect_finite_converged_diagnostics(result);
    expect_finite_converged_diagnostics(repeated);
    EXPECT_DOUBLE_EQ(result.params.omega, repeated.params.omega);
    EXPECT_DOUBLE_EQ(result.params.alpha, repeated.params.alpha);
    EXPECT_DOUBLE_EQ(result.params.gamma, repeated.params.gamma);
    EXPECT_DOUBLE_EQ(result.params.beta, repeated.params.beta);
    EXPECT_DOUBLE_EQ(result.log_likelihood, repeated.log_likelihood);
    EXPECT_NEAR(result.params.omega, -0.4898979840671724, 2e-3);
    EXPECT_NEAR(result.params.alpha, 0.2513723688487509, 2e-3);
    EXPECT_NEAR(result.params.gamma, 0.29181294103945654, 2e-3);
    EXPECT_NEAR(result.params.beta, 0.9699282283629626, 2e-3);
    EXPECT_NEAR(result.log_likelihood, 749.6345510786522, 2e-3);
    EXPECT_NEAR(result.conditional_variances[0], 0.006338728834392523,
                1e-12);
    EXPECT_NEAR(result.conditional_variances.back(), 0.0001585249892216032,
                1e-7);
    const auto forecast = fincore::forecast_egarch(result, 3U);
    EXPECT_NEAR(forecast[0], 0.00012303941916578446, 1e-7);
    EXPECT_NEAR(forecast[2], 0.0001186138132055392, 1e-7);
}

TEST(RiskGarch, GjrGarchFitMatchesPythonOracleWithinTolerance) {
    const auto result = fincore::fit_gjr_garch(garch_frame());
    expect_finite_converged_diagnostics(result);
    EXPECT_NEAR(result.params.omega, 2.7366455496523534e-05, 1e-7);
    EXPECT_NEAR(result.params.alpha, 0.25611200984967286, 1e-4);
    EXPECT_LE(result.params.gamma, 1e-3);
    EXPECT_NEAR(result.params.beta, 0.6581371813024437, 1e-4);
    EXPECT_NEAR(result.log_likelihood, 747.7286055791172, 1e-4);
    EXPECT_NEAR(result.conditional_variances.back(), 0.00020257012409381558,
                1e-7);
    const auto forecast = fincore::forecast_gjr_garch(result, 3U);
    EXPECT_NEAR(forecast[0], 0.00017828427330696095, 1e-7);
    EXPECT_NEAR(forecast[2], 0.00020140540745966628, 1e-7);
}

TEST(RiskGarch, ConditionalVaRAndESComposeModelForecasts) {
    const auto frame = garch_frame();
    const auto egarch_var = fincore::conditional_var(
        frame, fincore::garch_model::egarch, 0.05, 3U);
    EXPECT_TRUE(egarch_var.converged);
    EXPECT_NEAR(egarch_var.value, -0.031313875234808844, 1e-5);
    EXPECT_NEAR(egarch_var.conditional_variances[0],
                0.00012303941916578446, 1e-7);

    const auto egarch_es = fincore::conditional_es(
        frame, fincore::garch_model::egarch, 0.05, 3U);
    EXPECT_NEAR(egarch_es.value, -0.03926886285878336, 1e-5);

    const auto gjr_var = fincore::conditional_var(
        frame, fincore::garch_model::gjr_garch, 0.05, 3U);
    EXPECT_NEAR(gjr_var.value, -0.03927214673733541, 1e-5);

    // The observed native zero-mean GARCH fit has a higher likelihood than the
    // pinned source fit; assert closed-form composition without assuming either
    // optimizer found a global optimum.
    const auto garch_fit = fincore::fit_garch11(frame, false);
    const auto garch_var = fincore::conditional_var(
        frame, fincore::garch_model::garch, 0.05, 3U);
    const auto garch_forecast = fincore::forecast_garch11(garch_fit, 3U);
    double total = 0.0;
    for (const double value : garch_forecast) {
        total += value;
    }
    EXPECT_NEAR(garch_var.value, -1.6448536269514722 * std::sqrt(total),
                1e-15);
    EXPECT_DOUBLE_EQ(garch_var.conditional_variances[0],
                     garch_forecast[0]);

    const auto invalid = fincore::conditional_var(
        frame, fincore::garch_model::egarch, 0.0, 1U);
    EXPECT_TRUE(std::isnan(invalid.value));
}

TEST(RiskGarch, FamilyFitsRejectShortInputAndNulSelectors) {
    const auto short_frame = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9}, "returns",
        std::vector<double>(9, 0.01));
    EXPECT_THROW(static_cast<void>(fincore::fit_egarch(short_frame)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::fit_gjr_garch(short_frame)),
                 fincore::input_error);
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::fit_egarch(
                     garch_frame(), embedded_selector)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::forecast_volatility(
                     garch_frame(), fincore::garch_model::egarch, 0U)),
                 fincore::input_error);
}

TEST(RiskGarch, RejectsInsufficientAndAllNaNInput) {
    const auto short_frame = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        std::vector<double>(10, 0.01));
    EXPECT_THROW(static_cast<void>(fincore::fit_garch11(short_frame)),
                 fincore::input_error);
    const auto all_nan = fincore::make_series(
        {1, 2, 3}, "returns", {nan, nan, nan});
    EXPECT_THROW(static_cast<void>(fincore::fit_garch11(all_nan)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::forecast_garch11({}, 0U)),
                 fincore::input_error);
}

TEST(RiskGarch, FitsRejectNonfiniteOrZeroVarianceAfterNaNCleaning) {
    const auto constant_garch = returns_frame(std::vector<double>(11U, 0.1));
    const auto constant_family = returns_frame(std::vector<double>(10U, 0.1));
    EXPECT_THROW(static_cast<void>(fincore::fit_garch11(constant_garch)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::fit_egarch(constant_family)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::fit_gjr_garch(constant_family)),
                 fincore::input_error);

    auto nonfinite_garch = garch_fixture();
    nonfinite_garch[3] = std::numeric_limits<double>::infinity();
    auto nonfinite_family = garch_fixture();
    nonfinite_family[3] = -std::numeric_limits<double>::infinity();
    EXPECT_THROW(static_cast<void>(fincore::fit_garch11(
                     returns_frame(nonfinite_garch))),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::fit_egarch(
                     returns_frame(nonfinite_family))),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::fit_gjr_garch(
                     returns_frame(nonfinite_family))),
                 fincore::input_error);

    auto one_nan_garch = std::vector<double>(11U, 0.01);
    one_nan_garch.front() = nan;
    auto one_nan_family = std::vector<double>(10U, 0.01);
    one_nan_family.front() = nan;
    EXPECT_THROW(static_cast<void>(fincore::fit_garch11(
                     returns_frame(one_nan_garch))),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::fit_egarch(
                     returns_frame(one_nan_family))),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::fit_gjr_garch(
                     returns_frame(one_nan_family))),
                 fincore::input_error);
}

TEST(RiskGarch, ZeroIterationBudgetPropagatesNonconvergenceThroughFitResults) {
    const auto frame = garch_frame();
    const auto garch =
        fincore::risk::internal::fit_garch11_with_max_iterations(
            frame, false, "returns", 0U);
    const auto egarch = fincore::risk::internal::fit_egarch_with_max_iterations(
        frame, "returns", 0U);
    const auto egarch_one_iteration =
        fincore::risk::internal::fit_egarch_with_max_iterations(
            frame, "returns", 1U);
    const auto gjr = fincore::risk::internal::fit_gjr_garch_with_max_iterations(
        frame, "returns", 0U);

    EXPECT_FALSE(garch.converged);
    EXPECT_FALSE(egarch.converged);
    EXPECT_FALSE(egarch_one_iteration.converged);
    EXPECT_FALSE(gjr.converged);
    EXPECT_EQ(garch.conditional_variances.size(), garch_fixture().size());
    EXPECT_EQ(egarch.conditional_variances.size(), garch_fixture().size());
    EXPECT_EQ(gjr.conditional_variances.size(), garch_fixture().size());
    EXPECT_EQ(garch.residuals.size(), garch_fixture().size());
    EXPECT_EQ(egarch.residuals.size(), garch_fixture().size());
    EXPECT_EQ(gjr.residuals.size(), garch_fixture().size());
    EXPECT_TRUE(std::isfinite(garch.log_likelihood));
    EXPECT_TRUE(std::isfinite(egarch.log_likelihood));
    EXPECT_TRUE(std::isfinite(gjr.log_likelihood));
}

TEST(RiskGarch, SupportsSelectorsAndRejectsNul) {
    auto frame = garch_frame();
    auto strategy = frame;
    strategy.rename_column("returns", "strategy");
    const auto result =
        fincore::fit_garch11(strategy, true, "strategy");
    EXPECT_TRUE(result.converged);
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::fit_garch11(
                     strategy, true, embedded_selector)),
                 fincore::input_error);
}

}  // namespace
