#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace {

TEST(SimulationParametersUmbrella, ExportsNamedDataframeCalibration) {
    const auto returns = fincore::make_series(
        {42, -5, 42, 0, 8}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), -0.02, 0.03, 0.0});

    const auto parameters = fincore::estimate_simulation_parameters(returns);

    // The repeated 42 and the unsorted index do not matter because the estimator
    // reads values row-wise, the NaN is dropped before both moments, and the
    // result is daily annualised: 1.26 is 252 x 0.005, and the volatility is the
    // sample deviation of the four remaining values times sqrt(252).
    EXPECT_NEAR(parameters.drift, 1.2599999999999998, 1.0e-12);
    EXPECT_NEAR(parameters.volatility, 0.3304542328371662, 1.0e-12);
}

}  // namespace
