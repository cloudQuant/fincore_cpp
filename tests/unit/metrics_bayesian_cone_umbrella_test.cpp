#include <fincore/metrics.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using compute_bayes_cone_function = fincore::dataframe (*) (
    const fincore::dataframe&, double);

static_assert(std::is_same_v<decltype(&fincore::compute_bayes_cone),
                             compute_bayes_cone_function>);
static_assert(std::is_same_v<
              decltype(fincore::compute_bayes_cone(
                  std::declval<const fincore::dataframe&>())),
              fincore::dataframe>);

constexpr std::array<std::string_view, 7U> cone_column_names{
    fincore::bayesian_cone_percentile_1_column,
    fincore::bayesian_cone_percentile_5_column,
    fincore::bayesian_cone_percentile_25_column,
    fincore::bayesian_cone_percentile_50_column,
    fincore::bayesian_cone_percentile_75_column,
    fincore::bayesian_cone_percentile_95_column,
    fincore::bayesian_cone_percentile_99_column};

TEST(MetricsBayesianConeUmbrella, ExposesBayesianConeOnlyThroughMetricsFacade) {
    EXPECT_EQ(fincore::bayesian_cone_percentile_1_column, "percentile_1");
    EXPECT_EQ(fincore::bayesian_cone_percentile_5_column, "percentile_5");
    EXPECT_EQ(fincore::bayesian_cone_percentile_25_column, "percentile_25");
    EXPECT_EQ(fincore::bayesian_cone_percentile_50_column, "percentile_50");
    EXPECT_EQ(fincore::bayesian_cone_percentile_75_column, "percentile_75");
    EXPECT_EQ(fincore::bayesian_cone_percentile_95_column, "percentile_95");
    EXPECT_EQ(fincore::bayesian_cone_percentile_99_column, "percentile_99");

    fincore::dataframe predicted_returns;
    // Duplicate and unsorted labels are deliberate: rows are consumed positionally and the
    // input index is copied to the result untouched.
    predicted_returns.load_index(
        std::vector<fincore::timestamp>{42, -7, 42});
    predicted_returns.load_column<double>(
        "path_0", std::vector<double>{.10, -.20, .00},
        hmdf::nan_policy::dont_pad_with_nans);
    predicted_returns.load_column<double>(
        "path_1", std::vector<double>{-.10, .20, .00},
        hmdf::nan_policy::dont_pad_with_nans);

    const compute_bayes_cone_function function_pointer =
        &fincore::compute_bayes_cone;
    const auto actual = function_pointer(predicted_returns, 2.0);
    const auto default_start = fincore::compute_bayes_cone(predicted_returns);

    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{42, -7, 42}));
    EXPECT_EQ(actual.shape().first, 3U);
    EXPECT_EQ(actual.shape().second, cone_column_names.size());

    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), cone_column_names.size());
    for (std::size_t position = 0U; position < cone_column_names.size(); ++position) {
        EXPECT_EQ(std::string{std::get<0>(columns[position]).c_str()},
                  std::string{cone_column_names[position]});
        EXPECT_EQ(std::get<1>(columns[position]), 3U);
        EXPECT_EQ(std::get<2>(columns[position]), typeid(double));
    }

    const auto& percentile_1 = actual.get_column<double>(
        fincore::bayesian_cone_percentile_1_column.data());
    const auto& percentile_50 = actual.get_column<double>(
        fincore::bayesian_cone_percentile_50_column.data());
    const auto& percentile_99 = actual.get_column<double>(
        fincore::bayesian_cone_percentile_99_column.data());

    // The path columns are exact sign mirrors, so the middle percentile sits on the
    // starting level at row 0 only; the outer percentiles interpolate between the two
    // wealths, and the row of zero returns repeats the band above it.
    EXPECT_DOUBLE_EQ(percentile_1[0], 1.804);
    EXPECT_DOUBLE_EQ(percentile_50[0], 2.0);
    EXPECT_DOUBLE_EQ(percentile_99[0], 2.196);
    EXPECT_DOUBLE_EQ(percentile_50[1], 1.96);
    EXPECT_DOUBLE_EQ(percentile_1[2], percentile_1[1]);
    EXPECT_DOUBLE_EQ(percentile_50[2], percentile_50[1]);
    EXPECT_DOUBLE_EQ(percentile_99[2], percentile_99[1]);
    EXPECT_DOUBLE_EQ(
        default_start.get_column<double>(
            fincore::bayesian_cone_percentile_50_column.data())[0],
        1.0);
}

}  // namespace
