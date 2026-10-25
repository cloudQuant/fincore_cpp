#include <fincore/rolling_engine.hpp>
#include <fincore/metrics/rolling.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

void expect_rolling_series_equal(const std::string_view metric,
                                const fincore::dataframe& actual,
                                const fincore::dataframe& expected) {
    EXPECT_EQ(actual.get_index(), expected.get_index()) << metric;
    const auto actual_values =
        fincore::require_double_column(actual, fincore::returns_column);
    const auto expected_values =
        fincore::require_double_column(expected, fincore::returns_column);
    ASSERT_EQ(actual_values.size(), expected_values.size()) << metric;
    for (std::size_t row = 0U; row < actual_values.size(); ++row) {
        if (std::isnan(expected_values[row])) {
            EXPECT_TRUE(std::isnan(actual_values[row]))
                << metric << " row " << row;
        } else if (std::isinf(expected_values[row])) {
            EXPECT_EQ(actual_values[row], expected_values[row])
                << metric << " row " << row;
        } else {
            EXPECT_TRUE(std::isfinite(actual_values[row]))
                << metric << " row " << row;
            EXPECT_NEAR(actual_values[row], expected_values[row], 1e-12)
                << metric << " row " << row;
        }
    }
}

TEST(RollingEngine, PreservesWindowEndIndex) {
    fincore::rolling_engine engine{
        fincore::make_series({1, 2, 3, 4}, "returns", {0.01, 0.02, -0.01, 0.03}),
        std::nullopt, 3U};
    const std::array metrics{fincore::rolling_metric::mean_return};
    const auto result = engine.compute(metrics);

    EXPECT_EQ(result.at("mean_return").get_index(),
              (std::vector<fincore::timestamp>{3, 4}));
    const auto& mean_values = result.at("mean_return").get_column<double>(
        fincore::returns_column.data());
    EXPECT_NEAR(mean_values[0], 1.68, 1e-12);
    EXPECT_NEAR(mean_values[1], 3.36, 1e-12);
}

TEST(RollingEngine, RejectsInvalidWindowAndMissingBetaBenchmark) {
    const auto returns = fincore::make_series({1, 2}, "returns", {0.01, 0.02});
    EXPECT_THROW((fincore::rolling_engine{returns, std::nullopt, 1U}),
                 fincore::input_error);
    const fincore::rolling_engine engine{returns, std::nullopt, 2U};
    const std::array metrics{fincore::rolling_metric::beta};
    EXPECT_THROW(static_cast<void>(engine.compute(metrics)), fincore::input_error);
}

TEST(RollingEngine, AlignsBetaWindowsBeforeChoosingTheirEndIndex) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, 0.02, 0.03, 0.04});
    const auto benchmark = fincore::make_series(
        {2, 3, 4, 5}, "benchmark", {0.01, 0.02, 0.03, 0.04});
    const fincore::rolling_engine engine{returns, benchmark, 3U};
    const std::array metrics{fincore::rolling_metric::beta};

    const auto result = engine.compute(metrics);
    const auto& beta_result = result.at("beta");

    EXPECT_EQ(beta_result.get_index(), (std::vector<fincore::timestamp>{4}));
    EXPECT_NEAR(beta_result.get_column<double>("returns")[0], 1.0, 1e-12);
}

TEST(RollingEngine, ComputesAllSupportedMetricsAndRejectsUnknownEnumValues) {
    const auto returns =
        fincore::make_series({1, 2, 3}, "returns", {0.01, -0.02, 0.03});
    const auto benchmark =
        fincore::make_series({1, 2, 3}, "benchmark", {0.02, -0.04, 0.06});
    const fincore::rolling_engine engine{returns, benchmark, 3U};
    const std::array metrics{fincore::rolling_metric::volatility,
                             fincore::rolling_metric::sharpe,
                             fincore::rolling_metric::max_drawdown,
                             fincore::rolling_metric::beta};

    const auto result = engine.compute(metrics);
    EXPECT_EQ(result.size(), metrics.size());
    EXPECT_NEAR(result.at("max_drawdown").get_column<double>("returns")[0], -0.02,
                1e-12);

    const std::array invalid{static_cast<fincore::rolling_metric>(99)};
    EXPECT_THROW(static_cast<void>(engine.compute(invalid)), fincore::input_error);
}

TEST(RollingEngine, LongWindowNanAndSparseBenchmarkMatchRollingOracles) {
    constexpr std::size_t rows = 620U;
    constexpr std::size_t window = 252U;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto start = fincore::utc_timestamp(2020, 1, 1);
    constexpr std::int64_t day_ns = 86'400'000'000'000LL;
    std::vector<fincore::timestamp> return_index;
    std::vector<double> returns;
    std::vector<fincore::timestamp> benchmark_index;
    std::vector<double> benchmark;
    return_index.reserve(rows);
    returns.reserve(rows);
    benchmark_index.reserve(rows);
    benchmark.reserve(rows);

    for (std::size_t row = 0U; row < rows; ++row) {
        const auto timestamp =
            start + static_cast<std::int64_t>(row) * day_ns;
        const int return_cycle = static_cast<int>((row * 37U + 11U) % 199U) - 99;
        const int return_noise = static_cast<int>((row * 13U + 7U) % 23U) - 11;
        double return_value = static_cast<double>(return_cycle) * 0.00008 +
                              static_cast<double>(return_noise) * 0.000003;
        if (row == 303U || row == 571U) {
            return_value = nan;
        }
        return_index.push_back(timestamp);
        returns.push_back(return_value);

        if (row != 0U && row % 31U == 0U) {
            continue;
        }
        const int benchmark_cycle =
            static_cast<int>((row * 71U + 3U) % 211U) - 105;
        const int benchmark_noise =
            static_cast<int>((row * 11U + 2U) % 17U) - 8;
        double benchmark_value =
            static_cast<double>(benchmark_cycle) * 0.00005 +
            static_cast<double>(benchmark_noise) * 0.000002;
        if (row == 319U || row == 580U) {
            benchmark_value = nan;
        }
        benchmark_index.push_back(timestamp);
        benchmark.push_back(benchmark_value);
    }

    const auto returns_frame =
        fincore::make_series(return_index, fincore::returns_column, returns);
    const auto benchmark_frame = fincore::make_series(
        benchmark_index, fincore::benchmark_column, benchmark);
    const fincore::rolling_engine engine{
        returns_frame, benchmark_frame, window};
    const std::array metrics{fincore::rolling_metric::mean_return,
                             fincore::rolling_metric::volatility,
                             fincore::rolling_metric::sharpe,
                             fincore::rolling_metric::max_drawdown,
                             fincore::rolling_metric::beta};
    const auto actual = engine.compute(metrics);

    std::vector<fincore::timestamp> mean_index;
    std::vector<double> mean_values;
    const std::size_t output_rows = rows - window + 1U;
    mean_index.reserve(output_rows);
    mean_values.reserve(output_rows);
    for (std::size_t begin = 0U; begin < output_rows; ++begin) {
        double total = 0.0;
        for (std::size_t offset = 0U; offset < window; ++offset) {
            total += returns[begin + offset];
        }
        mean_index.push_back(return_index[begin + window - 1U]);
        mean_values.push_back(total / static_cast<double>(window) * 252.0);
    }
    const auto mean_oracle = fincore::make_series(
        std::move(mean_index), fincore::returns_column, std::move(mean_values));
    const auto volatility_oracle =
        fincore::roll_annual_volatility(returns_frame, window);
    const auto sharpe_oracle = fincore::roll_sharpe_ratio(returns_frame, window);
    std::vector<double> drawdown_values;
    drawdown_values.reserve(output_rows);
    for (std::size_t begin = 0U; begin < output_rows; ++begin) {
        double wealth = 100.0;
        double peak = wealth;
        double worst_drawdown = nan;
        for (std::size_t offset = 0U; offset < window; ++offset) {
            const double entry = returns[begin + offset];
            wealth *= 1.0 + (std::isnan(entry) ? 0.0 : entry);
            peak = std::fmax(peak, wealth);
            const double drawdown = (wealth - peak) / peak;
            if (!std::isnan(drawdown) &&
                (std::isnan(worst_drawdown) || drawdown < worst_drawdown)) {
                worst_drawdown = drawdown;
            }
        }
        drawdown_values.push_back(worst_drawdown);
    }
    std::vector<fincore::timestamp> drawdown_index(
        return_index.begin() + static_cast<std::ptrdiff_t>(window - 1U),
        return_index.end());
    const auto drawdown_oracle = fincore::make_series(
        std::move(drawdown_index), fincore::returns_column,
        std::move(drawdown_values));

    const auto beta_oracle =
        fincore::roll_beta(returns_frame, benchmark_frame, window);

    ASSERT_EQ(actual.size(), metrics.size());
    expect_rolling_series_equal("mean_return", actual.at("mean_return"),
                                mean_oracle);
    expect_rolling_series_equal("volatility", actual.at("volatility"),
                                volatility_oracle);
    expect_rolling_series_equal("sharpe", actual.at("sharpe"), sharpe_oracle);
    expect_rolling_series_equal("max_drawdown", actual.at("max_drawdown"),
                                drawdown_oracle);
    expect_rolling_series_equal("beta", actual.at("beta"), beta_oracle);

    EXPECT_LT(actual.at("beta").get_index().size(),
              actual.at("mean_return").get_index().size());
    const auto mean_values_actual = fincore::require_double_column(
        actual.at("mean_return"), fincore::returns_column);
    const auto volatility_values = fincore::require_double_column(
        actual.at("volatility"), fincore::returns_column);
    EXPECT_TRUE(std::any_of(mean_values_actual.begin(), mean_values_actual.end(),
                            [](const double value) {
                                return std::isnan(value);
                            }));
    EXPECT_EQ(std::count_if(volatility_values.begin(), volatility_values.end(),
                            [](const double value) {
                                return std::isfinite(value);
                            }),
              volatility_values.size());
}

}  // namespace
