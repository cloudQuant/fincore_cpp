#include <fincore/simulation/paths.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace {

using gbm_paths_function = fincore::dataframe (*) (
    double, double, double, double, double, std::size_t,
    std::optional<std::uint64_t>);

static_assert(std::is_same_v<decltype(&fincore::geometric_brownian_motion),
                             gbm_paths_function>);

constexpr double tolerance = 1.0e-12;

void expect_path_schema(const fincore::dataframe& paths,
                        const std::size_t expected_step_count,
                        const std::size_t expected_path_count) {
    std::vector<fincore::timestamp> expected_index;
    expected_index.reserve(expected_step_count + 1U);
    for (std::size_t step = 0U; step <= expected_step_count; ++step) {
        expected_index.push_back(static_cast<fincore::timestamp>(step));
    }
    EXPECT_EQ(paths.get_index(), expected_index);
    ASSERT_EQ(paths.shape().first, expected_step_count + 1U);
    ASSERT_EQ(paths.shape().second, expected_path_count);

    const auto columns = paths.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected_path_count);
    for (std::size_t path = 0U; path < expected_path_count; ++path) {
        const std::string expected_name =
            std::string{fincore::gbm_path_column_prefix} + std::to_string(path);
        EXPECT_EQ(std::get<0>(columns[path]), expected_name.c_str());
        EXPECT_EQ(std::get<1>(columns[path]), expected_step_count + 1U);
        EXPECT_EQ(std::get<2>(columns[path]), typeid(double));
    }
}

void expect_path_values(const fincore::dataframe& paths,
                        const std::string_view path_name,
                        const std::vector<double>& expected) {
    const auto& actual = paths.get_column<double>(path_name.data());
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t step = 0U; step < expected.size(); ++step) {
        EXPECT_NEAR(actual[step], expected[step], tolerance) << "step " << step;
    }
}

TEST(GbmPaths, MatchesZeroVolatilityPythonSourceOracleWithNativeTranspose) {
    const auto paths = fincore::geometric_brownian_motion(
        100.0, 0.1, 0.0, 1.0, 0.25, 2U, 42U);
    const std::vector<double> expected{100.0,
                                       102.53151205244289,
                                       105.12710963760242,
                                       107.78841508846317,
                                       110.51709180756478};

    expect_path_schema(paths, 4U, 2U);
    expect_path_values(paths, "path_0", expected);
    expect_path_values(paths, "path_1", expected);
}

TEST(GbmPaths, ReturnsInitialOnlyTableForPythonSubstepHorizon) {
    const auto paths = fincore::geometric_brownian_motion(
        100.0, 0.0, 0.0, 0.1, 0.25, 1U, 42U);

    expect_path_schema(paths, 0U, 1U);
    expect_path_values(paths, "path_0", {100.0});
}

TEST(GbmPaths, PreservesExactTypedSchemaOrdinalIndexAndPathOrder) {
    const auto paths = fincore::geometric_brownian_motion(
        25.0, 0.0, 0.0, 0.5, 0.25, 3U, 19U);

    expect_path_schema(paths, 2U, 3U);
    EXPECT_EQ(paths.get_column<double>("path_0"),
              (std::vector<double>{25.0, 25.0, 25.0}));
    EXPECT_EQ(paths.get_column<double>("path_1"),
              (std::vector<double>{25.0, 25.0, 25.0}));
    EXPECT_EQ(paths.get_column<double>("path_2"),
              (std::vector<double>{25.0, 25.0, 25.0}));
}

TEST(GbmPaths, RepeatsNativeSeedExactlyAndDifferentSeedsDiverge) {
    const auto first = fincore::geometric_brownian_motion(
        100.0, 0.05, 0.2, 1.0, 0.25, 3U, 917U);
    const auto repeated = fincore::geometric_brownian_motion(
        100.0, 0.05, 0.2, 1.0, 0.25, 3U, 917U);
    const auto distinct = fincore::geometric_brownian_motion(
        100.0, 0.05, 0.2, 1.0, 0.25, 3U, 918U);

    expect_path_schema(first, 4U, 3U);
    expect_path_schema(repeated, 4U, 3U);
    expect_path_schema(distinct, 4U, 3U);

    bool diverged = false;
    for (std::size_t path = 0U; path < 3U; ++path) {
        const std::string name = "path_" + std::to_string(path);
        EXPECT_EQ(first.get_column<double>(name.c_str()),
                  repeated.get_column<double>(name.c_str()));
        const auto& first_values = first.get_column<double>(name.c_str());
        const auto& distinct_values = distinct.get_column<double>(name.c_str());
        for (std::size_t step = 1U; step < first_values.size(); ++step) {
            diverged = diverged || first_values[step] != distinct_values[step];
        }
    }
    EXPECT_TRUE(diverged);
}

TEST(GbmPaths, BuildsFiniteUnseededPaths) {
    const auto paths = fincore::geometric_brownian_motion(
        100.0, 0.05, 0.2, 0.5, 0.25, 2U);

    expect_path_schema(paths, 2U, 2U);
    for (const std::string_view name : {"path_0", "path_1"}) {
        for (const double value : paths.get_column<double>(name.data())) {
            EXPECT_TRUE(std::isfinite(value));
        }
    }
}

TEST(GbmPaths, PropagatesZeroNegativeInitialValuesAndFiniteNegativeVolatility) {
    const auto zero = fincore::geometric_brownian_motion(
        0.0, 0.05, 0.2, 1.0, 0.25, 2U, 22U);
    expect_path_schema(zero, 4U, 2U);
    for (const std::string_view name : {"path_0", "path_1"}) {
        EXPECT_EQ(zero.get_column<double>(name.data()),
                  (std::vector<double>{0.0, 0.0, 0.0, 0.0, 0.0}));
    }

    const auto negative = fincore::geometric_brownian_motion(
        -100.0, 0.1, 0.0, 0.5, 0.25, 1U, 22U);
    expect_path_schema(negative, 2U, 1U);
    expect_path_values(negative, "path_0",
                       {-100.0, -102.53151205244289, -105.12710963760242});

    const auto negative_volatility = fincore::geometric_brownian_motion(
        10.0, 0.0, -0.2, 0.25, 0.25, 1U, 22U);
    expect_path_schema(negative_volatility, 1U, 1U);
    EXPECT_TRUE(std::isfinite(negative_volatility.get_column<double>("path_0")[1]));
}

TEST(GbmPaths, OwnsIndexLabelsAndValuesAfterLocalConstruction) {
    const auto retained = [] {
        return fincore::geometric_brownian_motion(
            100.0, 0.1, 0.0, 0.5, 0.25, 2U, 42U);
    }();
    const auto unrelated = fincore::geometric_brownian_motion(
        3.0, 0.0, 0.0, 0.25, 0.25, 1U, 5U);

    expect_path_schema(retained, 2U, 2U);
    expect_path_values(retained, "path_0",
                       {100.0, 102.53151205244289, 105.12710963760242});
    expect_path_values(retained, "path_1",
                       {100.0, 102.53151205244289, 105.12710963760242});
    expect_path_schema(unrelated, 1U, 1U);
}

TEST(GbmPaths, RejectsNonfiniteAndInvalidModelInputs) {
    const std::array<double, 3U> nonfinite{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()};
    for (const double invalid : nonfinite) {
        EXPECT_THROW(fincore::geometric_brownian_motion(
                         invalid, 0.1, 0.2, 1.0, 0.25, 1U, 1U),
                     fincore::input_error);
        EXPECT_THROW(fincore::geometric_brownian_motion(
                         1.0, invalid, 0.2, 1.0, 0.25, 1U, 1U),
                     fincore::input_error);
        EXPECT_THROW(fincore::geometric_brownian_motion(
                         1.0, 0.1, invalid, 1.0, 0.25, 1U, 1U),
                     fincore::input_error);
        EXPECT_THROW(fincore::geometric_brownian_motion(
                         1.0, 0.1, 0.2, invalid, 0.25, 1U, 1U),
                     fincore::input_error);
        EXPECT_THROW(fincore::geometric_brownian_motion(
                         1.0, 0.1, 0.2, 1.0, invalid, 1U, 1U),
                     fincore::input_error);
    }

    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, 0.1, 0.2, 1.0, 0.0, 1U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, 0.1, 0.2, 1.0, -0.25, 1U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, 0.1, 0.2, -0.25, 0.25, 1U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, 0.1, 0.2, 1.0, 0.25, 0U, 1U),
                 fincore::input_error);
}

TEST(GbmPaths, RejectsUnrepresentableAndNonfinitePathArithmetic) {
    const double maximum = std::numeric_limits<double>::max();
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, maximum, 0.0, 1.0, 1.0, 1U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, 0.0, maximum, 1.0, 1.0, 1U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     maximum, 1.0, 0.0, 1.0, 1.0, 1U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, 0.0, 0.0, maximum,
                     std::numeric_limits<double>::denorm_min(), 1U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, 0.0, 0.0,
                     static_cast<double>(std::numeric_limits<fincore::timestamp>::max()),
                     1.0, 1U, 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::geometric_brownian_motion(
                     1.0, 0.0, 0.0, 0.0, 1.0,
                     std::numeric_limits<std::size_t>::max(), 1U),
                 fincore::input_error);
}

}  // namespace
