#include <fincore/error.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/perf_stats.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/stats.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using statistics = std::map<std::string, double>;

const std::set<std::string>& unary_keys() {
    static const std::set<std::string> keys{
        "Annual return",       "Cumulative returns", "Annual volatility",
        "Sharpe ratio",        "Calmar ratio",       "Stability",
        "Max drawdown",        "Omega ratio",        "Sortino ratio",
        "Skew",                "Kurtosis",           "Tail ratio",
        "Daily value at risk"};
    return keys;
}

std::set<std::string> keys_of(const statistics& values) {
    std::set<std::string> keys;
    for (const auto& [key, value] : values) {
        static_cast<void>(value);
        keys.insert(key);
    }
    return keys;
}

statistics expected_unary_statistics(const fincore::dataframe& returns,
                                     const fincore::frequency period,
                                     const std::string_view column) {
    const double annualization = fincore::annualization_factor(period);
    return {
        {"Annual return", fincore::annual_return(returns, annualization, column)},
        {"Cumulative returns", fincore::cumulative_return(returns, column)},
        {"Annual volatility",
         fincore::annual_volatility(returns, annualization, column)},
        {"Sharpe ratio", fincore::sharpe_ratio(returns, 0.0, annualization, column)},
        {"Calmar ratio", fincore::calmar_ratio(returns, 0.0, annualization, column)},
        {"Stability", fincore::stability_of_timeseries(returns, column)},
        {"Max drawdown", fincore::max_drawdown(returns, column)},
        {"Omega ratio",
         fincore::omega_ratio(returns, 0.0, 0.0, fincore::daily_annualization,
                               column)},
        {"Sortino ratio",
         fincore::sortino_ratio(returns, 0.0, annualization, column)},
        {"Skew", fincore::skewness(returns, column)},
        {"Kurtosis", fincore::kurtosis(returns, column)},
        {"Tail ratio", fincore::tail_ratio(returns, column)},
        {"Daily value at risk",
         fincore::historical_value_at_risk(returns, 0.05, column)},
    };
}

void expect_same_value(const double actual, const double expected) {
    if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(actual));
    } else if (std::isinf(expected)) {
        EXPECT_EQ(actual, expected);
    } else {
        EXPECT_DOUBLE_EQ(actual, expected);
    }
}

void expect_statistics_equal(const statistics& actual,
                             const statistics& expected) {
    ASSERT_EQ(keys_of(actual), keys_of(expected));
    for (const auto& [key, expected_value] : expected) {
        const auto actual_value = actual.find(key);
        ASSERT_NE(actual_value, actual.end());
        expect_same_value(actual_value->second, expected_value);
    }
}

fincore::dataframe wrong_type_frame(
    const std::vector<fincore::timestamp>& index,
    const std::string_view column) {
    fincore::dataframe frame;
    auto copied_index = index;
    frame.load_index(std::move(copied_index));
    const std::string name{column};
    frame.load_column<int>(name.c_str(), std::vector<int>(index.size(), 1),
                           hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe malformed_frame(const std::string_view column) {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{1, 2});
    const std::string name{column};
    frame.load_column<double>(name.c_str(), std::vector<double>{0.01},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(PerfStats, MatchesFiniteDailyPythonOracleAndExactKeySets) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8}, "returns",
        {.01, -.02, .015, .0, .03, -.01, .005, .02});
    const auto benchmark = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8}, "benchmark",
        {.004, -.01, .012, -.003, .02, -.008, .004, .014});

    const auto unary = fincore::perf_stats(returns);
    const auto paired = fincore::perf_stats(returns, benchmark);

    EXPECT_EQ(keys_of(unary), unary_keys());
    auto expected_paired_keys = unary_keys();
    expected_paired_keys.insert("Alpha");
    expected_paired_keys.insert("Beta");
    EXPECT_EQ(keys_of(paired), expected_paired_keys);
    ASSERT_EQ(unary.size(), 13U);
    ASSERT_EQ(paired.size(), 15U);

    const statistics expected{
        {"Annual return", 3.6713515622970077},
        {"Cumulative returns", .05015195340208978},
        {"Annual volatility", .2571964229922337},
        {"Sharpe ratio", 6.123724356957944},
        {"Calmar ratio", 183.56757811485073},
        {"Stability", .7123343398383044},
        {"Max drawdown", -.019999999999999962},
        {"Omega ratio", 2.666666666666667},
        {"Sortino ratio", 12.549900398011134},
        {"Skew", -.2120878539880257},
        {"Kurtosis", -.8532555879494659},
        {"Tail ratio", 1.6060606060606057},
        {"Daily value at risk", -.0165},
        {"Alpha", .0417341829666007},
        {"Beta", 1.4758151753979294},
    };
    expect_statistics_equal(paired, expected);
    for (const auto& [key, expected_value] : expected) {
        if (key == "Alpha" || key == "Beta") {
            continue;
        }
        const auto actual = unary.find(key);
        ASSERT_NE(actual, unary.end());
        expect_same_value(actual->second, expected_value);
    }
}

TEST(PerfStats, AppliesWeeklyFactorOnlyToPythonPeriodSensitiveMetrics) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8}, "returns",
        {.01, -.02, .015, .0, .03, -.01, .005, .02});
    const auto benchmark = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8}, "benchmark",
        {.004, -.01, .012, -.003, .02, -.008, .004, .014});

    const auto daily = fincore::perf_stats(returns, benchmark);
    const auto weekly =
        fincore::perf_stats(returns, benchmark, fincore::frequency::weekly);

    EXPECT_NEAR(weekly.at("Annual return"), .3744816290191584, 1.0e-12);
    EXPECT_NEAR(weekly.at("Annual volatility"), .11683321445547923, 1.0e-12);
    EXPECT_NEAR(weekly.at("Sharpe ratio"), 2.7817432013209333, 1.0e-12);
    EXPECT_NEAR(weekly.at("Calmar ratio"), 18.724081450957954, 1.0e-12);
    EXPECT_NEAR(weekly.at("Sortino ratio"), 5.70087712549569, 1.0e-12);
    EXPECT_DOUBLE_EQ(weekly.at("Alpha"), daily.at("Alpha"));
    EXPECT_DOUBLE_EQ(weekly.at("Beta"), daily.at("Beta"));
    EXPECT_DOUBLE_EQ(weekly.at("Omega ratio"), daily.at("Omega ratio"));
}

TEST(PerfStats, DelegatesMismatchedBenchmarkAlignmentAndNonFiniteInputs) {
    const auto returns = fincore::make_series(
        {4, 1, 3, 2, 5}, "strategy",
        {.01, std::numeric_limits<double>::quiet_NaN(), -.02,
         std::numeric_limits<double>::infinity(), .03});
    const auto benchmark = fincore::make_series(
        {2, 3, 4, 6, 5}, "market", {.01, -.01, .005, .02, -.015});

    const auto actual = fincore::perf_stats(returns, benchmark,
                                             fincore::frequency::weekly,
                                             "strategy", "market");
    auto expected = expected_unary_statistics(
        returns, fincore::frequency::weekly, "strategy");
    const auto [expected_alpha, expected_beta] = fincore::alpha_beta(
        returns, benchmark, 0.0, fincore::daily_annualization, "strategy",
        "market");
    expected.emplace("Alpha", expected_alpha);
    expected.emplace("Beta", expected_beta);

    expect_statistics_equal(actual, expected);
}

TEST(PerfStats, SupportsCustomColumnsAndEmptyGenuineDoubleColumns) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "strategy", {.01, -.02, .03});
    const auto benchmark = fincore::make_series(
        {1, 2, 3}, "market", {.005, -.01, .015});
    const auto named = fincore::perf_stats(returns, benchmark,
                                            fincore::frequency::daily,
                                            "strategy", "market");
    auto expected_named = expected_unary_statistics(
        returns, fincore::frequency::daily, "strategy");
    const auto [expected_alpha, expected_beta] = fincore::alpha_beta(
        returns, benchmark, 0.0, fincore::daily_annualization, "strategy",
        "market");
    expected_named.emplace("Alpha", expected_alpha);
    expected_named.emplace("Beta", expected_beta);
    expect_statistics_equal(named, expected_named);

    const auto empty = fincore::make_series({}, "strategy", {});
    const auto empty_statistics =
        fincore::perf_stats(empty, fincore::frequency::daily, "strategy");
    EXPECT_EQ(keys_of(empty_statistics), unary_keys());
    EXPECT_EQ(empty_statistics.size(), 13U);
    expect_statistics_equal(
        empty_statistics,
        expected_unary_statistics(empty, fincore::frequency::daily, "strategy"));

    const auto empty_benchmark = fincore::make_series({}, "market", {});
    const auto empty_paired_statistics = fincore::perf_stats(
        empty, empty_benchmark, fincore::frequency::daily, "strategy", "market");
    auto expected_empty_paired = expected_unary_statistics(
        empty, fincore::frequency::daily, "strategy");
    const auto [empty_alpha, empty_beta] = fincore::alpha_beta(
        empty, empty_benchmark, 0.0, fincore::daily_annualization, "strategy",
        "market");
    expected_empty_paired.emplace("Alpha", empty_alpha);
    expected_empty_paired.emplace("Beta", empty_beta);
    EXPECT_EQ(empty_paired_statistics.size(), 15U);
    expect_statistics_equal(empty_paired_statistics, expected_empty_paired);
}

TEST(PerfStats, ValidatesColumnsBeforeResolvingFrequency) {
    const auto returns = fincore::make_series({1, 2}, "returns", {.01, -.02});
    const auto benchmark =
        fincore::make_series({1, 2}, "benchmark", {.005, -.01});
    const auto wrong_returns = wrong_type_frame({1, 2}, "returns");
    const auto wrong_benchmark = wrong_type_frame({1, 2}, "benchmark");
    const auto malformed_returns = malformed_frame("returns");
    constexpr auto unknown = static_cast<fincore::frequency>(999);

    EXPECT_THROW(fincore::perf_stats(returns, fincore::frequency::daily,
                                     "missing"),
                 fincore::input_error);
    EXPECT_THROW(fincore::perf_stats(returns, fincore::frequency::daily, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::perf_stats(wrong_returns), fincore::input_error);
    EXPECT_THROW(fincore::perf_stats(malformed_returns), fincore::input_error);
    EXPECT_THROW(fincore::perf_stats(returns, wrong_benchmark,
                                     fincore::frequency::daily, "returns",
                                     "benchmark"),
                 fincore::input_error);
    EXPECT_THROW(fincore::perf_stats(returns, benchmark,
                                     fincore::frequency::daily, "returns", ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::perf_stats(returns, unknown), fincore::input_error);
    try {
        static_cast<void>(fincore::perf_stats(wrong_returns, unknown));
        FAIL() << "expected the malformed return column to be rejected";
    } catch (const fincore::input_error& error) {
        EXPECT_STREQ(error.what(), "required column is not a double column");
    }
    try {
        static_cast<void>(fincore::perf_stats(
            returns, wrong_benchmark, unknown, "returns", "benchmark"));
        FAIL() << "expected the malformed benchmark column to be rejected";
    } catch (const fincore::input_error& error) {
        EXPECT_STREQ(error.what(), "required column is not a double column");
    }
}

}  // namespace
