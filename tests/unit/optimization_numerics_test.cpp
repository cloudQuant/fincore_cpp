#include <fincore/detail/optimization.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

TEST(OptimizationNumerics, NelderMeadFindsQuadraticMinimumDeterministically) {
    const auto objective = [](const std::vector<double>& point) {
        const double x = point[0] - 1.0;
        const double y = point[1] + 0.5;
        return x * x + 3.0 * y * y;
    };
    const auto first = fincore::detail::minimize_nelder_mead(
        objective, {0.0, 0.0});
    const auto second = fincore::detail::minimize_nelder_mead(
        objective, {0.0, 0.0});

    EXPECT_TRUE(first.converged);
    EXPECT_NEAR(first.parameters[0], 1.0, 1e-6);
    EXPECT_NEAR(first.parameters[1], -0.5, 1e-6);
    EXPECT_NEAR(first.value, 0.0, 1e-10);
    EXPECT_EQ(first.parameters, second.parameters);
    EXPECT_EQ(first.iterations, second.iterations);
}

TEST(OptimizationNumerics, NelderMeadSolvesRosenbrock) {
    const auto rosenbrock = [](const std::vector<double>& point) {
        const double x = point[0];
        const double y = point[1];
        return (1.0 - x) * (1.0 - x) +
               100.0 * (y - x * x) * (y - x * x);
    };
    const auto result = fincore::detail::minimize_nelder_mead(
        rosenbrock, {-1.2, 1.0});
    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.parameters[0], 1.0, 1e-4);
    EXPECT_NEAR(result.parameters[1], 1.0, 1e-4);
}

TEST(OptimizationNumerics, ProjectOntoSimplexMatchesKnownProjection) {
    const std::vector<double> values{0.6, 0.0, 0.6};
    const auto projected = fincore::detail::project_onto_simplex(values);
    EXPECT_NEAR(projected[0], 0.5, 1e-12);
    EXPECT_NEAR(projected[1], 0.0, 1e-12);
    EXPECT_NEAR(projected[2], 0.5, 1e-12);

    const std::vector<double> bounded{-1.0, 2.0, 2.0};
    const auto bounded_projection =
        fincore::detail::project_onto_simplex(bounded);
    EXPECT_NEAR(bounded_projection[0], 0.0, 1e-12);
    EXPECT_NEAR(bounded_projection[1] + bounded_projection[2], 1.0, 1e-12);
}

TEST(OptimizationNumerics, ProjectedGradientSolvesMinVarianceWeights) {
    // Two assets, equal covariance: the minimum-variance long-only portfolio
    // with full investment is 50/50.
    const std::vector<double> hessian{1.0, 0.5, 0.5, 1.0};
    const std::vector<double> linear{0.0, 0.0};
    const auto result = fincore::detail::minimize_quadratic_on_simplex(
        hessian, linear, {0.8, 0.2});
    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.parameters[0], 0.5, 1e-8);
    EXPECT_NEAR(result.parameters[1], 0.5, 1e-8);

    // Three assets with diagonal variances 4/2/1: the full-investment
    // long-only optimum is inverse-variance normalized, [1/7, 2/7, 4/7].
    const std::vector<double> diagonal{4.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0,
                                       0.0, 1.0};
    const std::vector<double> zero_linear{0.0, 0.0, 0.0};
    const auto interior = fincore::detail::minimize_quadratic_on_simplex(
        diagonal, zero_linear, {0.34, 0.33, 0.33});
    EXPECT_TRUE(interior.converged);
    EXPECT_NEAR(interior.parameters[0], 1.0 / 7.0, 1e-7);
    EXPECT_NEAR(interior.parameters[1], 2.0 / 7.0, 1e-7);
    EXPECT_NEAR(interior.parameters[2], 4.0 / 7.0, 1e-7);
}

}  // namespace
