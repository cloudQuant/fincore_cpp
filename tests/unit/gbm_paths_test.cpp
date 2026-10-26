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

using calibrated_paths_function = fincore::dataframe (*) (
    const fincore::dataframe&, std::size_t, std::size_t, std::size_t,
    std::optional<std::uint64_t>, std::string_view);

static_assert(std::is_same_v<decltype(&fincore::geometric_brownian_motion),
                             gbm_paths_function>);
static_assert(std::is_same_v<decltype(&fincore::gbm_from_returns),
                             calibrated_paths_function>);

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

void expect_calibrated_schema(const fincore::dataframe& paths,
                              const std::size_t expected_row_count,
                              const std::size_t expected_path_count) {
    std::vector<fincore::timestamp> expected_index;
    expected_index.reserve(expected_row_count);
    for (std::size_t row = 1U; row <= expected_row_count; ++row) {
        expected_index.push_back(static_cast<fincore::timestamp>(row));
    }
    EXPECT_EQ(paths.get_index(), expected_index);
    ASSERT_EQ(paths.shape().first, expected_row_count);
    ASSERT_EQ(paths.shape().second, expected_path_count);

    const auto columns = paths.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected_path_count);
    for (std::size_t path = 0U; path < expected_path_count; ++path) {
        const std::string expected_name =
            std::string{fincore::gbm_path_column_prefix} + std::to_string(path);
        EXPECT_EQ(std::get<0>(columns[path]), expected_name.c_str());
        EXPECT_EQ(std::get<1>(columns[path]), expected_row_count);
        EXPECT_EQ(std::get<2>(columns[path]), typeid(double));
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

TEST(GbmPaths, CalibratedPathsMatchZeroVolatilityPythonOracleAndSchema) {
    const auto paths = fincore::gbm_from_returns(
        fincore::make_series({19, -3, 19, 7}, "returns",
                             {0.01, 0.01, 0.01, 0.01}),
        4U, 2U, 4U, 42U);

    expect_calibrated_schema(paths, 4U, 2U);
    const std::vector<double> expected{0.0025031276057951857,
                                       0.005012520859401182,
                                       0.0075281954445340915,
                                       0.010050167084168393};
    expect_path_values(paths, "path_0", expected);
    expect_path_values(paths, "path_1", expected);
}

TEST(GbmPaths, CalibratedPathsPreserveSourceDoubleTruncationAndZeroStepShape) {
    const auto truncated = fincore::gbm_from_returns(
        fincore::make_series({19, -3, 19, 7}, "returns",
                             {0.01, 0.01, 0.01, 0.01}),
        3U, 1U, 5U, 42U);
    expect_calibrated_schema(truncated, 2U, 1U);
    expect_path_values(truncated, "path_0",
                       {0.002002001334000303, 0.004008010677341911});

    const auto zero_step = fincore::gbm_from_returns(
        fincore::make_series({19, -3, 19, 7}, "returns",
                             {0.01, 0.01, 0.01, 0.01}),
        0U, 2U, 4U, 42U);
    expect_calibrated_schema(zero_step, 0U, 2U);
    EXPECT_TRUE(zero_step.get_column<double>("path_0").empty());
    EXPECT_TRUE(zero_step.get_column<double>("path_1").empty());
}

TEST(GbmPaths, CalibratedPathsDelegateParametersAndNativeGbmExactly) {
    const auto returns = fincore::make_series(
        {7, -3, 7, 19, 2}, "returns", {0.01, -0.02, 0.03, 0.0, 0.015});
    constexpr std::size_t horizon = 7U;
    constexpr std::size_t path_count = 3U;
    constexpr std::size_t periods_per_year = 4U;
    constexpr std::uint64_t seed = 917U;

    const auto actual = fincore::gbm_from_returns(
        returns, horizon, path_count, periods_per_year, seed);
    const auto parameters =
        fincore::estimate_simulation_parameters(returns, periods_per_year);
    const auto prices = fincore::geometric_brownian_motion(
        1.0, parameters.drift / static_cast<double>(periods_per_year),
        parameters.volatility / std::sqrt(static_cast<double>(periods_per_year)),
        static_cast<double>(horizon) / static_cast<double>(periods_per_year),
        1.0 / static_cast<double>(periods_per_year), path_count, seed);

    expect_calibrated_schema(actual, prices.get_index().size() - 1U, path_count);
    for (std::size_t path = 0U; path < path_count; ++path) {
        const std::string name = "path_" + std::to_string(path);
        const auto& actual_values = actual.get_column<double>(name.c_str());
        const auto& price_values = prices.get_column<double>(name.c_str());
        ASSERT_EQ(actual_values.size(), price_values.size() - 1U);
        for (std::size_t row = 0U; row < actual_values.size(); ++row) {
            EXPECT_NEAR(actual_values[row], price_values[row + 1U] - 1.0,
                        tolerance)
                << "path " << path << ", row " << row;
        }
    }

    const auto repeated = fincore::gbm_from_returns(
        returns, horizon, path_count, periods_per_year, seed);
    const auto distinct = fincore::gbm_from_returns(
        returns, horizon, path_count, periods_per_year, seed + 1U);
    bool diverged = false;
    for (std::size_t path = 0U; path < path_count; ++path) {
        const std::string name = "path_" + std::to_string(path);
        EXPECT_EQ(actual.get_column<double>(name.c_str()),
                  repeated.get_column<double>(name.c_str()));
        diverged = diverged || actual.get_column<double>(name.c_str()) !=
                                  distinct.get_column<double>(name.c_str());
    }
    EXPECT_TRUE(diverged);
}

TEST(GbmPaths, CalibratedPathsOwnResultAndPreserveNamedReturnColumn) {
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{19, -3, 19, 7});
    returns.load_column<double>("strategy", {0.01, 0.01, 0.01, 0.01},
                                hmdf::nan_policy::dont_pad_with_nans);
    std::array<char, 8U> name_storage{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view strategy{name_storage.data(), name_storage.size()};

    auto paths = fincore::gbm_from_returns(returns, 4U, 2U, 4U, 42U, strategy);
    const auto expected_index = paths.get_index();
    const auto expected_values = paths.get_column<double>("path_0");
    returns.get_column<double>("strategy")[0] = -0.99;
    name_storage[0] = 'x';

    EXPECT_EQ(paths.get_index(), expected_index);
    EXPECT_EQ(paths.get_column<double>("path_0"), expected_values);
    EXPECT_EQ(paths.get_column<double>("path_1"), expected_values);
}

TEST(GbmPaths, CalibratedPathsRejectStructuralSelectorAndNumericalBoundaries) {
    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{1, 2});
    EXPECT_THROW(fincore::gbm_from_returns(missing), fincore::input_error);

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1, 2});
    wrong_type.load_column<int>("returns", {1, 2},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::gbm_from_returns(wrong_type), fincore::input_error);

    fincore::dataframe short_column;
    short_column.load_index(std::vector<fincore::timestamp>{1, 2});
    short_column.load_column<double>("returns", {0.01},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::gbm_from_returns(short_column), fincore::input_error);

    const auto empty = fincore::make_series({}, "returns", {});
    EXPECT_THROW(fincore::gbm_from_returns(empty), fincore::input_error);

    const auto valid = fincore::make_series({1, 2}, "returns", {0.01, 0.01});
    EXPECT_THROW(fincore::gbm_from_returns(valid, 1U, 1U, 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::gbm_from_returns(valid, 1U, 0U, 1U),
                 fincore::input_error);

    const auto nan_only = fincore::make_series(
        {1, 2}, "returns", {std::numeric_limits<double>::quiet_NaN(),
                              std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(fincore::gbm_from_returns(nan_only), fincore::input_error);
    const auto one_retained = fincore::make_series(
        {1, 2}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN()});
    EXPECT_THROW(fincore::gbm_from_returns(one_retained), fincore::input_error);
    const auto infinite = fincore::make_series(
        {1, 2}, "returns", {0.01, std::numeric_limits<double>::infinity()});
    EXPECT_THROW(fincore::gbm_from_returns(infinite), fincore::input_error);

    constexpr std::array<char, 12U> embedded_nul_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l'};
    const std::string_view embedded_nul{embedded_nul_storage.data(),
                                        embedded_nul_storage.size()};
    EXPECT_THROW(fincore::gbm_from_returns(valid, 1U, 1U, 1U, 42U, embedded_nul),
                 fincore::input_error);

    EXPECT_THROW(fincore::gbm_from_returns(
                     valid, 0U, std::numeric_limits<std::size_t>::max(), 1U),
                 fincore::input_error);
    EXPECT_THROW(fincore::gbm_from_returns(
                     valid, std::numeric_limits<std::size_t>::max(), 1U, 1U),
                 fincore::input_error);
    const double maximum = std::numeric_limits<double>::max();
    const auto overflowing_path =
        fincore::make_series({1, 2}, "returns", {maximum, maximum});
    EXPECT_THROW(fincore::gbm_from_returns(overflowing_path, 2U, 1U, 1U, 42U),
                 fincore::input_error);
}

}  // namespace
