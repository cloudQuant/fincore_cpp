#include <fincore/dataframe.hpp>
#include <fincore/error.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/rolling.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double tolerance = 1.0e-12;

const std::vector<fincore::timestamp> reference_index{1, 2, 3, 4, 5, 6};
const std::vector<double> reference_returns{.01, -.02, .03, 0.0, -.01, .02};
const std::vector<double> reference_benchmark{.005, -.01, .02, .01, -.02,
                                               .01};

fincore::dataframe reference_series(std::string_view column = "returns") {
    return fincore::make_series(reference_index, column, reference_returns);
}

fincore::dataframe reference_benchmark_series(
    std::string_view column = "benchmark") {
    return fincore::make_series(reference_index, column, reference_benchmark);
}

fincore::dataframe reference_factor_frame() {
    fincore::dataframe factors;
    factors.load_index(std::vector<fincore::timestamp>{reference_index});
    factors.load_column<double>("market", reference_benchmark,
                                hmdf::nan_policy::dont_pad_with_nans);
    factors.load_column<double>("inverse",
                                {-.005, .01, -.02, -.01, .02, -.01},
                                hmdf::nan_policy::dont_pad_with_nans);
    factors.load_column<int>("metadata", {1, 2, 3, 4, 5, 6},
                             hmdf::nan_policy::dont_pad_with_nans);
    return factors;
}

void expect_values(const fincore::dataframe& actual,
                   std::string_view column,
                   const std::vector<double>& expected) {
    const auto& values = actual.get_column<double>(std::string{column}.c_str());
    ASSERT_EQ(values.size(), expected.size());
    for (std::size_t position = 0; position < expected.size(); ++position) {
        if (std::isnan(expected[position])) {
            EXPECT_TRUE(std::isnan(values[position]));
        } else {
            EXPECT_NEAR(values[position], expected[position], tolerance);
        }
    }
}

void expect_empty_typed_series(const fincore::dataframe& actual,
                               std::string_view column) {
    EXPECT_TRUE(actual.get_index().empty());
    EXPECT_TRUE(actual.has_column(std::string{column}.c_str()));
    EXPECT_TRUE(actual.get_column<double>(std::string{column}.c_str()).empty());
    EXPECT_EQ(actual.get_columns_info<double>().size(), 1U);
}

void expect_double_column_names(
    const fincore::dataframe& actual,
    const std::vector<std::string_view>& expected_names) {
    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected_names.size());
    for (std::size_t position = 0U; position < expected_names.size(); ++position) {
        EXPECT_STREQ(std::get<0>(columns[position]).c_str(),
                     std::string{expected_names[position]}.c_str());
    }
}

template <typename Function>
void expect_input_errors(Function&& function) {
    const auto missing = fincore::make_series({1}, "other", {.01});
    EXPECT_THROW(function(missing), fincore::input_error);

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(function(wrong_type), fincore::input_error);

    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed.load_column<double>("returns", std::vector<double>{.01},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(function(malformed), fincore::input_error);
}

template <typename Function>
void expect_paired_input_errors(Function&& function) {
    const auto valid_returns = reference_series();
    const auto valid_benchmark = reference_benchmark_series();
    const auto missing_returns = fincore::make_series({1}, "other", {.01});
    const auto missing_benchmark =
        fincore::make_series({1}, "other", {.005});
    EXPECT_THROW(function(missing_returns, valid_benchmark, "returns", "benchmark"),
                 fincore::input_error);
    EXPECT_THROW(function(valid_returns, missing_benchmark, "returns", "benchmark"),
                 fincore::input_error);

    fincore::dataframe wrong_returns;
    wrong_returns.load_index(std::vector<fincore::timestamp>{1});
    wrong_returns.load_column<int>("returns", std::vector<int>{1},
                                   hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe wrong_benchmark;
    wrong_benchmark.load_index(std::vector<fincore::timestamp>{1});
    wrong_benchmark.load_column<int>("benchmark", std::vector<int>{1},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(function(wrong_returns, valid_benchmark, "returns", "benchmark"),
                 fincore::input_error);
    EXPECT_THROW(function(valid_returns, wrong_benchmark, "returns", "benchmark"),
                 fincore::input_error);

    fincore::dataframe short_returns;
    short_returns.load_index(std::vector<fincore::timestamp>{1, 2});
    short_returns.load_column<double>("returns", std::vector<double>{.01},
                                      hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe short_benchmark;
    short_benchmark.load_index(std::vector<fincore::timestamp>{1, 2});
    short_benchmark.load_column<double>("benchmark", std::vector<double>{.005},
                                        hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(function(short_returns, valid_benchmark, "returns", "benchmark"),
                 fincore::input_error);
    EXPECT_THROW(function(valid_returns, short_benchmark, "returns", "benchmark"),
                 fincore::input_error);

    EXPECT_THROW(function(valid_returns, valid_benchmark, "", "benchmark"),
                 fincore::input_error);
    EXPECT_THROW(function(valid_returns, valid_benchmark, "returns", ""),
                 fincore::input_error);
}

template <typename RollingMetric, typename ScalarMetric>
void expect_paired_endpoint_delegation(
    const fincore::dataframe& returns,
    const fincore::dataframe& benchmark,
    std::size_t window,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name,
    RollingMetric&& rolling_metric,
    ScalarMetric&& scalar_metric) {
    const auto [aligned_returns, aligned_benchmark] = fincore::align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    const auto returns_values =
        fincore::require_double_column(aligned_returns, returns_column_name);
    const auto benchmark_values =
        fincore::require_double_column(aligned_benchmark, benchmark_column_name);
    ASSERT_EQ(returns_values.size(), benchmark_values.size());
    ASSERT_GE(returns_values.size(), window);

    const auto& index = aligned_returns.get_index();
    std::vector<fincore::timestamp> expected_index;
    std::vector<double> expected_values;
    expected_index.reserve(returns_values.size() - window + 1U);
    expected_values.reserve(returns_values.size() - window + 1U);
    for (std::size_t start = 0U; start + window <= returns_values.size(); ++start) {
        const auto first = static_cast<std::ptrdiff_t>(start);
        const auto last = static_cast<std::ptrdiff_t>(start + window);
        const std::vector<fincore::timestamp> window_index(index.begin() + first,
                                                             index.begin() + last);
        const std::vector<double> window_returns(returns_values.begin() + first,
                                                 returns_values.begin() + last);
        const std::vector<double> window_benchmark(benchmark_values.begin() + first,
                                                   benchmark_values.begin() + last);
        const auto returns_window = fincore::make_series(
            window_index, returns_column_name, window_returns);
        const auto benchmark_window = fincore::make_series(
            window_index, benchmark_column_name, window_benchmark);
        expected_index.push_back(index[start + window - 1U]);
        expected_values.push_back(scalar_metric(returns_window, benchmark_window));
    }

    const auto actual = rolling_metric(returns, benchmark);
    EXPECT_EQ(actual.get_index(), expected_index);
    ASSERT_TRUE(actual.has_column(std::string{returns_column_name}.c_str()));
    EXPECT_EQ(actual.template get_columns_info<double>().size(), 1U);
    expect_values(actual, returns_column_name, expected_values);
}

TEST(RollingMetrics, MatchesPythonGoldensWithEndpointAndFullIndexShapes) {
    const auto returns = reference_series();

    const auto roll_sharpe = fincore::roll_sharpe_ratio(returns, 3U);
    EXPECT_EQ(roll_sharpe.get_index(),
              (std::vector<fincore::timestamp>{3, 4, 5, 6}));
    expect_values(roll_sharpe, "returns",
                  {4.205259864302774, 2.1026299321513866,
                   5.083911274417939, 3.464101615137753});

    const auto roll_drawdown = fincore::roll_max_drawdown(returns, 3U);
    EXPECT_EQ(roll_drawdown.get_index(),
              (std::vector<fincore::timestamp>{3, 4, 5, 6}));
    expect_values(roll_drawdown, "returns",
                  {-.019999999999999997, -.020000000000000018,
                   -.009999999999999976, -.010000000000000009});

    const auto volatility = fincore::rolling_volatility(returns, 3U);
    EXPECT_EQ(volatility.get_index(), reference_index);
    expect_values(volatility, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   .3994996871087636, .3994996871087636,
                   .3304542328371662, .24248711305964288});

    const auto sharpe = fincore::rolling_sharpe(returns, 3U);
    EXPECT_EQ(sharpe.get_index(), reference_index);
    expect_values(sharpe, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   4.205259864302774, 2.1026299321513866,
                   5.083911274417939, 3.464101615137753});
}

TEST(RollingMetrics, SupportsAnnualizationFrequencyAndNamedColumns) {
    const auto returns = reference_series("strategy");

    const auto sharpe = fincore::roll_sharpe_ratio(
        returns, 3U, 0.0, fincore::frequency::daily, 100.0, "strategy");
    EXPECT_EQ(sharpe.get_index(),
              (std::vector<fincore::timestamp>{3, 4, 5, 6}));
    expect_values(sharpe, "strategy",
                  {2.649064714130087, 1.3245323570650434,
                   3.2025630761017414, 2.1821789023599227});

    const auto volatility = fincore::rolling_volatility(
        returns, 3U, fincore::frequency::weekly, std::nullopt, "strategy");
    EXPECT_EQ(volatility.get_index(), reference_index);
    expect_values(volatility, "strategy",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   .18147543451754933, .18147543451754933,
                   .1501110699893027, .11015141094572206});

    const auto rolling_sharpe = fincore::rolling_sharpe(
        returns, 3U, fincore::frequency::weekly, std::nullopt, "strategy");
    expect_values(rolling_sharpe, "strategy",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   1.9102677317636767, .9551338658818381,
                   2.309401076758502, 1.5735915849388855});
}

TEST(RollingMetrics, PreservesOriginalUnsortedInputOrder) {
    const auto returns = fincore::make_series(
        {30, 10, 20, 5, 50, 40}, "returns", reference_returns);

    EXPECT_EQ(fincore::roll_sharpe_ratio(returns, 3U).get_index(),
              (std::vector<fincore::timestamp>{20, 5, 50, 40}));
    EXPECT_EQ(fincore::roll_max_drawdown(returns, 3U).get_index(),
              (std::vector<fincore::timestamp>{20, 5, 50, 40}));
    EXPECT_EQ(fincore::rolling_volatility(returns, 3U).get_index(),
              returns.get_index());
    EXPECT_EQ(fincore::rolling_sharpe(returns, 3U).get_index(),
              returns.get_index());
}

TEST(RollingMetrics, HandlesShortAndExactlyOneCompleteWindow) {
    const auto short_series =
        fincore::make_series({1, 2}, "returns", {.01, -.02});
    expect_empty_typed_series(fincore::roll_sharpe_ratio(short_series, 3U),
                              "returns");
    expect_empty_typed_series(fincore::roll_max_drawdown(short_series, 3U),
                              "returns");
    const auto short_volatility = fincore::rolling_volatility(short_series, 3U);
    const auto short_sharpe = fincore::rolling_sharpe(short_series, 3U);
    EXPECT_EQ(short_volatility.get_index(), short_series.get_index());
    EXPECT_EQ(short_sharpe.get_index(), short_series.get_index());
    expect_values(short_volatility, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});
    expect_values(short_sharpe, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});

    const auto one_window =
        fincore::make_series({1, 2, 3}, "returns", {.01, -.02, .03});
    EXPECT_EQ(fincore::roll_sharpe_ratio(one_window, 3U).get_index(),
              (std::vector<fincore::timestamp>{3}));
    EXPECT_EQ(fincore::roll_max_drawdown(one_window, 3U).get_index(),
              (std::vector<fincore::timestamp>{3}));
    EXPECT_EQ(fincore::rolling_volatility(one_window, 3U).get_index(),
              one_window.get_index());
    EXPECT_EQ(fincore::rolling_sharpe(one_window, 3U).get_index(),
              one_window.get_index());
}

TEST(RollingMetrics, AppliesTheRequiredNanPolicies) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {.01, std::numeric_limits<double>::quiet_NaN(), -.01});

    expect_values(fincore::roll_sharpe_ratio(returns, 3U), "returns",
                  {0.0});
    expect_values(fincore::roll_max_drawdown(returns, 3U), "returns", {0.0});
    expect_values(fincore::rolling_volatility(returns, 3U), "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});
    expect_values(fincore::rolling_sharpe(returns, 3U), "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});
}

TEST(RollingMetrics, IncludesInitialWealthPointForNanPoisonedDrawdowns) {
    const auto leading_nan = fincore::make_series(
        {1, 2, 3}, "returns",
        {std::numeric_limits<double>::quiet_NaN(), .01, -.01});
    const auto all_nan = fincore::make_series(
        {1, 2, 3}, "returns",
        {std::numeric_limits<double>::quiet_NaN(),
         std::numeric_limits<double>::quiet_NaN(),
         std::numeric_limits<double>::quiet_NaN()});

    expect_values(fincore::roll_max_drawdown(leading_nan, 3U), "returns",
                  {0.0});
    expect_values(fincore::roll_max_drawdown(all_nan, 3U), "returns", {0.0});
}

TEST(RollingMetrics, AcceptsEmptyDoubleColumnsAndRejectsInvalidInputs) {
    const auto empty = fincore::make_series({}, "returns", {});
    expect_empty_typed_series(fincore::roll_sharpe_ratio(empty, 3U), "returns");
    expect_empty_typed_series(fincore::roll_max_drawdown(empty, 3U), "returns");
    expect_empty_typed_series(fincore::rolling_volatility(empty, 3U), "returns");
    expect_empty_typed_series(fincore::rolling_sharpe(empty, 3U), "returns");

    expect_input_errors(
        [](const fincore::dataframe& frame) { return fincore::roll_sharpe_ratio(frame, 3U); });
    expect_input_errors(
        [](const fincore::dataframe& frame) { return fincore::roll_max_drawdown(frame, 3U); });
    expect_input_errors(
        [](const fincore::dataframe& frame) { return fincore::rolling_volatility(frame, 3U); });
    expect_input_errors(
        [](const fincore::dataframe& frame) { return fincore::rolling_sharpe(frame, 3U); });

    const auto returns = reference_series();
    EXPECT_THROW(fincore::roll_sharpe_ratio(returns, 0U), fincore::input_error);
    EXPECT_THROW(fincore::roll_max_drawdown(returns, 0U), fincore::input_error);
    EXPECT_THROW(fincore::rolling_volatility(returns, 0U), fincore::input_error);
    EXPECT_THROW(fincore::rolling_sharpe(returns, 0U), fincore::input_error);
}

TEST(RollingMetrics, PairedEndpointsMatchPythonGoldensAndOutputShape) {
    const auto returns = reference_series();
    const auto benchmark = reference_benchmark_series();
    const auto expected_index = std::vector<fincore::timestamp>{3, 4, 5, 6};

    const auto alpha = fincore::roll_alpha(returns, benchmark, 3U);
    const auto beta = fincore::roll_beta(returns, benchmark, 3U);
    const auto up_capture = fincore::roll_up_capture(returns, benchmark, 3U);
    const auto down_capture =
        fincore::roll_down_capture(returns, benchmark, 3U);
    const auto up_down_capture =
        fincore::roll_up_down_capture(returns, benchmark, 3U);

    for (const auto* actual : {&alpha, &beta, &up_capture, &down_capture,
                               &up_down_capture}) {
        EXPECT_EQ(actual->get_index(), expected_index);
        EXPECT_TRUE(actual->has_column("returns"));
        EXPECT_EQ(actual->get_columns_info<double>().size(), 1U);
    }
    expect_values(alpha, "returns",
                  {-.3431833620809164, -.8357653939371649,
                   1.6310200676280466, 1.313133510455331});
    expect_values(beta, "returns",
                  {1.6666666666666659, 1.571428571428571,
                   .8461538461538458, .6666666666666666});
    expect_values(up_capture, "returns",
                  {6.637108874074204, .9752504552841719,
                   .9752504552841719, .9866347325869022});
    expect_values(down_capture, "returns",
                  {1.079619499803848, 1.079619499803848,
                   .9262522584870747, .9262522584870747});
    expect_values(up_down_capture, "returns",
                  {6.147637084435837, .903327936797512,
                   1.0528994087173726, 1.0651900964845744});
}

TEST(RollingMetrics, PairedEndpointsSupportCustomArgumentsAndBetaCompatibility) {
    const auto returns = reference_series("strategy");
    const auto benchmark = reference_benchmark_series("market");

    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "strategy", "market",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_alpha(left, right, 3U, .001,
                                       fincore::frequency::weekly, 7.0,
                                       "strategy", "market");
        },
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::alpha(left, right, .001, 7.0, "strategy", "market");
        });

    const auto ignored_period = static_cast<fincore::frequency>(999);
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "strategy", "market",
        [ignored_period](const fincore::dataframe& left,
                         const fincore::dataframe& right) {
            return fincore::roll_beta(
                left, right, 3U, .001, ignored_period,
                std::numeric_limits<double>::quiet_NaN(), "strategy", "market");
        },
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::beta(left, right, .001, "strategy", "market");
        });

    const double weekly_factor =
        fincore::annualization_factor(fincore::frequency::weekly, std::nullopt);
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "strategy", "market",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_up_capture(left, right, 3U,
                                             fincore::frequency::weekly,
                                             "strategy", "market");
        },
        [weekly_factor](const fincore::dataframe& left,
                        const fincore::dataframe& right) {
            return fincore::up_capture(left, right, weekly_factor, "strategy", "market");
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "strategy", "market",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_down_capture(left, right, 3U,
                                               fincore::frequency::weekly,
                                               "strategy", "market");
        },
        [weekly_factor](const fincore::dataframe& left,
                        const fincore::dataframe& right) {
            return fincore::down_capture(left, right, weekly_factor, "strategy", "market");
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "strategy", "market",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_up_down_capture(left, right, 3U,
                                                  fincore::frequency::weekly,
                                                  "strategy", "market");
        },
        [weekly_factor](const fincore::dataframe& left,
                        const fincore::dataframe& right) {
            return fincore::up_down_capture(left, right, weekly_factor,
                                            "strategy", "market");
        });
}

TEST(RollingMetrics, PairedEndpointsDelegateUnsortedAlignedWindowsToScalars) {
    const auto returns = fincore::make_series(
        {30, 10, 20, 5, 50}, "returns", {.01, -.02, .03, -.01, .02});
    const auto benchmark = fincore::make_series(
        {20, 30, 5, 40, 10}, "benchmark", {.02, .005, -.01, .03, -.01});
    const auto [aligned_returns, aligned_benchmark] = fincore::align_on_common_index(
        returns, "returns", benchmark, "benchmark");
    EXPECT_EQ(aligned_returns.get_index(),
              (std::vector<fincore::timestamp>{30, 10, 20, 5}));
    EXPECT_EQ(aligned_benchmark.get_index(), aligned_returns.get_index());

    const double daily_factor =
        fincore::annualization_factor(fincore::frequency::daily, std::nullopt);
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_alpha(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::alpha(left, right, 0.0, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_beta(left, right, 3U);
        },
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::beta(left, right);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_up_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::up_capture(left, right, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_down_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::down_capture(left, right, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_up_down_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::up_down_capture(left, right, daily_factor);
        });
}

TEST(RollingMetrics, PairedEndpointsDelegateNanWindowsToScalars) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5}, "returns",
        {.01, std::numeric_limits<double>::quiet_NaN(), .03, -.02, .04});
    const auto benchmark = fincore::make_series(
        {1, 2, 3, 4, 5}, "benchmark",
        {.005, -.01, .02, std::numeric_limits<double>::quiet_NaN(), .01});
    const double daily_factor =
        fincore::annualization_factor(fincore::frequency::daily, std::nullopt);

    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_alpha(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::alpha(left, right, 0.0, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_beta(left, right, 3U);
        },
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::beta(left, right);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_up_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::up_capture(left, right, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_down_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::down_capture(left, right, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_up_down_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::up_down_capture(left, right, daily_factor);
        });
}

TEST(RollingMetrics, PairedEndpointsReturnTypedEmptyForEmptyNoCommonAndShort) {
    const auto populated_returns =
        fincore::make_series({1, 2, 3}, "returns", {.01, -.02, .03});
    const auto populated_benchmark =
        fincore::make_series({1, 2, 3}, "benchmark", {.005, -.01, .02});
    const auto empty_returns = fincore::make_series({}, "returns", {});
    const auto empty_benchmark = fincore::make_series({}, "benchmark", {});
    const auto expect_all_empty = [](const fincore::dataframe& left,
                                     const fincore::dataframe& right) {
        expect_empty_typed_series(fincore::roll_alpha(left, right, 3U), "returns");
        expect_empty_typed_series(fincore::roll_beta(left, right, 3U), "returns");
        expect_empty_typed_series(fincore::roll_up_capture(left, right, 3U),
                                  "returns");
        expect_empty_typed_series(fincore::roll_down_capture(left, right, 3U),
                                  "returns");
        expect_empty_typed_series(fincore::roll_up_down_capture(left, right, 3U),
                                  "returns");
    };

    expect_all_empty(empty_returns, populated_benchmark);
    expect_all_empty(populated_returns, empty_benchmark);
    expect_all_empty(fincore::make_series({1, 2}, "returns", {.01, -.02}),
                     fincore::make_series({3, 4}, "benchmark", {.005, -.01}));
    expect_all_empty(fincore::make_series({1, 2, 3}, "returns", {.01, -.02, .03}),
                     fincore::make_series({2, 3, 4}, "benchmark", {-.01, .02, .01}));
}

TEST(RollingMetrics,
     PairedEndpointsValidateAnnualizationBeforeReturningEmptyAlignedResults) {
    const auto populated_benchmark =
        fincore::make_series({1, 2, 3}, "benchmark", {.005, -.01, .02});
    const auto empty_returns = fincore::make_series({}, "returns", {});
    const auto short_returns =
        fincore::make_series({1, 2, 3}, "returns", {.01, -.02, .03});
    const auto short_benchmark =
        fincore::make_series({2, 3, 4}, "benchmark", {-.01, .02, .01});
    const auto unknown_frequency = static_cast<fincore::frequency>(999);
    const auto expect_validation_before_empty =
        [unknown_frequency](const fincore::dataframe& returns,
                            const fincore::dataframe& benchmark) {
            EXPECT_THROW(fincore::roll_alpha(returns, benchmark, 3U, 0.0,
                                             unknown_frequency),
                         fincore::input_error);
            EXPECT_THROW(fincore::roll_up_capture(returns, benchmark, 3U,
                                                  unknown_frequency),
                         fincore::input_error);
            EXPECT_THROW(fincore::roll_down_capture(returns, benchmark, 3U,
                                                    unknown_frequency),
                         fincore::input_error);
            EXPECT_THROW(fincore::roll_up_down_capture(returns, benchmark, 3U,
                                                       unknown_frequency),
                         fincore::input_error);

            expect_empty_typed_series(
                fincore::roll_beta(returns, benchmark, 3U, 0.0,
                                   unknown_frequency,
                                   std::numeric_limits<double>::quiet_NaN()),
                "returns");
        };

    expect_validation_before_empty(empty_returns, populated_benchmark);
    expect_validation_before_empty(short_returns, short_benchmark);
}

TEST(RollingMetrics, PairedEndpointsHandleOneWindowZeroWindowAndInputErrors) {
    const auto returns =
        fincore::make_series({1, 2, 3}, "returns", {.01, -.02, .03});
    const auto benchmark =
        fincore::make_series({1, 2, 3}, "benchmark", {.005, -.01, .02});
    const double daily_factor =
        fincore::annualization_factor(fincore::frequency::daily, std::nullopt);

    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_alpha(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::alpha(left, right, 0.0, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_beta(left, right, 3U);
        },
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::beta(left, right);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_up_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::up_capture(left, right, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_down_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::down_capture(left, right, daily_factor);
        });
    expect_paired_endpoint_delegation(
        returns, benchmark, 3U, "returns", "benchmark",
        [](const fincore::dataframe& left, const fincore::dataframe& right) {
            return fincore::roll_up_down_capture(left, right, 3U);
        },
        [daily_factor](const fincore::dataframe& left,
                       const fincore::dataframe& right) {
            return fincore::up_down_capture(left, right, daily_factor);
        });

    const auto empty_returns = fincore::make_series({}, "returns", {});
    const auto empty_benchmark = fincore::make_series({}, "benchmark", {});
    EXPECT_THROW(fincore::roll_alpha(empty_returns, empty_benchmark, 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::roll_beta(empty_returns, empty_benchmark, 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::roll_up_capture(empty_returns, empty_benchmark, 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::roll_down_capture(empty_returns, empty_benchmark, 0U),
                 fincore::input_error);
    EXPECT_THROW(
        fincore::roll_up_down_capture(empty_returns, empty_benchmark, 0U),
        fincore::input_error);

    expect_paired_input_errors(
        [](const fincore::dataframe& left, const fincore::dataframe& right,
           std::string_view returns_column_name,
           std::string_view benchmark_column_name) {
            return fincore::roll_alpha(left, right, 3U, 0.0,
                                       fincore::frequency::daily, std::nullopt,
                                       returns_column_name, benchmark_column_name);
        });
    expect_paired_input_errors(
        [](const fincore::dataframe& left, const fincore::dataframe& right,
           std::string_view returns_column_name,
           std::string_view benchmark_column_name) {
            return fincore::roll_beta(left, right, 3U, 0.0,
                                      fincore::frequency::daily, std::nullopt,
                                      returns_column_name, benchmark_column_name);
        });
    expect_paired_input_errors(
        [](const fincore::dataframe& left, const fincore::dataframe& right,
           std::string_view returns_column_name,
           std::string_view benchmark_column_name) {
            return fincore::roll_up_capture(left, right, 3U,
                                            fincore::frequency::daily,
                                            returns_column_name, benchmark_column_name);
        });
    expect_paired_input_errors(
        [](const fincore::dataframe& left, const fincore::dataframe& right,
           std::string_view returns_column_name,
           std::string_view benchmark_column_name) {
            return fincore::roll_down_capture(left, right, 3U,
                                              fincore::frequency::daily,
                                              returns_column_name, benchmark_column_name);
        });
    expect_paired_input_errors(
        [](const fincore::dataframe& left, const fincore::dataframe& right,
           std::string_view returns_column_name,
           std::string_view benchmark_column_name) {
            return fincore::roll_up_down_capture(left, right, 3U,
                                                 fincore::frequency::daily,
                                                 returns_column_name,
                                                 benchmark_column_name);
        });
}

TEST(RollingMetrics, AlphaBetaAndBetaFacadesMatchNativeRollingGoldens) {
    const auto returns = reference_series();
    const auto benchmark = reference_benchmark_series();
    const auto factors = reference_factor_frame();
    const std::array<std::string_view, 2U> factor_names{"market", "inverse"};
    const auto expected_endpoints = std::vector<fincore::timestamp>{3, 4, 5, 6};
    const std::vector<double> expected_alpha{
        -.3431833620809164, -.8357653939371649, 1.6310200676280466,
        1.313133510455331};
    const std::vector<double> expected_beta{
        1.6666666666666667, 1.5714285714285716, .846153846153846,
        .6666666666666666};

    const auto alpha_beta = fincore::roll_alpha_beta(returns, benchmark, 3U);
    EXPECT_EQ(alpha_beta.get_index(), expected_endpoints);
    expect_double_column_names(alpha_beta, {"alpha", "beta"});
    expect_values(alpha_beta, "alpha", expected_alpha);
    expect_values(alpha_beta, "beta", expected_beta);

    const auto first_returns = fincore::make_series(
        {1, 2, 3}, "returns", {.01, -.02, .03});
    const auto first_benchmark = fincore::make_series(
        {1, 2, 3}, "benchmark", {.005, -.01, .02});
    const auto direct = fincore::alpha_beta(first_returns, first_benchmark);
    EXPECT_NEAR(alpha_beta.get_column<double>("alpha")[0], direct.first,
                tolerance);
    EXPECT_NEAR(alpha_beta.get_column<double>("beta")[0], direct.second,
                tolerance);

    const auto single_beta = fincore::rolling_beta(returns, benchmark, 3U);
    EXPECT_EQ(single_beta.get_index(), reference_index);
    expect_values(single_beta, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   1.6666666666666659, 1.571428571428571,
                   .8461538461538458, .6666666666666666});

    const auto multi_beta =
        fincore::rolling_beta(returns, factors, factor_names, 3U);
    EXPECT_EQ(multi_beta.get_index(), reference_index);
    expect_double_column_names(multi_beta, {"market", "inverse"});
    expect_values(multi_beta, "market",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   1.6666666666666659, 1.571428571428571,
                   .8461538461538458, .6666666666666666});
    expect_values(multi_beta, "inverse",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   -1.6666666666666659, -1.571428571428571,
                   -.8461538461538458, -.6666666666666666});

    const auto regression = fincore::rolling_regression(returns, benchmark, 3U);
    EXPECT_EQ(regression.get_index(), expected_endpoints);
    expect_double_column_names(regression, {"alpha", "beta"});
    expect_values(regression, "alpha",
                  {-.0016666666666666644, -.007142857142857138,
                   .0038461538461538455, .0033333333333333322});
    expect_values(regression, "beta",
                  {1.6666666666666659, 1.571428571428571,
                   .8461538461538458, .6666666666666666});
}

TEST(RollingMetrics, NewRollingFacadesAlignAndPreserveSpecifiedShapes) {
    const auto returns = fincore::make_series(
        {30, 10, 20, 5, 50}, "strategy", {.01, -.02, .03, -.01, .02});
    const auto benchmark = fincore::make_series(
        {20, 30, 5, 40, 10}, "market", {.02, .005, -.01, .03, -.01});
    const auto alpha_beta = fincore::roll_alpha_beta(
        returns, benchmark, 3U, 0.0, fincore::frequency::daily, std::nullopt,
        "strategy", "market");
    const auto beta = fincore::rolling_beta(returns, benchmark, 3U,
                                             "strategy", "market");
    const auto regression = fincore::rolling_regression(returns, benchmark, 3U,
                                                         "strategy", "market");
    EXPECT_EQ(alpha_beta.get_index(),
              (std::vector<fincore::timestamp>{20, 5}));
    EXPECT_EQ(beta.get_index(),
              (std::vector<fincore::timestamp>{30, 10, 20, 5}));
    EXPECT_EQ(regression.get_index(),
              (std::vector<fincore::timestamp>{20, 5}));
    EXPECT_TRUE(beta.has_column("strategy"));

    const auto short_returns = fincore::make_series({1, 2}, "returns", {.01, -.02});
    const auto short_benchmark =
        fincore::make_series({1, 2}, "benchmark", {.005, -.01});
    const auto short_alpha_beta =
        fincore::roll_alpha_beta(short_returns, short_benchmark, 3U);
    const auto short_regression =
        fincore::rolling_regression(short_returns, short_benchmark, 3U);
    for (const auto* output : {&short_alpha_beta, &short_regression}) {
        EXPECT_TRUE(output->get_index().empty());
        expect_double_column_names(*output, {"alpha", "beta"});
    }
    const auto no_common_beta = fincore::rolling_beta(
        fincore::make_series({1, 2}, "returns", {.01, -.02}),
        fincore::make_series({3, 4}, "benchmark", {.005, -.01}), 3U);
    EXPECT_TRUE(no_common_beta.get_index().empty());
    EXPECT_TRUE(no_common_beta.has_column("returns"));
}

TEST(RollingMetrics, NewRollingFacadesValidateInputsAndInvalidateFixedWindows) {
    const auto returns = reference_series();
    const auto benchmark = reference_benchmark_series();
    const auto factors = reference_factor_frame();
    const std::array<std::string_view, 2U> duplicate{"market", "market"};
    const std::array<std::string_view, 1U> empty{""};
    const std::array<std::string_view, 1U> missing{"absent"};
    const std::array<std::string_view, 1U> wrong_type{"metadata"};
    const std::array<std::string_view, 0U> none{};
    EXPECT_THROW(fincore::roll_alpha_beta(returns, benchmark, 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_beta(returns, benchmark, 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_regression(returns, benchmark, 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_beta(returns, factors, duplicate, 3U),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_beta(returns, factors, empty, 3U),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_beta(returns, factors, missing, 3U),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_beta(returns, factors, wrong_type, 3U),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_beta(returns, factors, none, 3U),
                 fincore::input_error);
    fincore::dataframe malformed_factors;
    malformed_factors.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed_factors.load_column<double>("market", std::vector<double>{.01},
                                          hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::rolling_beta(returns, malformed_factors,
                                       std::array<std::string_view, 1U>{"market"},
                                       3U),
                 fincore::input_error);
    EXPECT_THROW(fincore::roll_alpha_beta(
                     returns, benchmark, 3U, 0.0,
                     static_cast<fincore::frequency>(999)),
                 fincore::input_error);

    const auto constant = fincore::make_series(
        reference_index, "benchmark", {.01, .01, .01, .01, .01, .01});
    const auto constant_beta = fincore::rolling_beta(returns, constant, 3U);
    const auto constant_regression =
        fincore::rolling_regression(returns, constant, 3U);
    expect_values(constant_beta, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});
    EXPECT_TRUE(constant_regression.get_index().empty());

    const auto nonfinite_returns = fincore::make_series(
        reference_index, "returns",
        {.01, std::numeric_limits<double>::quiet_NaN(), .03, 0.0, -.01, .02});
    const auto nonfinite_benchmark = fincore::make_series(
        reference_index, "benchmark",
        {.005, -.01, std::numeric_limits<double>::infinity(), .01, -.02, .01});
    const auto invalid_beta =
        fincore::rolling_beta(nonfinite_returns, nonfinite_benchmark, 3U);
    expect_values(invalid_beta, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   .6666666666666666});
    EXPECT_EQ(fincore::rolling_regression(nonfinite_returns, nonfinite_benchmark,
                                          3U)
                  .get_index(),
              (std::vector<fincore::timestamp>{6}));
}

}  // namespace
