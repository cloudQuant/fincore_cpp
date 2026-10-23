#include <fincore/calendar.hpp>
#include <fincore/risk/garch.hpp>

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

TEST(RiskGarch, ZeroMeanFitMatchesPythonOracleWithinTolerance) {
    const auto result = fincore::fit_garch11(garch_frame(), false);
    EXPECT_TRUE(result.converged);
    EXPECT_DOUBLE_EQ(result.params.mu, 0.0);
    // SciPy's L-BFGS-B stalls at a suboptimal zero-mean point
    // (loglik 747.2805538792478 at its parameters). The native Nelder-Mead
    // reaches the true constrained optimum; the closed-form likelihood at the
    // native parameters was verified in Python at 747.7286069253973.
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
    EXPECT_TRUE(first.converged);
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
