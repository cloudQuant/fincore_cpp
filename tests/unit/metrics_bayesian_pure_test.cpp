#include <fincore/metrics/bayesian.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

using fincore::dataframe;
using fincore::matrix;
using fincore::timestamp;

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double tolerance = 1.0e-12;

std::vector<std::string> column_names(const dataframe& frame) {
    std::vector<std::string> names;
    for (const auto& entry : frame.get_columns_info<double>()) {
        names.emplace_back(std::get<0>(entry).c_str());
    }
    return names;
}

TEST(MetricsBayesianPure, ConsistencyScoreCountsStrictlyBelowRealizedWealth) {
    const auto returns =
        fincore::make_series({0, 1}, "returns", {0.10, -0.20});
    const matrix predicted{2, 2, {0.10, -0.20, 0.05, 0.10}};

    const auto scores = fincore::compute_consistency_score(returns, predicted);
    ASSERT_EQ(scores.size(), 2U);
    EXPECT_DOUBLE_EQ(scores[0], 0.5);
    EXPECT_DOUBLE_EQ(scores[1], 0.0);
}

TEST(MetricsBayesianPure, ConsistencyScoreAveragesAcrossFourPaths) {
    const auto returns = fincore::make_series({0, 1}, "returns", {0.0, 0.0});
    // Rows are paths and columns are days, compared on each row's compounded wealth rather
    // than on its own return: the all-zero path ties the realized level exactly, and the
    // strict test keeps that tie out of the day-1 count.
    const matrix predicted{4, 2,
                           {0.5, -0.5,
                            -0.5, 0.5,
                            0.0, 0.0,
                            -0.5, -0.5}};

    const auto scores = fincore::compute_consistency_score(returns, predicted);
    ASSERT_EQ(scores.size(), 2U);
    EXPECT_DOUBLE_EQ(scores[0], 0.5);
    EXPECT_DOUBLE_EQ(scores[1], 0.75);
}

TEST(MetricsBayesianPure, ConsistencyScoreTreatsNaNRealizedReturnAsZero) {
    const auto returns =
        fincore::make_series({0, 1}, "returns", {0.10, nan});
    const matrix predicted{2, 2, {0.10, 0.0, 0.0, 0.0}};

    const auto scores = fincore::compute_consistency_score(returns, predicted);
    ASSERT_EQ(scores.size(), 2U);
    EXPECT_DOUBLE_EQ(scores[0], 0.5);
    EXPECT_DOUBLE_EQ(scores[1], 0.5);
}

TEST(MetricsBayesianPure, ConsistencyScoreValidatesShapeAndValues) {
    const auto returns = fincore::make_series({0, 1}, "returns", {0.1, 0.2});
    const matrix mismatched{1, 3, {0.0, 0.0, 0.0}};
    EXPECT_THROW(
        static_cast<void>(
            fincore::compute_consistency_score(returns, mismatched)),
        fincore::input_error);

    const matrix nan_prediction{1, 2, {0.1, nan}};
    EXPECT_THROW(
        static_cast<void>(
            fincore::compute_consistency_score(returns, nan_prediction)),
        fincore::input_error);

    const auto infinite =
        fincore::make_series({0, 1}, "returns",
                             {0.1, std::numeric_limits<double>::infinity()});
    const matrix fine{1, 2, {0.0, 0.0}};
    EXPECT_THROW(
        static_cast<void>(
            fincore::compute_consistency_score(infinite, fine)),
        fincore::input_error);

    const matrix no_paths{0, 2, {}};
    const auto empty_scores =
        fincore::compute_consistency_score(returns, no_paths);
    ASSERT_EQ(empty_scores.size(), 2U);
    EXPECT_TRUE(std::isnan(empty_scores[0]));
    EXPECT_TRUE(std::isnan(empty_scores[1]));
}

TEST(MetricsBayesianPure, SimulatePathsHasRequestedShapeAndIsDeterministic) {
    const std::vector<double> in_sample{0.01, 0.02, 0.03, -0.01};
    const auto first =
        fincore::simulate_paths(in_sample, 4, 1.0, 5, 42U);

    EXPECT_EQ(first.rows(), 5U);
    EXPECT_EQ(first.columns(), 4U);
    ASSERT_EQ(first.values().size(), 20U);
    for (const double value : first.values()) {
        EXPECT_TRUE(value == 0.01 || value == 0.02 || value == 0.03 ||
                    value == -0.01);
    }

    const auto second =
        fincore::simulate_paths(in_sample, 4, 1.0, 5, 42U);
    const std::vector<double> first_values{first.values().begin(),
                                           first.values().end()};
    const std::vector<double> second_values{second.values().begin(),
                                            second.values().end()};
    EXPECT_EQ(first_values, second_values);

    const auto different_seed =
        fincore::simulate_paths(in_sample, 4, 1.0, 5, 43U);
    const std::vector<double> different_values{
        different_seed.values().begin(), different_seed.values().end()};
    EXPECT_NE(first_values, different_values);

    // simulate_paths keeps starting_value for signature parity and discards it, so any
    // value gives the same draw; the cone level is applied later, in summarize_paths.
    const auto ignored_start =
        fincore::simulate_paths(in_sample, 4, 7.5, 5, 42U);
    const std::vector<double> ignored_values{ignored_start.values().begin(),
                                             ignored_start.values().end()};
    EXPECT_EQ(first_values, ignored_values);
}

TEST(MetricsBayesianPure, SimulatePathsHandlesZeroDimensionsAndEmptyInputs) {
    const std::vector<double> in_sample{0.01, 0.02};

    const auto no_days =
        fincore::simulate_paths(in_sample, 0, 1.0, 3, 1U);
    EXPECT_EQ(no_days.rows(), 3U);
    EXPECT_EQ(no_days.columns(), 0U);
    EXPECT_TRUE(no_days.values().empty());

    const auto no_samples =
        fincore::simulate_paths(in_sample, 4, 1.0, 0, 1U);
    EXPECT_EQ(no_samples.rows(), 0U);
    EXPECT_EQ(no_samples.columns(), 4U);
    EXPECT_TRUE(no_samples.values().empty());

    const auto empty_draw =
        fincore::simulate_paths(std::span<const double>{}, 0, 1.0, 0, 1U);
    EXPECT_EQ(empty_draw.rows(), 0U);
    EXPECT_EQ(empty_draw.columns(), 0U);

    EXPECT_THROW(
        static_cast<void>(fincore::simulate_paths(
            std::span<const double>{}, 2, 1.0, 3, 1U)),
        fincore::input_error);
}

TEST(MetricsBayesianPure, SimulatePathsPreservesNaNAndRejectsInfinity) {
    const std::vector<double> only_nan{nan};
    const auto paths = fincore::simulate_paths(only_nan, 3, 1.0, 2, 5U);
    ASSERT_EQ(paths.values().size(), 6U);
    for (const double value : paths.values()) {
        EXPECT_TRUE(std::isnan(value));
    }

    const std::vector<double> infinite{
        std::numeric_limits<double>::infinity()};
    EXPECT_THROW(
        static_cast<void>(fincore::simulate_paths(infinite, 2, 1.0, 2, 5U)),
        fincore::input_error);
}

TEST(MetricsBayesianPure, SimulatePathsFrameOverloadMatchesVectorOverload) {
    const auto frame =
        fincore::make_series({10, 20, 30}, "returns", {0.01, -0.02, 0.03});
    const std::vector<double> values{0.01, -0.02, 0.03};

    const auto from_frame =
        fincore::simulate_paths(frame, 4, 1.0, 6, 17U);
    const auto from_values =
        fincore::simulate_paths(values, 4, 1.0, 6, 17U);
    const std::vector<double> frame_values{from_frame.values().begin(),
                                           from_frame.values().end()};
    const std::vector<double> vector_values{from_values.values().begin(),
                                            from_values.values().end()};
    EXPECT_EQ(frame_values, vector_values);

    EXPECT_THROW(
        static_cast<void>(
            fincore::simulate_paths(frame, 2, 1.0, 3, 1U, "missing")),
        fincore::input_error);
}

TEST(MetricsBayesianPure, SummarizePathsMatchesHandComputedCone) {
    const matrix samples{2, 2, {0.10, -0.10, 0.20, 0.00}};
    const auto cone = fincore::summarize_paths(samples);

    EXPECT_EQ(cone.get_index(), (std::vector<timestamp>{0, 1}));
    EXPECT_EQ(column_names(cone),
              (std::vector<std::string>{"cone_1", "cone_-1", "cone_1.5",
                                        "cone_-1.5", "cone_2", "cone_-2"}));

    const auto& upper_one = cone.get_column<double>("cone_1");
    const auto& lower_one = cone.get_column<double>("cone_-1");
    const auto& upper_fifteen = cone.get_column<double>("cone_1.5");
    const auto& lower_fifteen = cone.get_column<double>("cone_-1.5");
    const auto& upper_two = cone.get_column<double>("cone_2");
    const auto& lower_two = cone.get_column<double>("cone_-2");
    ASSERT_EQ(upper_one.size(), 2U);

    EXPECT_NEAR(upper_one[0], 1.2207106781186545, tolerance);
    EXPECT_NEAR(lower_one[0], 1.0792893218813453, tolerance);
    EXPECT_NEAR(upper_fifteen[0], 1.2560660171779818, tolerance);
    EXPECT_NEAR(lower_fifteen[0], 1.043933982822018, tolerance);
    EXPECT_NEAR(upper_two[0], 1.2914213562373091, tolerance);
    EXPECT_NEAR(lower_two[0], 1.0085786437626907, tolerance);

    EXPECT_NEAR(upper_one[1], 1.243492424049175, tolerance);
    EXPECT_NEAR(lower_one[1], 0.9465075759508251, tolerance);
    EXPECT_NEAR(upper_fifteen[1], 1.3177386360737624, tolerance);
    EXPECT_NEAR(lower_fifteen[1], 0.8722613639262377, tolerance);
    EXPECT_NEAR(upper_two[1], 1.3919848480983497, tolerance);
    EXPECT_NEAR(lower_two[1], 0.7980151519016503, tolerance);
}

TEST(MetricsBayesianPure, SummarizePathsSupportsScalarWidthAndZeroStart) {
    const matrix samples{2, 2, {0.10, -0.10, 0.20, 0.00}};
    const auto scalar = fincore::summarize_paths(samples, 1.5);
    EXPECT_EQ(column_names(scalar),
              (std::vector<std::string>{"cone_1.5", "cone_-1.5"}));
    EXPECT_NEAR(scalar.get_column<double>("cone_1.5")[0],
                1.2560660171779818, tolerance);
    EXPECT_NEAR(scalar.get_column<double>("cone_-1.5")[1],
                0.8722613639262377, tolerance);

    const auto zero_start = fincore::summarize_paths(samples, 1.0, 0.0);
    EXPECT_NEAR(zero_start.get_column<double>("cone_1")[0],
                0.2207106781186548, tolerance);
    EXPECT_NEAR(zero_start.get_column<double>("cone_-1")[1],
                -0.0534924240491749, tolerance);
}

TEST(MetricsBayesianPure, SummarizePathsHandlesNaNEmptyAndInvalidShapes) {
    const matrix one_nan{1, 1, {nan}};
    const auto nan_cone = fincore::summarize_paths(one_nan, 1.0);
    EXPECT_TRUE(std::isnan(nan_cone.get_column<double>("cone_1")[0]));
    EXPECT_TRUE(std::isnan(nan_cone.get_column<double>("cone_-1")[0]));

    const matrix no_paths{0, 2, {}};
    const auto empty_cone = fincore::summarize_paths(no_paths, 1.0);
    EXPECT_EQ(empty_cone.get_index(), (std::vector<timestamp>{0, 1}));
    EXPECT_TRUE(std::isnan(empty_cone.get_column<double>("cone_1")[0]));
    EXPECT_TRUE(std::isnan(empty_cone.get_column<double>("cone_-1")[1]));

    const auto no_widths =
        fincore::summarize_paths(one_nan, std::span<const double>{});
    EXPECT_EQ(no_widths.shape().first, 0U);
    EXPECT_EQ(no_widths.shape().second, 0U);

    const std::array<double, 2> duplicate_widths{1.0, 1.0};
    EXPECT_THROW(static_cast<void>(fincore::summarize_paths(
                     one_nan, duplicate_widths)),
                 fincore::input_error);

    const std::array<double, 1> invalid_widths{nan};
    EXPECT_THROW(static_cast<void>(fincore::summarize_paths(
                     one_nan, invalid_widths)),
                 fincore::input_error);

    EXPECT_THROW(
        static_cast<void>(fincore::summarize_paths(one_nan, 1.0, nan)),
        fincore::input_error);
}

TEST(MetricsBayesianPure, ForecastConeComposesSimulationAndSummary) {
    const auto frame =
        fincore::make_series({1, 2, 3, 4}, "returns",
                             {0.01, -0.02, 0.03, 0.00});
    const std::vector<double> values{0.01, -0.02, 0.03, 0.00};
    const std::array<double, 2> widths{1.0, 2.0};
    constexpr std::size_t days = 5U;
    constexpr std::size_t samples = 7U;
    constexpr double starting_value = 1.5;
    constexpr std::uint32_t seed = 7U;

    const auto simulated = fincore::simulate_paths(
        values, days, starting_value, samples, seed);
    const auto expected = fincore::summarize_paths(
        simulated, widths, starting_value);
    const auto actual = fincore::forecast_cone_bootstrap(
        frame, days, widths, starting_value, samples, seed);

    EXPECT_EQ(actual.get_index(), expected.get_index());
    EXPECT_EQ(column_names(actual), column_names(expected));
    ASSERT_EQ(actual.shape().second, expected.shape().second);
    for (const std::string& name : column_names(expected)) {
        const auto& expected_values = expected.get_column<double>(name.c_str());
        const auto& actual_values = actual.get_column<double>(name.c_str());
        ASSERT_EQ(actual_values.size(), expected_values.size());
        for (std::size_t row = 0U; row < expected_values.size(); ++row) {
            EXPECT_DOUBLE_EQ(actual_values[row], expected_values[row]);
        }
    }

    const std::array<double, 1> single_width{1.5};
    const auto expected_single = fincore::summarize_paths(
        fincore::simulate_paths(values, days, starting_value, samples, seed),
        single_width, starting_value);
    const auto actual_single = fincore::forecast_cone_bootstrap(
        frame, days, 1.5, starting_value, samples, seed);
    EXPECT_EQ(column_names(actual_single), column_names(expected_single));
    for (const std::string& name : column_names(expected_single)) {
        const auto& expected_values =
            expected_single.get_column<double>(name.c_str());
        const auto& actual_values =
            actual_single.get_column<double>(name.c_str());
        for (std::size_t row = 0U; row < expected_values.size(); ++row) {
            EXPECT_DOUBLE_EQ(actual_values[row], expected_values[row]);
        }
    }

    const auto default_call = fincore::forecast_cone_bootstrap(frame, 3);
    EXPECT_EQ(default_call.get_index(),
              (std::vector<timestamp>{0, 1, 2}));
    EXPECT_EQ(column_names(default_call),
              (std::vector<std::string>{"cone_1", "cone_-1", "cone_1.5",
                                        "cone_-1.5", "cone_2", "cone_-2"}));

    EXPECT_THROW(
        static_cast<void>(fincore::forecast_cone_bootstrap(
            frame, 2, widths, 1.0, 3, 1U, "missing")),
        fincore::input_error);
}

}  // namespace
