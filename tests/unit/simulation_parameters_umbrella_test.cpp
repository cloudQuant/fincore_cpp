#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace {

TEST(SimulationParametersUmbrella, ExportsNamedDataframeCalibration) {
    const auto returns = fincore::make_series(
        {42, -5, 42, 0, 8}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), -0.02, 0.03, 0.0});

    const auto parameters = fincore::estimate_simulation_parameters(returns);

    EXPECT_NEAR(parameters.drift, 1.2599999999999998, 1.0e-12);
    EXPECT_NEAR(parameters.volatility, 0.3304542328371662, 1.0e-12);
}

}  // namespace
