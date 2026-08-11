#include <fincore/metrics/returns.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

void expect_values(const std::vector<double>& actual,
                   const std::vector<double>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t position = 0U; position < expected.size(); ++position) {
        if (std::isnan(expected[position])) {
            EXPECT_TRUE(std::isnan(actual[position])) << "position " << position;
        } else if (std::isinf(expected[position])) {
            EXPECT_EQ(actual[position], expected[position]) << "position " << position;
        } else {
            EXPECT_NEAR(actual[position], expected[position], 1.0e-12)
                << "position " << position;
        }
    }
}

void expect_empty_double_series(const fincore::dataframe& actual,
                                std::string_view column) {
    const std::string name{column};
    EXPECT_TRUE(actual.get_index().empty());
    ASSERT_TRUE(actual.has_column(name.c_str()));
    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(columns.front()).c_str(), name.c_str());
    EXPECT_EQ(std::get<2>(columns.front()), std::type_index(typeid(double)));
    EXPECT_TRUE(actual.get_column<double>(name.c_str()).empty());
}

void expect_aggregation_schema(const fincore::dataframe& actual,
                               std::string_view return_column) {
    const std::string return_name{return_column};
    EXPECT_EQ(actual.shape().second, 3U);
    const auto columns = actual.get_columns_info<int, double>();
    ASSERT_EQ(columns.size(), 3U);
    EXPECT_STREQ(std::get<0>(columns[0]).c_str(),
                 std::string{fincore::return_aggregation_year_column}.c_str());
    EXPECT_STREQ(std::get<0>(columns[1]).c_str(),
                 std::string{fincore::return_aggregation_period_column}.c_str());
    EXPECT_STREQ(std::get<0>(columns[2]).c_str(), return_name.c_str());
    EXPECT_EQ(std::get<2>(columns[0]), std::type_index(typeid(int)));
    EXPECT_EQ(std::get<2>(columns[1]), std::type_index(typeid(int)));
    EXPECT_EQ(std::get<2>(columns[2]), std::type_index(typeid(double)));
}

void expect_aggregation_labels(const fincore::dataframe& actual,
                               const std::vector<int>& expected_years,
                               const std::vector<int>& expected_periods) {
    EXPECT_EQ(actual.get_column<int>(
                  fincore::return_aggregation_year_column.data()),
              expected_years);
    EXPECT_EQ(actual.get_column<int>(
                  fincore::return_aggregation_period_column.data()),
              expected_periods);
}

fincore::dataframe aggregation_fixture() {
    return fincore::make_series(
        {fincore::utc_timestamp(2020, 12, 28, 15),
         fincore::utc_timestamp(2020, 12, 31, 9),
         fincore::utc_timestamp(2021, 1, 1, 11),
         fincore::utc_timestamp(2021, 1, 3, 18),
         fincore::utc_timestamp(2021, 1, 4, 8),
         fincore::utc_timestamp(2021, 3, 31, 12),
         fincore::utc_timestamp(2021, 4, 1, 12),
         fincore::utc_timestamp(2021, 12, 31, 12),
         fincore::utc_timestamp(2022, 1, 1, 12)},
        "strategy", {.1, .02, .03, nan, -.1, .1, .2, .01, .02});
}

TEST(Returns, ComputesSimpleAndCumulativeReturnsWithOriginalIndex) {
    const auto prices =
        fincore::make_series({1, 2, 3}, "price", {100.0, 110.0, 99.0});

    const auto simple = fincore::simple_returns(prices, "price");

    EXPECT_EQ(simple.get_index(), (std::vector<fincore::timestamp>{2, 3}));
    const auto& simple_values =
        simple.get_column<double>(fincore::returns_column.data());
    ASSERT_EQ(simple_values.size(), 2U);
    EXPECT_NEAR(simple_values[0], 0.10, 1e-12);
    EXPECT_NEAR(simple_values[1], -0.10, 1e-12);
    EXPECT_NEAR(fincore::cumulative_return(simple), -0.01, 1e-12);
}

TEST(Returns, TreatsNanAsZeroDuringCumulativeCompounding) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.02});

    const auto cumulative = fincore::cumulative_returns(returns);
    const auto& values =
        cumulative.get_column<double>(fincore::returns_column.data());

    EXPECT_EQ(cumulative.get_index(), returns.get_index());
    EXPECT_NEAR(values[0], 0.01, 1e-12);
    EXPECT_NEAR(values[1], 0.01, 1e-12);
    EXPECT_NEAR(values[2], 0.0302, 1e-12);
}

TEST(Returns, CumulativeReturnCompoundsNanSafely) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.02});

    EXPECT_NEAR(fincore::cumulative_return(returns), 0.0302, 1e-12);
}

TEST(Returns, HandlesEmptyAndNonPositiveEndingWealth) {
    const auto wiped_out = fincore::make_series({1}, "returns", {-1.0});
    const auto empty = fincore::make_series({}, "returns", {});

    EXPECT_DOUBLE_EQ(fincore::annual_return(wiped_out), -1.0);
    EXPECT_TRUE(std::isnan(fincore::annual_return(empty)));
}

TEST(Returns, AnnualReturnUsesRequestedAnnualization) {
    const auto returns = fincore::make_series({1, 2}, "returns", {0.10, 0.10});

    EXPECT_NEAR(fincore::annual_return(returns, 2.0), 0.21, 1e-12);
}

TEST(Returns, IgnoresNanSamplesForAnnualVolatility) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.03});

    EXPECT_NEAR(fincore::annual_volatility(returns), 0.2244994432064365,
                1e-12);
}

TEST(Returns, AnnualVolatilityRequiresTwoValidSamples) {
    const auto returns = fincore::make_series(
        {1, 2}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN()});

    EXPECT_TRUE(std::isnan(fincore::annual_volatility(returns)));
}

TEST(Returns, ValidatesRequestedInputColumn) {
    const auto returns = fincore::make_series({1}, "returns", {0.01});

    EXPECT_THROW(fincore::simple_returns(returns, "missing"),
                 fincore::input_error);
}

TEST(Returns, PublicHeaderDeclaresNativeReturnTransformations) {
    const auto returns = fincore::make_series({1}, "returns", {0.01});

    static_cast<void>(fincore::cumulative_returns(returns, 100.0));
    static_cast<void>(fincore::cumulative_return(returns, 100.0));
    static_cast<void>(fincore::normalize(returns));
    static_cast<void>(fincore::aggregate_returns(
        returns, fincore::return_aggregation_period::monthly));
}

TEST(Returns, PreservesZeroStartingValueCompatibilityAndSupportsNumericStarts) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                               {.1, nan, -.1});

    expect_values(fincore::cumulative_returns(returns)
                      .get_column<double>(fincore::returns_column.data()),
                  {.1, .1, -.01});
    EXPECT_NEAR(fincore::cumulative_return(returns), -.01, 1.0e-12);

    expect_values(fincore::cumulative_returns(returns, -0.0)
                      .get_column<double>(fincore::returns_column.data()),
                  {.1, .1, -.01});
    EXPECT_NEAR(fincore::cumulative_return(returns, -0.0), -.01, 1.0e-12);

    expect_values(fincore::cumulative_returns(returns, 100.0)
                      .get_column<double>(fincore::returns_column.data()),
                  {110.0, 110.0, 99.0});
    EXPECT_NEAR(fincore::cumulative_return(returns, 100.0), 99.0, 1.0e-12);

    expect_values(fincore::cumulative_returns(returns, -2.0)
                      .get_column<double>(fincore::returns_column.data()),
                  {-2.2, -2.2, -1.98});
    EXPECT_NEAR(fincore::cumulative_return(returns, -2.0), -1.98, 1.0e-12);

    const auto empty = fincore::make_series({}, "strategy", {});
    expect_empty_double_series(fincore::cumulative_returns(empty, 100.0, "strategy"),
                               fincore::returns_column);
    EXPECT_TRUE(
        std::isnan(fincore::cumulative_return(empty, 100.0, "strategy")));
}

TEST(Returns, PreservesBracedEmptyCumulativeColumnValidation) {
    const auto returns = fincore::make_series({1}, "returns", {.01});

    EXPECT_THROW(fincore::cumulative_returns(returns, {}), fincore::input_error);
    EXPECT_THROW(fincore::cumulative_return(returns, {}), fincore::input_error);
}

TEST(Returns, ForwardsNarrowLegacyCumulativeColumnInitializerLists) {
    const auto returns = fincore::make_series({1}, "strategy", {.01});

    expect_values(fincore::cumulative_returns(returns, {"strategy"})
                      .get_column<double>(fincore::returns_column.data()),
                  {.01});
    EXPECT_NEAR(fincore::cumulative_return(returns, {"strategy"}), .01,
                1.0e-12);
    EXPECT_THROW(fincore::cumulative_returns(returns, {"strategy", "other"}),
                 fincore::input_error);
    EXPECT_THROW(fincore::cumulative_return(returns, {"strategy", "other"}),
                 fincore::input_error);
}

TEST(Returns, PreservesAnnualReturnCancellationArithmetic) {
    const auto returns = fincore::make_series({1, 2, 3, 4, 5, 6}, "returns",
                                               {-.999, -.999, -.999,
                                                -.999, -.999, -.999});

    EXPECT_DOUBLE_EQ(fincore::annual_return(returns, .1), -1.0);
}

TEST(Returns, NormalizesSingleDoubleSeriesWithPythonBoundarySemantics) {
    const auto values =
        fincore::make_series({11, 12, 13}, "equity", {100.0, 110.0, 90.0});

    const auto default_values =
        fincore::make_series({11, 12, 13}, "returns", {100.0, 110.0, 90.0});
    const auto default_normalized = fincore::normalize(default_values);
    EXPECT_EQ(default_normalized.get_index(), default_values.get_index());
    expect_values(default_normalized.get_column<double>("returns"), {1.0, 1.1, .9});

    const auto negative_normalized = fincore::normalize(values, -2.0, "equity");
    EXPECT_EQ(negative_normalized.get_index(), values.get_index());
    expect_values(negative_normalized.get_column<double>("equity"),
                  {-2.0, -2.2, -1.8});

    const auto empty = fincore::make_series({}, "equity", {});
    expect_empty_double_series(fincore::normalize(empty, -2.0, "equity"), "equity");

    const auto zero_base = fincore::make_series({1, 2, 3}, "equity",
                                                 {0.0, 10.0, nan});
    const auto negative_zero_base = fincore::make_series({1, 2, 3}, "equity",
                                                          {-0.0, 10.0, nan});
    for (const auto* input : {&zero_base, &negative_zero_base}) {
        const auto normalized = fincore::normalize(*input, -2.0, "equity");
        EXPECT_EQ(normalized.get_index(), input->get_index());
        for (const double value : normalized.get_column<double>("equity")) {
            EXPECT_TRUE(std::isnan(value));
        }
    }

    const auto infinite_values =
        fincore::make_series({1, 2, 3}, "equity", {2.0, infinity, 4.0});
    expect_values(fincore::normalize(infinite_values, 1.0, "equity")
                      .get_column<double>("equity"),
                  {1.0, infinity, 2.0});
}

TEST(Returns, AggregatesWeeklyReturnsWithCalendarYearAndIsoWeekLabels) {
    const auto actual = fincore::aggregate_returns(
        aggregation_fixture(), fincore::return_aggregation_period::weekly,
        "strategy");

    expect_aggregation_schema(actual, "strategy");
    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{
                  fincore::utc_timestamp(2020, 12, 28),
                  fincore::utc_timestamp(2021, 1, 4),
                  fincore::utc_timestamp(2021, 3, 29),
                  fincore::utc_timestamp(2021, 12, 27),
                  fincore::utc_timestamp(2021, 1, 1),
                  fincore::utc_timestamp(2022, 1, 1)}));
    expect_aggregation_labels(actual, {2020, 2021, 2021, 2021, 2021, 2022},
                              {53, 1, 13, 52, 53, 52});
    expect_values(actual.get_column<double>("strategy"),
                  {.122, -.1, .32, .01, .03, .02});
}

TEST(Returns, AggregatesWeeklyCollisionWithLabelDerivedCanonicalIndex) {
    const auto january_then_december = fincore::make_series(
        {fincore::utc_timestamp(2018, 1, 1),
         fincore::utc_timestamp(2018, 12, 31)},
        "strategy", {.1, .2});
    const auto december_then_january = fincore::make_series(
        {fincore::utc_timestamp(2018, 12, 31),
         fincore::utc_timestamp(2018, 1, 1)},
        "strategy", {.2, .1});

    for (const fincore::dataframe* input :
         {&january_then_december, &december_then_january}) {
        const auto actual = fincore::aggregate_returns(
            *input, fincore::return_aggregation_period::weekly, "strategy");

        expect_aggregation_schema(actual, "strategy");
        EXPECT_EQ(actual.get_index(),
                  (std::vector<fincore::timestamp>{
                      fincore::utc_timestamp(2018, 1, 1)}));
        expect_aggregation_labels(actual, {2018}, {1});
        expect_values(actual.get_column<double>("strategy"), {.32});
    }
}

TEST(Returns, AggregationPreservesOriginalInputOrderWithinEachGroup) {
    const auto overflow_first = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 3),
         fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 1)},
        "strategy", {1.0e160, 1.0e160, -.9999999999999999});
    const auto cancellation_first = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 1),
         fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 3)},
        "strategy", {-.9999999999999999, 1.0e160, 1.0e160});

    const auto overflow_result = fincore::aggregate_returns(
        overflow_first, fincore::return_aggregation_period::monthly, "strategy");
    const auto cancellation_result = fincore::aggregate_returns(
        cancellation_first, fincore::return_aggregation_period::monthly,
        "strategy");

    const auto& overflow_values = overflow_result.get_column<double>("strategy");
    const auto& cancellation_values =
        cancellation_result.get_column<double>("strategy");
    ASSERT_EQ(overflow_values.size(), 1U);
    ASSERT_EQ(cancellation_values.size(), 1U);
    EXPECT_EQ(overflow_values.front(), infinity);
    EXPECT_TRUE(std::isfinite(cancellation_values.front()));
    EXPECT_GT(cancellation_values.front(), 0.0);
}

TEST(Returns, AggregatesMonthlyReturnsWithCanonicalUtcBucketStarts) {
    const auto actual = fincore::aggregate_returns(
        aggregation_fixture(), fincore::return_aggregation_period::monthly,
        "strategy");

    expect_aggregation_schema(actual, "strategy");
    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{
                  fincore::utc_timestamp(2020, 12, 1),
                  fincore::utc_timestamp(2021, 1, 1),
                  fincore::utc_timestamp(2021, 3, 1),
                  fincore::utc_timestamp(2021, 4, 1),
                  fincore::utc_timestamp(2021, 12, 1),
                  fincore::utc_timestamp(2022, 1, 1)}));
    expect_aggregation_labels(actual, {2020, 2021, 2021, 2021, 2021, 2022},
                              {12, 1, 3, 4, 12, 1});
    expect_values(actual.get_column<double>("strategy"),
                  {.122, -.073, .1, .2, .01, .02});
}

TEST(Returns, AggregatesQuarterlyReturnsWithCanonicalUtcBucketStarts) {
    const auto actual = fincore::aggregate_returns(
        aggregation_fixture(), fincore::return_aggregation_period::quarterly,
        "strategy");

    expect_aggregation_schema(actual, "strategy");
    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{
                  fincore::utc_timestamp(2020, 10, 1),
                  fincore::utc_timestamp(2021, 1, 1),
                  fincore::utc_timestamp(2021, 4, 1),
                  fincore::utc_timestamp(2021, 10, 1),
                  fincore::utc_timestamp(2022, 1, 1)}));
    expect_aggregation_labels(actual, {2020, 2021, 2021, 2021, 2022},
                              {4, 1, 2, 4, 1});
    expect_values(actual.get_column<double>("strategy"),
                  {.122, .0197, .2, .01, .02});
}

TEST(Returns, AggregatesYearlyReturnsWithExplicitPeriodOne) {
    const auto actual = fincore::aggregate_returns(
        aggregation_fixture(), fincore::return_aggregation_period::yearly,
        "strategy");

    expect_aggregation_schema(actual, "strategy");
    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2020),
                                                fincore::utc_timestamp(2021),
                                                fincore::utc_timestamp(2022)}));
    expect_aggregation_labels(actual, {2020, 2021, 2022}, {1, 1, 1});
    expect_values(actual.get_column<double>("strategy"),
                  {.122, .2358764, .02});
}

TEST(Returns, AggregationPermitsUnorderedIntradayAndDuplicateObservations) {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2021, 3, 5, 12),
         fincore::utc_timestamp(2020, 12, 31, 9),
         fincore::utc_timestamp(2021, 3, 1, 8),
         fincore::utc_timestamp(2021, 3, 1, 8)},
        "strategy", {.1, .02, .2, -.1});

    const auto actual = fincore::aggregate_returns(
        returns, fincore::return_aggregation_period::monthly, "strategy");

    expect_aggregation_schema(actual, "strategy");
    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 12, 1),
                                                fincore::utc_timestamp(2021, 3, 1)}));
    expect_aggregation_labels(actual, {2020, 2021}, {12, 3});
    expect_values(actual.get_column<double>("strategy"), {.02, .188});
}

TEST(Returns, AggregationPreservesInfinityAndProducesTypedEmptySchema) {
    const auto infinite_returns = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 3)},
        "strategy", {.1, infinity});
    const auto infinite_result = fincore::aggregate_returns(
        infinite_returns, fincore::return_aggregation_period::monthly,
        "strategy");
    EXPECT_EQ(infinite_result.get_column<double>("strategy").size(), 1U);
    EXPECT_EQ(infinite_result.get_column<double>("strategy").front(), infinity);

    const auto empty = fincore::make_series({}, "strategy", {});
    const auto empty_result = fincore::aggregate_returns(
        empty, fincore::return_aggregation_period::monthly, "strategy");
    EXPECT_TRUE(empty_result.get_index().empty());
    expect_aggregation_schema(empty_result, "strategy");
    EXPECT_TRUE(empty_result.get_column<int>(
                    fincore::return_aggregation_year_column.data())
                    .empty());
    EXPECT_TRUE(empty_result.get_column<int>(
                    fincore::return_aggregation_period_column.data())
                    .empty());
    EXPECT_TRUE(empty_result.get_column<double>("strategy").empty());
}

TEST(Returns, NewTransformationsValidateColumnsReservedNamesAndPeriod) {
    fincore::dataframe missing;
    missing.load_index(std::vector<fincore::timestamp>{fincore::utc_timestamp(2021)});

    fincore::dataframe wrong_type;
    wrong_type.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2021)});
    wrong_type.load_column<int>("strategy", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{fincore::utc_timestamp(2021),
                                                          fincore::utc_timestamp(2021, 1, 2)});
    malformed.load_column<double>("strategy", std::vector<double>{.01},
                                  hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(fincore::cumulative_returns(missing, 1.0, "strategy"),
                 fincore::input_error);
    EXPECT_THROW(fincore::cumulative_return(wrong_type, 1.0, "strategy"),
                 fincore::input_error);
    EXPECT_THROW(fincore::normalize(wrong_type, 1.0, "strategy"),
                 fincore::input_error);
    EXPECT_THROW(fincore::aggregate_returns(
                     malformed, fincore::return_aggregation_period::monthly,
                     "strategy"),
                 fincore::input_error);

    const auto valid =
        fincore::make_series({fincore::utc_timestamp(2021)}, "strategy", {.01});
    EXPECT_THROW(fincore::aggregate_returns(
                     valid, fincore::return_aggregation_period::monthly, ""),
                 fincore::input_error);

    for (const std::string_view reserved : {
             fincore::return_aggregation_year_column,
             fincore::return_aggregation_period_column}) {
        const auto reserved_input = fincore::make_series(
            {fincore::utc_timestamp(2021)}, reserved, {.01});
        EXPECT_THROW(fincore::aggregate_returns(
                         reserved_input,
                         fincore::return_aggregation_period::monthly, reserved),
                     fincore::input_error);
    }

    EXPECT_THROW(fincore::aggregate_returns(
                     valid,
                     static_cast<fincore::return_aggregation_period>(999),
                     "strategy"),
                 fincore::input_error);
}

}  // namespace
