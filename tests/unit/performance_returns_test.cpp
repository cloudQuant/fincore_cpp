#include <fincore/performance/returns.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

using twr_signature =
    double (*)(const fincore::dataframe&, std::string_view);

static_assert(std::is_same_v<decltype(&fincore::twr), twr_signature>);

TEST(PerformanceReturns, TwrMatchesPythonOracle) {
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, 0.02, -0.05, 0.03});
    EXPECT_NEAR(fincore::twr(ordinary), 0.008050700000000077, 1e-15);

    const auto single = fincore::make_series({1}, "returns", {0.01});
    EXPECT_NEAR(fincore::twr(single), 0.010000000000000009, 1e-15);

    const auto zeros =
        fincore::make_series({1, 2, 3}, "returns", {0.0, 0.0, 0.0});
    EXPECT_DOUBLE_EQ(fincore::twr(zeros), 0.0);
}

TEST(PerformanceReturns, TwrDropsNanOnlyAndPreservesNonfiniteClasses) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto empty = fincore::make_series({}, "returns", {});
    const auto all_nan =
        fincore::make_series({1, 2}, "returns", {nan, nan});
    const auto nan_mixed = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, nan, -0.02, 0.03});
    const auto total_loss = fincore::make_series({1, 2}, "returns", {-1.0, 0.05});
    const auto below_minus_one =
        fincore::make_series({1, 2}, "returns", {-1.5, 0.2});
    const auto positive_inf =
        fincore::make_series({1, 2}, "returns", {infinity, 0.01});
    const auto negative_inf =
        fincore::make_series({1, 2}, "returns", {-infinity, 0.01});

    EXPECT_DOUBLE_EQ(fincore::twr(empty), 0.0);
    EXPECT_DOUBLE_EQ(fincore::twr(all_nan), 0.0);
    EXPECT_NEAR(fincore::twr(nan_mixed), 0.019494000000000122, 1e-15);
    EXPECT_DOUBLE_EQ(fincore::twr(total_loss), -1.0);
    EXPECT_NEAR(fincore::twr(below_minus_one), -1.6, 1e-15);
    EXPECT_TRUE(std::isinf(fincore::twr(positive_inf)));
    EXPECT_GT(fincore::twr(positive_inf), 0.0);
    EXPECT_TRUE(std::isinf(fincore::twr(negative_inf)));
    EXPECT_LT(fincore::twr(negative_inf), 0.0);
}

TEST(PerformanceReturns, TwrSupportsSelectorsAndRejectsNul) {
    const auto strategy =
        fincore::make_series({1, 2, 3, 4}, "strategy",
                             {0.01, 0.02, -0.05, 0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};

    EXPECT_NEAR(fincore::twr(strategy, "strategy"), 0.008050700000000077,
                1e-15);
    EXPECT_DOUBLE_EQ(fincore::twr(strategy, bounded_selector),
                     fincore::twr(strategy, "strategy"));
    EXPECT_THROW(
        static_cast<void>(fincore::twr(strategy, embedded_selector)),
        fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::twr(strategy, "")),
                 fincore::input_error);
}

TEST(PerformanceReturns, TwrReturnsOwnedScalarAfterInputLifetimeEnds) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto frame = fincore::make_series(
            {1, 2, 3, 4}, "returns", {0.01, 0.02, -0.05, 0.03});
        return fincore::twr(frame, selector);
    }();

    EXPECT_NEAR(result, 0.008050700000000077, 1e-15);
}

}  // namespace
