#include <fincore/error.hpp>
#include <fincore/calendar.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/yearly.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
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
    EXPECT_TRUE(
        actual.get_column<double>(std::string{column}.c_str()).empty());
}

TEST(Yearly, MatchesDailyPythonGoldensAndUsesFixedOutputSchema) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "benchmark", fixture.benchmark);

    expect_result_frame(
        fincore::annual_return_by_year(returns), "annual_return",
        {.4761595646542174, 21.275106307663346, 26.170501183432872});
    expect_result_frame(
        fincore::annual_volatility_by_year(returns), "annual_volatility",
        {.3004995840263344, .2711088342345192, .33045423283716613});
    expect_result_frame(
        fincore::sharpe_ratio_by_year(returns), "sharpe_ratio",
        {1.3976724838434154, 11.618950038622254, 10.167822548835883});
    expect_result_frame(
        fincore::max_drawdown_by_year(returns), "max_drawdown",
        {-.019999999999999962, -.01000000000000002,
         -.00999999999999996});
    EXPECT_NEAR(fincore::annual_active_return(returns, benchmark),
                7.788034474258492, tolerance);
    expect_result_frame(
        fincore::annual_active_return_by_year(returns, benchmark),
        "annual_active_return",
        {.8225693616072098, 10.13361263157495, 25.694341618778655});
    expect_result_frame(
        fincore::information_ratio_by_year(returns, benchmark),
        "information_ratio",
        {4.205259864302773, 4.582575694955839, 64.15605972938177});
}

TEST(Yearly, SortsYearBucketsButPreservesInputOrderWithinEachYear) {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2021, 1, 6),
         fincore::utc_timestamp(2019, 12, 31),
         fincore::utc_timestamp(2021, 1, 4),
         fincore::utc_timestamp(2020, 6, 1),
         fincore::utc_timestamp(2021, 1, 5)},
        "returns", {-.10, .02, .10, -.05, -.10});

    expect_result_frame(fincore::max_drawdown_by_year(returns), "max_drawdown",
                        {0.0, -.05, -.109});
}

TEST(Yearly, ReturnsEmptyCorrectlyTypedFramesForEmptyInputs) {
    const auto returns = fincore::make_series({}, "returns", {});
    const auto benchmark = fincore::make_series({}, "benchmark", {});

    expect_empty_result_frame(fincore::annual_return_by_year(returns),
                              "annual_return");
    expect_empty_result_frame(fincore::annual_volatility_by_year(returns),
                              "annual_volatility");
    expect_empty_result_frame(fincore::sharpe_ratio_by_year(returns),
                              "sharpe_ratio");
    expect_empty_result_frame(fincore::max_drawdown_by_year(returns),
                              "max_drawdown");
    EXPECT_TRUE(std::isnan(fincore::annual_active_return(returns, benchmark)));
    expect_empty_result_frame(
        fincore::annual_active_return_by_year(returns, benchmark),
        "annual_active_return");
    expect_empty_result_frame(
        fincore::information_ratio_by_year(returns, benchmark),
        "information_ratio");
}

TEST(Yearly, SupportsExplicitAndFrequencyBasedAnnualization) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "benchmark", fixture.benchmark);

    expect_result_frame(
        fincore::annual_return_by_year(returns, fincore::frequency::daily,
                                       100.0),
        "annual_return",
        {.1671223636520296, 2.426487258210055, 2.7075444696683486});
    EXPECT_NEAR(fincore::annual_active_return(
                    returns, benchmark, fincore::frequency::daily, 100.0),
                1.059831325786902, tolerance);
    expect_result_frame(
        fincore::annual_active_return_by_year(
            returns, benchmark, fincore::frequency::daily, 100.0),
        "annual_active_return",
        {.3224105945057777, .7332989929958926, 2.540422106016319});
    expect_result_frame(
        fincore::information_ratio_by_year(
            returns, benchmark, fincore::frequency::daily, 100.0),
        "information_ratio",
        {2.649064714130087, 2.8867513459481287, 40.41451884327382});
    expect_result_frame(
        fincore::annual_volatility_by_year(returns,
                                           fincore::frequency::weekly),
        "annual_volatility",
        {.13650396819628846, .12315302134607442, .15011106998930268});
    expect_result_frame(
        fincore::sharpe_ratio_by_year(returns, 0.0,
                                      fincore::frequency::weekly),
        "sharpe_ratio",
        {.6349021776571554, 5.277986629117477, 4.618802153517007});
}

TEST(Yearly, AlignsPairedInputsBeforeCreatingYearBuckets) {
    const fixture_data fixture;
    auto benchmark_index = fixture.index;
    auto benchmark_values = fixture.benchmark;
    benchmark_index.erase(benchmark_index.begin() + 4);
    benchmark_values.erase(benchmark_values.begin() + 4);
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark = fincore::make_series(
        std::move(benchmark_index), "benchmark", std::move(benchmark_values));

    expect_result_frame(
        fincore::annual_active_return_by_year(returns, benchmark),
        "annual_active_return",
        {.8225693616072098, 19.18046753276349, 25.694341618778655});
    expect_result_frame(
        fincore::information_ratio_by_year(returns, benchmark),
        "information_ratio",
        {4.205259864302773, 9.16515138991168, 64.15605972938177});
}

TEST(Yearly, IsolatesNonFinitePairsToTheirCalendarYear) {
    const fixture_data fixture;
    auto return_values = fixture.returns;
    auto benchmark_values = fixture.benchmark;
    return_values[4] = std::numeric_limits<double>::quiet_NaN();
    benchmark_values[5] = std::numeric_limits<double>::infinity();
    const auto returns = fincore::make_series(fixture.index, "returns",
                                               std::move(return_values));
    const auto benchmark = fincore::make_series(
        fixture.index, "benchmark", std::move(benchmark_values));

    const auto result = fincore::information_ratio_by_year(returns, benchmark);
    EXPECT_EQ(result.get_index(), expected_year_index());
    const auto& values = result.get_column<double>("information_ratio");
    ASSERT_EQ(values.size(), 3U);
    EXPECT_NEAR(values[0], 4.205259864302773, tolerance);
    const auto returns_2020 = fincore::make_series(
        {fixture.index[3], fixture.index[4], fixture.index[5], fixture.index[6]},
        "returns", {.03, std::numeric_limits<double>::quiet_NaN(), -.01, .02});
    const auto benchmark_2020 = fincore::make_series(
        {fixture.index[3], fixture.index[4], fixture.index[5], fixture.index[6]},
        "benchmark", {.02, .015, std::numeric_limits<double>::infinity(), .01});
    EXPECT_DOUBLE_EQ(values[1],
                     fincore::information_ratio(returns_2020, benchmark_2020));
    EXPECT_DOUBLE_EQ(values[1], 9.151030747104376e16);
    EXPECT_NEAR(values[2], 64.15605972938177, tolerance);
}

TEST(Yearly, SupportsNamedColumnsAcrossUnaryAndPairedApis) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "strategy", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "market", fixture.benchmark);

    expect_result_frame(
        fincore::annual_return_by_year(returns, fincore::frequency::daily,
                                       std::nullopt, "strategy"),
        "annual_return",
        {.4761595646542174, 21.275106307663346, 26.170501183432872});
    expect_result_frame(
        fincore::annual_volatility_by_year(returns, fincore::frequency::daily,
                                           std::nullopt, "strategy"),
        "annual_volatility",
        {.3004995840263344, .2711088342345192, .33045423283716613});
    expect_result_frame(
        fincore::sharpe_ratio_by_year(returns, 0.0,
                                      fincore::frequency::daily, std::nullopt,
                                      "strategy"),
        "sharpe_ratio",
        {1.3976724838434154, 11.618950038622254, 10.167822548835883});
    expect_result_frame(fincore::max_drawdown_by_year(returns, "strategy"),
                        "max_drawdown",
                        {-.019999999999999962, -.01000000000000002,
                         -.00999999999999996});
    EXPECT_NEAR(fincore::annual_active_return(
                    returns, benchmark, fincore::frequency::daily,
                    std::nullopt, "strategy", "market"),
                7.788034474258492, tolerance);
    expect_result_frame(
        fincore::annual_active_return_by_year(
            returns, benchmark, fincore::frequency::daily, std::nullopt,
            "strategy", "market"),
        "annual_active_return",
        {.8225693616072098, 10.13361263157495, 25.694341618778655});
    expect_result_frame(
        fincore::information_ratio_by_year(
            returns, benchmark, fincore::frequency::daily, std::nullopt,
            "strategy", "market"),
        "information_ratio",
        {4.205259864302773, 4.582575694955839, 64.15605972938177});
}

TEST(Yearly, RejectsMissingEmptyAndWronglyTypedColumns) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "benchmark", fixture.benchmark);
    fincore::dataframe wrong_type;
    auto wrong_type_index = fixture.index;
    wrong_type.load_index(std::move(wrong_type_index));
    wrong_type.load_column<int>("returns", std::vector<int>(fixture.index.size(), 1),
                                hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(fincore::annual_return_by_year(returns,
                                                fincore::frequency::daily,
                                                std::nullopt, "missing"),
                 fincore::input_error);
    EXPECT_THROW(fincore::annual_volatility_by_year(wrong_type),
                 fincore::input_error);
    EXPECT_THROW(fincore::sharpe_ratio_by_year(returns, 0.0,
                                               fincore::frequency::daily,
                                               std::nullopt, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::max_drawdown_by_year(wrong_type),
                 fincore::input_error);
    EXPECT_THROW(fincore::annual_active_return(
                     returns, benchmark, fincore::frequency::daily,
                     std::nullopt, "missing", "benchmark"),
                 fincore::input_error);
    EXPECT_THROW(fincore::annual_active_return_by_year(
                     returns, benchmark, fincore::frequency::daily,
                     std::nullopt, "returns", "missing"),
                 fincore::input_error);
    EXPECT_THROW(fincore::information_ratio_by_year(
                     wrong_type, benchmark, fincore::frequency::daily,
                     std::nullopt, "returns", "benchmark"),
                 fincore::input_error);
}

TEST(Yearly, RejectsUnknownFrequencyButPassesExplicitAnnualizationThrough) {
    const fixture_data fixture;
    const auto returns =
        fincore::make_series(fixture.index, "returns", fixture.returns);
    const auto benchmark =
        fincore::make_series(fixture.index, "benchmark", fixture.benchmark);
    constexpr auto unknown = static_cast<fincore::frequency>(999);

    EXPECT_THROW(fincore::annual_return_by_year(returns, unknown),
                 fincore::input_error);
    EXPECT_THROW(fincore::annual_active_return(returns, benchmark, unknown),
                 fincore::input_error);

    const auto zero =
        fincore::annual_return_by_year(returns, unknown, 0.0);
    for (const double value : zero.get_column<double>("annual_return")) {
        EXPECT_DOUBLE_EQ(value, 0.0);
    }
    for (const double annualization :
         {-1.0, std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity()}) {
        const auto result = fincore::information_ratio_by_year(
            returns, benchmark, unknown, annualization);
        for (const double value :
             result.get_column<double>("information_ratio")) {
            EXPECT_TRUE(std::isnan(value));
        }
    }
}

}  // namespace
