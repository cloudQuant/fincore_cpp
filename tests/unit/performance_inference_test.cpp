#include <fincore/performance/inference.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

fincore::dataframe inference_fixture() {
    return fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
}

TEST(PerformanceInference, StandardErrorOfMeanMatchesPythonOracle) {
    EXPECT_NEAR(fincore::standard_error_of_mean(inference_fixture()),
                0.00896908269804914, 1e-15);

    const auto nan_mixed = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, nan, -0.02, 0.03});
    EXPECT_NEAR(fincore::standard_error_of_mean(nan_mixed),
                0.014529663145135579, 1e-15);

    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    EXPECT_TRUE(std::isnan(fincore::standard_error_of_mean(singleton)));
}

TEST(PerformanceInference, SharpeStandardErrorMatchesPythonOracle) {
    const auto returns = inference_fixture();
    EXPECT_NEAR(fincore::sharpe_standard_error(returns),
                0.31779627930188475, 1e-15);
    EXPECT_NEAR(fincore::sharpe_standard_error(returns, 0.001),
                0.3171110093724174, 1e-15);

    const auto constant =
        fincore::make_series({1, 2, 3, 4}, "returns",
                             {0.01, 0.01, 0.01, 0.01});
    EXPECT_DOUBLE_EQ(fincore::sharpe_standard_error(constant), 0.5);

    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    EXPECT_TRUE(std::isnan(fincore::sharpe_standard_error(singleton)));
}

TEST(PerformanceInference, SharpeConfidenceIntervalMatchesPythonOracle) {
    const auto returns = inference_fixture();
    const auto interval = fincore::sharpe_confidence_interval(returns);
    EXPECT_NEAR(interval.first, -0.4818505590150136, 1e-14);
    EXPECT_NEAR(interval.second, 0.7639108558483746, 1e-14);

    const auto wide = fincore::sharpe_confidence_interval(returns, 0.001, 2.58);
    EXPECT_NEAR(wide.first, -0.7123737928683265, 1e-14);
    EXPECT_NEAR(wide.second, 0.9239190154933472, 1e-14);

    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    const auto short_interval =
        fincore::sharpe_confidence_interval(singleton);
    EXPECT_TRUE(std::isnan(short_interval.first));
    EXPECT_TRUE(std::isnan(short_interval.second));
}

TEST(PerformanceInference, SupportsSelectorsAndRejectsNul) {
    const auto strategy = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};

    EXPECT_DOUBLE_EQ(fincore::standard_error_of_mean(strategy,
                                                     bounded_selector),
                     fincore::standard_error_of_mean(strategy, "strategy"));
    EXPECT_THROW(static_cast<void>(fincore::sharpe_standard_error(
                     strategy, 0.0, embedded_selector)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(
                     fincore::standard_error_of_mean(strategy, "")),
                 fincore::input_error);

    const double owned = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto frame = inference_fixture();
        return fincore::standard_error_of_mean(frame, selector);
    }();
    EXPECT_NEAR(owned, 0.00896908269804914, 1e-15);
}

}  // namespace
