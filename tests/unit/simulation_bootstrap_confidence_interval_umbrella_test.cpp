#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

namespace {

TEST(SimulationBootstrapConfidenceIntervalUmbrella, ExportsConfidenceIntervalSurface) {
    const auto returns = fincore::make_series({9, -3}, "returns", {0.01, 0.01});
    const auto interval = fincore::bootstrap_confidence_interval(
        returns, 4U, 0.2, fincore::bootstrap_statistic::mean, 42U);

    EXPECT_DOUBLE_EQ(interval.lower, 0.01);
    EXPECT_DOUBLE_EQ(interval.upper, 0.01);
}

}  // namespace
