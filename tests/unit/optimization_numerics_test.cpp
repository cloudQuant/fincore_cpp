#include <fincore/detail/optimization.hpp>
#include <fincore/error.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
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

TEST(OptimizationNumerics, NelderMeadHandlesIllConditionedFiniteQuadratic) {
    const auto objective = [](const std::vector<double>& point) {
        const double x = point[0] - 2.0;
        const double y = point[1] + 3.0;
        return x * x + 1e4 * y * y;
    };
    const auto result =
        fincore::detail::minimize_nelder_mead(objective, {0.0, 0.0});

    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.parameters[0], 2.0, 1e-4);
    EXPECT_NEAR(result.parameters[1], -3.0, 1e-4);
    EXPECT_NEAR(result.value, 0.0, 1e-8);
}

TEST(OptimizationNumerics, NelderMeadDoesNotConvergeOnNonfiniteObjective) {
    fincore::detail::nelder_mead_options options;
    options.max_iterations = 25U;
    const auto nan_result = fincore::detail::minimize_nelder_mead(
        [](const std::vector<double>&) {
            return std::numeric_limits<double>::quiet_NaN();
        },
        {1.0}, options);
    const auto infinity_result = fincore::detail::minimize_nelder_mead(
        [](const std::vector<double>&) {
            return -std::numeric_limits<double>::infinity();
        },
        {1.0}, options);

    EXPECT_FALSE(nan_result.converged);
    EXPECT_FALSE(infinity_result.converged);
    EXPECT_TRUE(std::isinf(nan_result.value));
    EXPECT_TRUE(std::isinf(infinity_result.value));
}

TEST(OptimizationNumerics, NelderMeadExhaustionReturnsBestFinitePoint) {
    fincore::detail::nelder_mead_options options;
    options.function_tolerance = 0.0;
    options.simplex_tolerance = 0.0;
    options.max_iterations = 1U;
    const auto result = fincore::detail::minimize_nelder_mead(
        [](const std::vector<double>& point) {
            return point[0] * point[0];
        },
        {1.0}, options);

    EXPECT_FALSE(result.converged);
    EXPECT_EQ(result.iterations, 1U);
    EXPECT_TRUE(std::isfinite(result.value));
    EXPECT_LE(result.value, 1.0);
}

TEST(OptimizationNumerics, NelderMeadZeroIterationIsNotConverged) {
    fincore::detail::nelder_mead_options options;
    options.max_iterations = 0U;
    const auto result = fincore::detail::minimize_nelder_mead(
        [](const std::vector<double>& point) {
            return point[0] * point[0];
        },
        {1.0}, options);

    EXPECT_FALSE(result.converged);
    EXPECT_EQ(result.iterations, 0U);
    EXPECT_NEAR(result.parameters[0], 1.0, 1e-12);
    EXPECT_NEAR(result.value, 1.0, 1e-12);
}

TEST(OptimizationNumerics, NelderMeadRejectsMalformedInputs) {
    const std::function<double(const std::vector<double>&)> empty_objective;
    EXPECT_THROW(static_cast<void>(fincore::detail::minimize_nelder_mead(
                     empty_objective, {0.0})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::detail::minimize_nelder_mead(
                     [](const std::vector<double>&) { return 0.0; }, {})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::detail::minimize_nelder_mead(
                     [](const std::vector<double>&) { return 0.0; },
                     {std::numeric_limits<double>::infinity()})),
                 fincore::input_error);

    fincore::detail::nelder_mead_options bad_options;
    bad_options.function_tolerance =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(static_cast<void>(fincore::detail::minimize_nelder_mead(
                     [](const std::vector<double>&) { return 0.0; }, {0.0},
                     bad_options)),
                 fincore::input_error);
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

TEST(OptimizationNumerics, ProjectOntoBoundedSimplexMatchesBoundaryFixture) {
    const auto projected = fincore::detail::project_onto_simplex(
        {-4.0, 2.0, 10.0}, 0.1, 0.6, 1.0);

    ASSERT_EQ(projected.size(), 3U);
    EXPECT_NEAR(projected[0], 0.1, 1e-12);
    EXPECT_NEAR(projected[1], 0.3, 1e-12);
    EXPECT_NEAR(projected[2], 0.6, 1e-12);
    EXPECT_NEAR(projected[0] + projected[1] + projected[2], 1.0, 1e-12);
}

TEST(OptimizationNumerics, ProjectOntoSimplexRejectsInvalidInputs) {
    EXPECT_THROW(static_cast<void>(fincore::detail::project_onto_simplex({})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::detail::project_onto_simplex(
                     {0.0, 1.0}, 0.5, 0.4, 0.9)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::detail::project_onto_simplex(
                     {0.0, 1.0}, 0.0, 0.4, 0.9)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::detail::project_onto_simplex(
                     {0.0, std::numeric_limits<double>::quiet_NaN()})),
                 fincore::input_error);
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

TEST(OptimizationNumerics, ProjectedGradientHandlesIllConditionedQuadratic) {
    const std::vector<double> hessian{1.0, 0.0, 0.0, 100.0};
    const std::vector<double> linear{0.0, 0.0};
    fincore::detail::projected_gradient_options options;
    options.gradient_tolerance = 1e-8;
    options.max_iterations = 50000U;
    const auto result = fincore::detail::minimize_quadratic_on_simplex(
        hessian, linear, {0.5, 0.5}, options);
    const double expected_first = 100.0 / 101.0;
    const double expected_second = 1.0 / 101.0;
    const double reference_value =
        0.5 * (expected_first * expected_first +
               100.0 * expected_second * expected_second);

    ASSERT_TRUE(result.converged);
    EXPECT_NEAR(result.parameters[0], expected_first, 1e-8);
    EXPECT_NEAR(result.parameters[1], expected_second, 1e-8);
    EXPECT_NEAR(result.parameters[0] + result.parameters[1], 1.0, 1e-12);
    EXPECT_NEAR(result.value, reference_value, 1e-10);
    EXPECT_LE(result.projected_gradient_norm, options.gradient_tolerance);
}

TEST(OptimizationNumerics, ProjectedGradientHandlesSingularQuadratic) {
    const std::vector<double> hessian{0.0, 0.0, 0.0, 0.0};
    const std::vector<double> linear{0.0, 0.0};
    const auto result = fincore::detail::minimize_quadratic_on_simplex(
        hessian, linear, {0.2, 0.8});

    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.parameters[0] + result.parameters[1], 1.0, 1e-12);
    EXPECT_NEAR(result.value, 0.0, 1e-12);
    EXPECT_NEAR(result.projected_gradient_norm, 0.0, 1e-12);
}

TEST(OptimizationNumerics, ProjectedGradientZeroIterationIsNotConverged) {
    const std::vector<double> hessian{2.0, 0.0, 0.0, 4.0};
    const std::vector<double> linear{0.0, 0.0};
    fincore::detail::projected_gradient_options options;
    options.max_iterations = 0U;
    const auto result = fincore::detail::minimize_quadratic_on_simplex(
        hessian, linear, {0.5, 0.5}, options);

    EXPECT_FALSE(result.converged);
    EXPECT_EQ(result.iterations, 0U);
    EXPECT_NEAR(result.value, 0.75, 1e-12);
    EXPECT_GT(result.projected_gradient_norm, 0.0);
}

TEST(OptimizationNumerics, QuadraticSolversRejectMalformedInputs) {
    const std::vector<double> linear{0.0, 0.0};
    EXPECT_THROW(static_cast<void>(fincore::detail::minimize_quadratic_on_simplex(
                     {1.0}, linear, {0.5, 0.5})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::detail::minimize_quadratic_on_simplex(
                     {1.0, 0.0, 0.0, 1.0}, linear, {1.0})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::detail::minimize_quadratic_on_simplex(
                     {1.0, 0.0, 0.0, std::numeric_limits<double>::infinity()},
                     linear, {0.5, 0.5})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::detail::quadratic_objective(
                     {1.0}, linear, {0.5, 0.5})),
                 fincore::input_error);
}

}  // namespace
