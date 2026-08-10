#include <fincore/calendar.hpp>
#include <fincore/error.hpp>
#include <fincore/metrics/alpha_beta.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <utility>
#include <vector>

namespace {

constexpr double tolerance = 1.0e-10;

struct fixture_data {
    std::vector<fincore::timestamp> index{
        fincore::utc_timestamp(2019, 12, 27),
        fincore::utc_timestamp(2019, 12, 30),
        fincore::utc_timestamp(2019, 12, 31),
        fincore::utc_timestamp(2020, 1, 2),
        fincore::utc_timestamp(2020, 1, 3),
        fincore::utc_timestamp(2020, 1, 6),
        fincore::utc_timestamp(2020, 1, 7),
        fincore::utc_timestamp(2021, 1, 4),
        fincore::utc_timestamp(2021, 1, 5),
        fincore::utc_timestamp(2021, 1, 6),
    };
    std::vector<double> returns{.01, -.02, .015, .03, .01,
                                -.01, .02, .02,  -.01, .03};
    std::vector<double> benchmark{.005, -.01, .0,   .02,  .015,
                                  -.005, .01, .01, -.02, .015};
};

std::vector<fincore::timestamp> expected_year_index() {
    return {fincore::utc_timestamp(2019), fincore::utc_timestamp(2020),
            fincore::utc_timestamp(2021)};
}

void expect_result_frame(const fincore::dataframe& actual,
                         std::string_view column,
                         const std::vector<double>& expected) {
    EXPECT_EQ(actual.get_index(), expected_year_index());
    ASSERT_TRUE(actual.has_column(std::string{column}.c_str()));
    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(columns.front()).c_str(),
                 std::string{column}.c_str());
    EXPECT_EQ(std::get<2>(columns.front()), std::type_index(typeid(double)));

    const auto& values = actual.get_column<double>(std::string{column}.c_str());
    ASSERT_EQ(values.size(), expected.size());
    for (std::size_t position = 0; position < expected.size(); ++position) {
        EXPECT_NEAR(values[position], expected[position], tolerance);
    }
}

void expect_empty_result_frame(const fincore::dataframe& actual,
                               std::string_view column) {
    EXPECT_TRUE(actual.get_index().empty());
    ASSERT_TRUE(actual.has_column(std::string{column}.c_str()));
    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(columns.front()).c_str(),
                 std::string{column}.c_str());
    EXPECT_EQ(std::get<2>(columns.front()), std::type_index(typeid(double)));
    EXPECT_TRUE(actual.get_column<double>(std::string{column}.c_str()).empty());
}

fincore::dataframe wrong_type_frame(const std::vector<fincore::timestamp>& index,
                                    std::string_view column) {
    fincore::dataframe frame;
    auto copied_index = index;
    frame.load_index(std::move(copied_index));
    const std::string column_name{column};
    frame.load_column<int>(column_name.c_str(), std::vector<int>(index.size(), 1),
                           hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(AnnualAlphaBeta, MatchesDailyPythonGoldensAndUsesFixedSchemas) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "benchmark", fixture.benchmark);

    expect_result_frame(fincore::annual_alpha(returns, benchmark), "alpha",
                        {2.8435514326690203, -0.36262829207500147,
                         16.89211410563986});
    expect_result_frame(fincore::annual_beta(returns, benchmark), "beta",
                        {2.2142857142857144, 1.4285714285714284,
                         1.0930232558139534});
}

TEST(AnnualAlphaBeta, SupportsRiskFreeCustomAndWeeklyAnnualization) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "benchmark", fixture.benchmark);
    const std::vector<double> expected_beta{2.2142857142857144,
                                            1.4285714285714284,
                                            1.0930232558139534};

    expect_result_frame(fincore::annual_alpha(returns, benchmark, .001), "alpha",
                        {4.210014181094135, -0.28981677695498087,
                         17.311587845177847});
    expect_result_frame(fincore::annual_beta(returns, benchmark, .001), "beta",
                        expected_beta);
    expect_result_frame(
        fincore::annual_alpha(returns, benchmark, 0.0,
                              fincore::frequency::daily, 100.0),
        "alpha", {.7062268994055438, -.16366920587598144,
                  2.141144822402827});
    expect_result_frame(
        fincore::annual_beta(returns, benchmark, 0.0,
                             fincore::frequency::daily, 100.0),
        "beta", expected_beta);
    expect_result_frame(
        fincore::annual_alpha(returns, benchmark, 0.0,
                              fincore::frequency::weekly),
        "alpha", {.32025897342761245, -.0887519630069038,
                  .8133671298825977});
    expect_result_frame(
        fincore::annual_beta(returns, benchmark, 0.0,
                             fincore::frequency::weekly),
        "beta", expected_beta);
}

TEST(AnnualAlphaBeta, AlignsCommonTimestampsBeforeYearGrouping) {
    const fixture_data fixture;
    auto benchmark_index = fixture.index;
    auto benchmark_values = fixture.benchmark;
    benchmark_index.erase(benchmark_index.begin() + 4);
    benchmark_values.erase(benchmark_values.begin() + 4);
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark = fincore::make_series(
        std::move(benchmark_index), "benchmark", std::move(benchmark_values));

    const auto [aligned_returns, aligned_benchmark] =
        fincore::align_on_common_index(returns, "returns", benchmark, "benchmark");
    auto expected_common_index = fixture.index;
    expected_common_index.erase(expected_common_index.begin() + 4);
    EXPECT_EQ(aligned_returns.get_index(), expected_common_index);
    EXPECT_EQ(aligned_benchmark.get_index(), expected_common_index);

    expect_result_frame(fincore::annual_alpha(returns, benchmark), "alpha",
                        {2.8435514326690203, -.06417287658240456,
                         16.89211410563986});
    expect_result_frame(fincore::annual_beta(returns, benchmark), "beta",
                        {2.2142857142857144, 1.6315789473684212,
                         1.0930232558139534});
}

TEST(AnnualAlphaBeta, IsolatesNanAndInfinityPairsToTheirCalendarYear) {
    const fixture_data fixture;
    auto return_values = fixture.returns;
    auto benchmark_values = fixture.benchmark;
    return_values[4] = std::numeric_limits<double>::quiet_NaN();
    benchmark_values[5] = std::numeric_limits<double>::infinity();
    const auto returns = fincore::make_series(fixture.index, "returns",
                                               std::move(return_values));
    const auto benchmark = fincore::make_series(
        fixture.index, "benchmark", std::move(benchmark_values));

    const auto alpha_result = fincore::annual_alpha(returns, benchmark);
    const auto beta_result = fincore::annual_beta(returns, benchmark);
    EXPECT_EQ(alpha_result.get_index(), expected_year_index());
    EXPECT_EQ(beta_result.get_index(), expected_year_index());
    const auto& alpha_values = alpha_result.get_column<double>("alpha");
    const auto& beta_values = beta_result.get_column<double>("beta");
    ASSERT_EQ(alpha_values.size(), 3U);
    ASSERT_EQ(beta_values.size(), 3U);
    EXPECT_NEAR(alpha_values[0], 2.8435514326690203, tolerance);
    EXPECT_TRUE(std::isnan(alpha_values[1]));
    EXPECT_NEAR(alpha_values[2], 16.89211410563986, tolerance);
    EXPECT_NEAR(beta_values[0], 2.2142857142857144, tolerance);
    EXPECT_TRUE(std::isnan(beta_values[1]));
    EXPECT_NEAR(beta_values[2], 1.0930232558139534, tolerance);
}

TEST(AnnualAlphaBeta, IsolatesFlatBenchmarkToItsCalendarYear) {
    const fixture_data fixture;
    auto benchmark_values = fixture.benchmark;
    for (std::size_t position = 3; position < 7; ++position) {
        benchmark_values[position] = .01;
    }
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark = fincore::make_series(
        fixture.index, "benchmark", std::move(benchmark_values));

    const auto alpha_result = fincore::annual_alpha(returns, benchmark);
    const auto beta_result = fincore::annual_beta(returns, benchmark);
    const auto& alpha_values = alpha_result.get_column<double>("alpha");
    const auto& beta_values = beta_result.get_column<double>("beta");
    ASSERT_EQ(alpha_values.size(), 3U);
    ASSERT_EQ(beta_values.size(), 3U);
    EXPECT_NEAR(alpha_values[0], 2.8435514326690203, tolerance);
    EXPECT_TRUE(std::isnan(alpha_values[1]));
    EXPECT_NEAR(alpha_values[2], 16.89211410563986, tolerance);
    EXPECT_NEAR(beta_values[0], 2.2142857142857144, tolerance);
    EXPECT_TRUE(std::isnan(beta_values[1]));
    EXPECT_NEAR(beta_values[2], 1.0930232558139534, tolerance);
}

TEST(AnnualAlphaBeta, ReturnsTypedEmptyFramesForEmptyAlignment) {
    const auto returns = fincore::make_series({1}, "returns", {.01});
    const auto benchmark = fincore::make_series({2}, "benchmark", {.02});
    expect_empty_result_frame(fincore::annual_alpha(returns, benchmark), "alpha");
    expect_empty_result_frame(fincore::annual_beta(returns, benchmark), "beta");

    const auto empty_returns = fincore::make_series({}, "returns", {});
    const auto empty_benchmark = fincore::make_series({}, "benchmark", {});
    expect_empty_result_frame(fincore::annual_alpha(empty_returns, empty_benchmark),
                              "alpha");
    expect_empty_result_frame(fincore::annual_beta(empty_returns, empty_benchmark),
                              "beta");
}

TEST(AnnualAlphaBeta, SupportsNamedColumnsAndRejectsStructuralErrors) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "strategy", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "market", fixture.benchmark);

    expect_result_frame(
        fincore::annual_alpha(returns, benchmark, 0.0,
                              fincore::frequency::daily, std::nullopt,
                              "strategy", "market"),
        "alpha", {2.8435514326690203, -0.36262829207500147,
                  16.89211410563986});
    expect_result_frame(
        fincore::annual_beta(returns, benchmark, 0.0,
                             fincore::frequency::daily, std::nullopt,
                             "strategy", "market"),
        "beta", {2.2142857142857144, 1.4285714285714284,
                 1.0930232558139534});

    const auto wrong_strategy = wrong_type_frame(fixture.index, "strategy");
    const auto wrong_market = wrong_type_frame(fixture.index, "market");
    EXPECT_THROW(
        fincore::annual_alpha(returns, benchmark, 0.0,
                              fincore::frequency::daily, std::nullopt,
                              "missing", "market"),
        fincore::input_error);
    EXPECT_THROW(
        fincore::annual_beta(returns, benchmark, 0.0,
                             fincore::frequency::daily, std::nullopt,
                             "strategy", ""),
        fincore::input_error);
    EXPECT_THROW(
        fincore::annual_alpha(wrong_strategy, benchmark, 0.0,
                              fincore::frequency::daily, std::nullopt,
                              "strategy", "market"),
        fincore::input_error);
    EXPECT_THROW(
        fincore::annual_beta(returns, wrong_market, 0.0,
                             fincore::frequency::daily, std::nullopt,
                             "strategy", "market"),
        fincore::input_error);
}

TEST(AnnualAlphaBeta, KeepsBetaPeriodAndAnnualizationCompatibilityOnly) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "benchmark", fixture.benchmark);
    constexpr auto unknown = static_cast<fincore::frequency>(999);
    const std::vector<double> expected_beta{2.2142857142857144,
                                            1.4285714285714284,
                                            1.0930232558139534};

    expect_result_frame(
        fincore::annual_beta(returns, benchmark, 0.0, unknown,
                             std::numeric_limits<double>::quiet_NaN()),
        "beta", expected_beta);
    expect_result_frame(
        fincore::annual_beta(returns, benchmark, 0.0, unknown,
                             std::numeric_limits<double>::infinity()),
        "beta", expected_beta);
    EXPECT_THROW(fincore::annual_alpha(returns, benchmark, 0.0, unknown),
                 fincore::input_error);
    expect_result_frame(
        fincore::annual_alpha(returns, benchmark, 0.0, unknown, 100.0),
        "alpha", {.7062268994055438, -.16366920587598144,
                  2.141144822402827});
}

}  // namespace
