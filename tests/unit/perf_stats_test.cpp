#include <fincore/error.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/perf_stats.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/stats.hpp>

#include "../fixtures/bootstrap_fixed_indices.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
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

void expect_matches_python_fixture(
    const double actual,
    const bootstrap_fixed_indices_fixture::expected_number& expected) {
    using bootstrap_fixed_indices_fixture::expected_class;
    switch (expected.value_class) {
    case expected_class::nan:
        EXPECT_TRUE(std::isnan(actual));
        return;
    case expected_class::positive_infinity:
        EXPECT_EQ(actual, std::numeric_limits<double>::infinity());
        return;
    case expected_class::negative_infinity:
        EXPECT_EQ(actual, -std::numeric_limits<double>::infinity());
        return;
    case expected_class::finite:
        ASSERT_TRUE(std::isfinite(actual));
        const double budget =
            bootstrap_fixed_indices_fixture::absolute_tolerance +
            bootstrap_fixed_indices_fixture::relative_tolerance *
                std::abs(expected.value);
        EXPECT_LE(std::abs(actual - expected.value), budget);
        return;
    }
    FAIL() << "unknown fixture value class";
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
    // Only the period-sensitive entries see annualization_factor(period): the
    // Omega row is built from the daily constant and alpha_beta is called with
    // it regardless of period, so equality here must be exact, not tolerated.
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

TEST(PerfStats, CalcDistributionStatsMatchesPythonOracle) {
    const std::vector<double> values{0.01, 0.02, -0.05, 0.03, 0.01,
                                     -0.02, 0.04, 0.01, 0.02, -0.03};
    const auto stats = fincore::calc_distribution_stats(values);

    EXPECT_NEAR(stats.mean, 0.003999999999999999, 1e-18);
    EXPECT_NEAR(stats.median, 0.01, 1e-18);
    EXPECT_NEAR(stats.std, 0.02690724809414742, 1e-16);
    EXPECT_NEAR(stats.p5, -0.041, 1e-18);
    EXPECT_NEAR(stats.p25, -0.0125, 1e-18);
    EXPECT_NEAR(stats.p75, 0.02, 1e-18);
    EXPECT_NEAR(stats.p95, 0.03549999999999999, 1e-16);
    EXPECT_NEAR(stats.iqr, 0.0325, 1e-18);
}

TEST(PerfStats, CalcDistributionStatsHandlesDegenerateSamples) {
    const std::vector<double> single{0.02};
    const auto one = fincore::calc_distribution_stats(single);
    EXPECT_DOUBLE_EQ(one.mean, 0.02);
    EXPECT_DOUBLE_EQ(one.median, 0.02);
    EXPECT_DOUBLE_EQ(one.std, 0.0);
    EXPECT_DOUBLE_EQ(one.p5, 0.02);
    EXPECT_DOUBLE_EQ(one.iqr, 0.0);

    const std::vector<double> empty;
    const auto none = fincore::calc_distribution_stats(empty);
    EXPECT_TRUE(std::isnan(none.mean));
    EXPECT_TRUE(std::isnan(none.median));
    EXPECT_TRUE(std::isnan(none.std));
    EXPECT_TRUE(std::isnan(none.iqr));
}

void expect_samples_equal(const std::vector<double>& left,
                          const std::vector<double>& right) {
    ASSERT_EQ(left.size(), right.size());
    for (std::size_t position = 0; position < left.size(); ++position) {
        if (std::isnan(left[position]) || std::isnan(right[position])) {
            EXPECT_TRUE(std::isnan(left[position]));
            EXPECT_TRUE(std::isnan(right[position]));
        } else {
            EXPECT_EQ(left[position], right[position]);
        }
    }
}

fincore::dataframe bootstrap_fixture(std::string_view column = "returns") {
    std::vector<fincore::timestamp> index(10);
    for (std::size_t position = 0; position < index.size(); ++position) {
        index[position] = static_cast<fincore::timestamp>(position + 1);
    }
    return fincore::make_series(
        index, column,
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
}

TEST(PerfStats, PerfStatsBootstrapIsDeterministicAndOrdered) {
    const auto returns = bootstrap_fixture();
    const auto first = fincore::perf_stats_bootstrap(returns, 8, 42);
    const auto second = fincore::perf_stats_bootstrap(returns, 8, 42);

    ASSERT_EQ(first.size(), 13U);
    const std::array<const char*, 13> expected_names{
        "Annual return",       "Cumulative returns", "Annual volatility",
        "Sharpe ratio",        "Calmar ratio",       "Stability",
        "Max drawdown",        "Omega ratio",        "Sortino ratio",
        "Skew",                "Kurtosis",           "Tail ratio",
        "Daily value at risk"};
    for (std::size_t metric = 0; metric < first.size(); ++metric) {
        EXPECT_EQ(first[metric].name, expected_names[metric]);
        ASSERT_EQ(first[metric].samples.size(), 8U);
        expect_samples_equal(first[metric].samples, second[metric].samples);
        const auto recomputed =
            fincore::calc_distribution_stats(first[metric].samples);
        if (std::isnan(recomputed.mean) || std::isnan(first[metric].summary.mean)) {
            EXPECT_TRUE(std::isnan(recomputed.mean));
            EXPECT_TRUE(std::isnan(first[metric].summary.mean));
        } else {
            EXPECT_EQ(recomputed.mean, first[metric].summary.mean);
        }
        if (std::isnan(recomputed.p95) || std::isnan(first[metric].summary.p95)) {
            EXPECT_TRUE(std::isnan(recomputed.p95));
            EXPECT_TRUE(std::isnan(first[metric].summary.p95));
        } else {
            EXPECT_EQ(recomputed.p95, first[metric].summary.p95);
        }
    }

    const auto different_seed = fincore::perf_stats_bootstrap(returns, 8, 43);
    bool any_difference = false;
    for (std::size_t metric = 0; metric < first.size(); ++metric) {
        if (first[metric].samples != different_seed[metric].samples) {
            any_difference = true;
        }
    }
    EXPECT_TRUE(any_difference);
}

TEST(PerfStats, PerfStatsBootstrapHandlesSingleSampleAndEmptyInput) {
    const auto returns = bootstrap_fixture();
    const auto single = fincore::perf_stats_bootstrap(returns, 1, 7);
    ASSERT_EQ(single.size(), 13U);
    for (const auto& metric : single) {
        EXPECT_EQ(metric.samples.size(), 1U);
        EXPECT_DOUBLE_EQ(metric.summary.std, 0.0);
    }

    const auto empty = fincore::make_series({}, "returns", {});
    const auto empty_result = fincore::perf_stats_bootstrap(empty, 4, 7);
    ASSERT_EQ(empty_result.size(), 13U);
    for (const auto& metric : empty_result) {
        EXPECT_TRUE(metric.samples.empty());
        EXPECT_TRUE(std::isnan(metric.summary.mean));
    }
}

TEST(PerfStats, PerfStatsBootstrapSupportsSelectorsAndRejectsNul) {
    const auto strategy = bootstrap_fixture("strategy");
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};

    EXPECT_EQ(
        fincore::perf_stats_bootstrap(strategy, 2, 1, bounded_selector).size(),
        13U);
    EXPECT_THROW(static_cast<void>(fincore::perf_stats_bootstrap(
                     strategy, 2, 1, embedded_selector)),
                 fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::perf_stats_bootstrap(strategy, 2, 1, "")),
        fincore::input_error);
}

TEST(PerfStats, PerfStatsBootstrapMatchesPinnedPythonFixedDrawFixture) {
    using namespace bootstrap_fixed_indices_fixture;
    std::vector<fincore::timestamp> index(performance_returns.size());
    for (std::size_t position = 0; position < index.size(); ++position) {
        index[position] = static_cast<fincore::timestamp>(position);
    }
    const auto returns =
        fincore::make_series(index, "returns", performance_returns);
    // The first pinned draw is the population in original order, so its column
    // of samples is the plain perf_stats of the fixture; only later draws test
    // a resample, which is why order-sensitive metrics differ there.
    const auto actual = fincore::perf_stats_bootstrap(
        returns, std::span<const std::vector<std::size_t>>{performance_draws});

    ASSERT_EQ(actual.size(), performance_names.size());
    ASSERT_EQ(performance_samples_by_draw.size(), performance_draws.size());
    for (std::size_t metric = 0; metric < actual.size(); ++metric) {
        EXPECT_EQ(actual[metric].name, performance_names[metric]);
        ASSERT_EQ(actual[metric].samples.size(), performance_draws.size());
        for (std::size_t draw = 0; draw < performance_draws.size(); ++draw) {
            ASSERT_EQ(performance_samples_by_draw[draw].size(), actual.size());
            expect_matches_python_fixture(
                actual[metric].samples[draw],
                performance_samples_by_draw[draw][metric]);
        }

        const std::array<double, 8> summary{
            actual[metric].summary.mean,
            actual[metric].summary.median,
            actual[metric].summary.std,
            actual[metric].summary.p5,
            actual[metric].summary.p25,
            actual[metric].summary.p75,
            actual[metric].summary.p95,
            actual[metric].summary.iqr,
        };
        for (std::size_t field = 0; field < summary.size(); ++field) {
            expect_matches_python_fixture(summary[field],
                                          performance_summary[metric][field]);
        }
    }
}

TEST(PerfStats, FixedDrawBootstrapPreservesEmptyAndRejectsMalformedDraws) {
    const std::vector<std::vector<std::size_t>> no_draws;
    const auto returns = bootstrap_fixture();
    const auto empty_distribution = fincore::perf_stats_bootstrap(
        returns, std::span<const std::vector<std::size_t>>{no_draws});
    ASSERT_EQ(empty_distribution.size(), 13U);
    for (const auto& metric : empty_distribution) {
        EXPECT_TRUE(metric.samples.empty());
        EXPECT_TRUE(std::isnan(metric.summary.mean));
    }

    const auto empty_input = fincore::make_series({}, "returns", {});
    const auto empty_result = fincore::perf_stats_bootstrap(
        empty_input, std::span<const std::vector<std::size_t>>{no_draws});
    ASSERT_EQ(empty_result.size(), 13U);
    for (const auto& metric : empty_result) {
        EXPECT_TRUE(metric.samples.empty());
        EXPECT_TRUE(std::isnan(metric.summary.mean));
    }

    const std::vector<std::vector<std::size_t>> one_short{{0U, 1U}};
    EXPECT_THROW(static_cast<void>(fincore::perf_stats_bootstrap(
                     returns, std::span<const std::vector<std::size_t>>{one_short})),
                 fincore::input_error);
    const std::vector<std::vector<std::size_t>> one_out_of_range{
        {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 10U}};
    EXPECT_THROW(static_cast<void>(fincore::perf_stats_bootstrap(
                     returns,
                     std::span<const std::vector<std::size_t>>{one_out_of_range})),
                 fincore::input_error);
    const std::vector<std::vector<std::size_t>> one_empty_draw{{}};
    EXPECT_THROW(static_cast<void>(fincore::perf_stats_bootstrap(
                     empty_input,
                     std::span<const std::vector<std::size_t>>{one_empty_draw})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::perf_stats_bootstrap(
                     returns, std::span<const std::vector<std::size_t>>{no_draws}, "")),
                 fincore::input_error);
}

}  // namespace
