#include <fincore/metrics/bayesian.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

namespace {

using compute_bayes_cone_function = fincore::dataframe (*) (
    const fincore::dataframe&, double);

static_assert(std::is_same_v<decltype(&fincore::compute_bayes_cone),
                             compute_bayes_cone_function>);
static_assert(std::is_same_v<decltype(fincore::bayesian_cone_percentile_1_column),
                             const std::string_view>);
static_assert(std::is_same_v<decltype(fincore::bayesian_cone_percentile_5_column),
                             const std::string_view>);
static_assert(std::is_same_v<decltype(fincore::bayesian_cone_percentile_25_column),
                             const std::string_view>);
static_assert(std::is_same_v<decltype(fincore::bayesian_cone_percentile_50_column),
                             const std::string_view>);
static_assert(std::is_same_v<decltype(fincore::bayesian_cone_percentile_75_column),
                             const std::string_view>);
static_assert(std::is_same_v<decltype(fincore::bayesian_cone_percentile_95_column),
                             const std::string_view>);
static_assert(std::is_same_v<decltype(fincore::bayesian_cone_percentile_99_column),
                             const std::string_view>);

constexpr double tolerance = 1.0e-12;

constexpr std::array<std::string_view, 7U> cone_column_names{
    fincore::bayesian_cone_percentile_1_column,
    fincore::bayesian_cone_percentile_5_column,
    fincore::bayesian_cone_percentile_25_column,
    fincore::bayesian_cone_percentile_50_column,
    fincore::bayesian_cone_percentile_75_column,
    fincore::bayesian_cone_percentile_95_column,
    fincore::bayesian_cone_percentile_99_column};

fincore::dataframe source_oracle_paths() {
    fincore::dataframe predicted_returns;
    predicted_returns.load_index(
        std::vector<fincore::timestamp>{19, -3, 19, 7});
    predicted_returns.load_column<double>(
        "path_0", std::vector<double>{.01, -.02, .03, 0.0},
        hmdf::nan_policy::dont_pad_with_nans);
    predicted_returns.load_column<double>(
        "path_1", std::vector<double>{0.0, .01, -.01, .02},
        hmdf::nan_policy::dont_pad_with_nans);
    predicted_returns.load_column<double>(
        "path_2", std::vector<double>{-.02, .03, 0.0, .01},
        hmdf::nan_policy::dont_pad_with_nans);
    return predicted_returns;
}

void expect_cone_schema(const fincore::dataframe& actual,
                        const std::vector<fincore::timestamp>& expected_index) {
    EXPECT_EQ(actual.get_index(), expected_index);
    EXPECT_EQ(actual.shape().first, expected_index.size());
    EXPECT_EQ(actual.shape().second, cone_column_names.size());

    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), cone_column_names.size());
    for (std::size_t position = 0U; position < cone_column_names.size(); ++position) {
        EXPECT_STREQ(std::get<0>(columns[position]).c_str(),
                     std::string{cone_column_names[position]}.c_str());
        EXPECT_EQ(std::get<1>(columns[position]), expected_index.size());
        EXPECT_EQ(std::get<2>(columns[position]), typeid(double));
    }
}

TEST(BayesianCone, PublishesExactConstantsSignatureAndDefaultCall) {
    EXPECT_EQ(fincore::bayesian_cone_percentile_1_column, "percentile_1");
    EXPECT_EQ(fincore::bayesian_cone_percentile_5_column, "percentile_5");
    EXPECT_EQ(fincore::bayesian_cone_percentile_25_column, "percentile_25");
    EXPECT_EQ(fincore::bayesian_cone_percentile_50_column, "percentile_50");
    EXPECT_EQ(fincore::bayesian_cone_percentile_75_column, "percentile_75");
    EXPECT_EQ(fincore::bayesian_cone_percentile_95_column, "percentile_95");
    EXPECT_EQ(fincore::bayesian_cone_percentile_99_column, "percentile_99");

    const compute_bayes_cone_function function_pointer =
        &fincore::compute_bayes_cone;
    const auto path = fincore::make_series({4}, "path", {.25});
    const auto explicit_start = function_pointer(path, 1.0);
    const auto default_start = fincore::compute_bayes_cone(path);
    expect_cone_schema(explicit_start, {4});
    expect_cone_schema(default_start, {4});
    for (const std::string_view name : cone_column_names) {
        EXPECT_DOUBLE_EQ(explicit_start.get_column<double>(name.data())[0], 1.25);
        EXPECT_DOUBLE_EQ(default_start.get_column<double>(name.data())[0], 1.25);
    }
}

TEST(BayesianCone, MatchesPythonSourceOracleWithRawIndexAndPathColumns) {
    const auto actual = fincore::compute_bayes_cone(source_oracle_paths(), 2.0);
    expect_cone_schema(actual, {19, -3, 19, 7});

    constexpr std::array<std::array<double, 4U>, 7U> expected{{
        {1.9608, 1.980384, 2.00018, 2.038988},
        {1.964, 1.98352, 2.0017, 2.038988},
        {1.98, 1.9992, 2.0093, 2.038988},
        {2.0, 2.0188, 2.0188, 2.038988},
        {2.01, 2.0194, 2.028894, 2.039392},
        {2.018, 2.01988, 2.0369692, 2.0397152},
        {2.0196, 2.019976, 2.03858424, 2.03977984},
    }};

    for (std::size_t column = 0U; column < cone_column_names.size(); ++column) {
        const auto& actual_values =
            actual.get_column<double>(cone_column_names[column].data());
        ASSERT_EQ(actual_values.size(), expected[column].size());
        for (std::size_t row = 0U; row < expected[column].size(); ++row) {
            EXPECT_NEAR(actual_values[row], expected[column][row], tolerance);
        }
    }
}

TEST(BayesianCone, CompoundsOneNegativeStartPathWithReturnBelowNegativeOne) {
    const auto path = fincore::make_series(
        {9, 2, 9}, "path", {.5, -1.5, .2});
    const auto actual = fincore::compute_bayes_cone(path, -2.0);

    expect_cone_schema(actual, {9, 2, 9});
    constexpr std::array<double, 3U> expected{-3.0, 1.5, 1.8};
    for (const std::string_view name : cone_column_names) {
        const auto& values = actual.get_column<double>(name.data());
        ASSERT_EQ(values.size(), expected.size());
        for (std::size_t row = 0U; row < expected.size(); ++row) {
            EXPECT_DOUBLE_EQ(values[row], expected[row]);
        }
    }
}

TEST(BayesianCone, ReturnsTypedEmptySchemaForTypedEmptyPaths) {
    fincore::dataframe predicted_returns;
    predicted_returns.load_index(std::vector<fincore::timestamp>{});
    predicted_returns.load_column<double>(
        "path_0", std::vector<double>{}, hmdf::nan_policy::dont_pad_with_nans);
    predicted_returns.load_column<double>(
        "path_1", std::vector<double>{}, hmdf::nan_policy::dont_pad_with_nans);

    const auto actual = fincore::compute_bayes_cone(predicted_returns);
    expect_cone_schema(actual, {});
    for (const std::string_view name : cone_column_names) {
        EXPECT_TRUE(actual.get_column<double>(name.data()).empty());
    }
}

TEST(BayesianCone, RejectsInvalidShapesTypesNonfiniteInputsAndArithmetic) {
    fincore::dataframe no_columns;
    no_columns.load_index(std::vector<fincore::timestamp>{1});
    EXPECT_THROW(static_cast<void>(fincore::compute_bayes_cone(no_columns)),
                 fincore::input_error);

    fincore::dataframe non_double;
    non_double.load_index(std::vector<fincore::timestamp>{1});
    non_double.load_column<int>("path", {1}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::compute_bayes_cone(non_double)),
                 fincore::input_error);

    fincore::dataframe mixed;
    mixed.load_index(std::vector<fincore::timestamp>{1});
    mixed.load_column<double>("path", {.01}, hmdf::nan_policy::dont_pad_with_nans);
    mixed.load_column<int>("metadata", {7}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::compute_bayes_cone(mixed)),
                 fincore::input_error);

    fincore::dataframe short_double;
    short_double.load_index(std::vector<fincore::timestamp>{1, 2});
    short_double.load_column<double>("path", {.01},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::compute_bayes_cone(short_double)),
                 fincore::input_error);

    const auto valid = fincore::make_series({1}, "path", {.01});
    for (const double start : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
        EXPECT_THROW(static_cast<void>(fincore::compute_bayes_cone(valid, start)),
                     fincore::input_error);
    }

    for (const double invalid_return : {
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()}) {
        const auto invalid = fincore::make_series({1}, "path", {invalid_return});
        EXPECT_THROW(static_cast<void>(fincore::compute_bayes_cone(invalid)),
                     fincore::input_error);
    }

    const double maximum = std::numeric_limits<double>::max();
    const auto cumulative_overflow = fincore::make_series({1}, "path", {1.0});
    EXPECT_THROW(static_cast<void>(
                     fincore::compute_bayes_cone(cumulative_overflow, maximum)),
                 fincore::input_error);

    fincore::dataframe percentile_overflow;
    percentile_overflow.load_index(std::vector<fincore::timestamp>{1});
    percentile_overflow.load_column<double>("negative_path", {-maximum},
                                             hmdf::nan_policy::dont_pad_with_nans);
    percentile_overflow.load_column<double>("positive_path", {maximum},
                                             hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::compute_bayes_cone(percentile_overflow)),
                 fincore::input_error);
}

TEST(BayesianCone, OwnsReturnedDataAfterSourceMutationAndDestruction) {
    const auto retained = [] {
        auto predicted_returns = source_oracle_paths();
        auto result = fincore::compute_bayes_cone(predicted_returns, 2.0);
        predicted_returns.get_column<double>("path_0")[0] = 42.0;
        predicted_returns.get_column<double>("path_1")[1] = -42.0;
        return result;
    }();

    expect_cone_schema(retained, {19, -3, 19, 7});
    EXPECT_NEAR(retained.get_column<double>(
                    fincore::bayesian_cone_percentile_1_column.data())[0],
                1.9608, tolerance);
    EXPECT_NEAR(retained.get_column<double>(
                    fincore::bayesian_cone_percentile_99_column.data())[3],
                2.03977984, tolerance);
}

}  // namespace
