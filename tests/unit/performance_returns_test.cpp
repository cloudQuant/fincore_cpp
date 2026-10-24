#include <fincore/calendar.hpp>
#include <fincore/performance/returns.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr long double irr_residual_budget = 1e-10L;

long double normalized_irr_residual(const std::vector<double>& cashflows,
                                    const std::vector<double>& periods,
                                    const double rate) {
    if (cashflows.size() != periods.size() || !std::isfinite(rate) ||
        !(rate > -1.0)) {
        return std::numeric_limits<long double>::infinity();
    }

    const long double log_gross =
        std::log1p(static_cast<long double>(rate));
    std::vector<long double> log_magnitudes;
    log_magnitudes.reserve(cashflows.size());
    for (std::size_t position = 0; position < cashflows.size(); ++position) {
        if (cashflows[position] == 0.0) {
            continue;
        }
        log_magnitudes.push_back(
            std::log(std::fabs(static_cast<long double>(cashflows[position]))) -
            static_cast<long double>(periods[position]) * log_gross);
    }
    if (log_magnitudes.empty()) {
        return std::numeric_limits<long double>::infinity();
    }

    const long double anchor =
        *std::max_element(log_magnitudes.begin(), log_magnitudes.end());
    long double signed_sum = 0.0L;
    long double absolute_sum = 0.0L;
    std::size_t term_position = 0;
    for (const double cashflow : cashflows) {
        if (cashflow == 0.0) {
            continue;
        }
        const long double magnitude =
            std::exp(log_magnitudes[term_position++] - anchor);
        signed_sum += std::copysign(magnitude,
                                    static_cast<long double>(cashflow));
        absolute_sum += magnitude;
    }
    return absolute_sum > 0.0L
               ? std::fabs(signed_sum) / absolute_sum
               : std::numeric_limits<long double>::infinity();
}

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

TEST(PerformanceReturns, XirrMatchesPythonOracleForConventionalFlows) {
    const auto two_flows = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 1, 1),
                                        fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {-1000.0, 1100.0});
    EXPECT_NEAR(fincore::xirr(two_flows), 0.09971358593414104, 1e-12);

    const auto three_flows = fincore::make_series(
        std::vector<fincore::timestamp>{
            fincore::utc_timestamp(2020, 1, 1),
            fincore::utc_timestamp(2020, 7, 1),
            fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {-1000.0, 300.0, 800.0});
    EXPECT_NEAR(fincore::xirr(three_flows), 0.11684905803770408, 1e-12);

    const auto same_day = fincore::make_series(
        std::vector<fincore::timestamp>{
            fincore::utc_timestamp(2020, 1, 1),
            fincore::utc_timestamp(2020, 1, 1),
            fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {-1000.0, -500.0, 1600.0});
    EXPECT_NEAR(fincore::xirr(same_day), 0.06647859284144313, 1e-12);

    const auto zero_dropped = fincore::make_series(
        std::vector<fincore::timestamp>{
            fincore::utc_timestamp(2020, 1, 1),
            fincore::utc_timestamp(2020, 6, 1),
            fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {-1000.0, 0.0, 1100.0});
    EXPECT_DOUBLE_EQ(fincore::xirr(zero_dropped), fincore::xirr(two_flows));

    const auto unsorted = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2021, 1, 1),
                                        fincore::utc_timestamp(2020, 1, 1)},
        "cashflow", {1100.0, -1000.0});
    EXPECT_DOUBLE_EQ(fincore::xirr(unsorted), fincore::xirr(two_flows));
}

TEST(PerformanceReturns, XirrRejectsNonConventionalAndInvalidInputs) {
    const auto non_conventional = fincore::make_series(
        std::vector<fincore::timestamp>{
            fincore::utc_timestamp(2020, 1, 1),
            fincore::utc_timestamp(2020, 6, 1),
            fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {-1000.0, 2500.0, -1600.0});
    EXPECT_TRUE(std::isnan(fincore::xirr(non_conventional)));

    const auto all_negative = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 1, 1),
                                        fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {-1000.0, -100.0});
    EXPECT_TRUE(std::isnan(fincore::xirr(all_negative)));

    const auto loss = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 1, 1),
                                        fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {-1000.0, 500.0});
    EXPECT_NEAR(fincore::xirr(loss), -0.4990521803938817, 1e-12);

    const auto single_date = fincore::make_series(
        std::vector<fincore::timestamp>{
            fincore::utc_timestamp(2020, 1, 1),
            fincore::utc_timestamp(2020, 1, 1)},
        "cashflow", {-1000.0, 1100.0});
    EXPECT_THROW(static_cast<void>(fincore::xirr(single_date)),
                 fincore::input_error);

    const auto nonfinite = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 1, 1),
                                        fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {-1000.0, std::numeric_limits<double>::infinity()});
    EXPECT_THROW(static_cast<void>(fincore::xirr(nonfinite)),
                 fincore::input_error);
}

TEST(PerformanceReturns, XirrSupportsSelectorsAndOwnsItsResult) {
    const auto named = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 1, 1),
                                        fincore::utc_timestamp(2021, 1, 1)},
        "amount", {-1000.0, 1100.0});
    EXPECT_NEAR(fincore::xirr(named, "amount"), 0.09971358593414104, 1e-12);

    const std::array<char, 13> embedded_storage{
        'c', 'a', 's', 'h', 'f', 'l', 'o', 'w', '\0', 'x', '!', '!', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::xirr(named, embedded_selector)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::xirr(named, "missing")),
                 fincore::input_error);

    const double owned = [] {
        const auto frame = fincore::make_series(
            std::vector<fincore::timestamp>{fincore::utc_timestamp(2020, 1, 1),
                                            fincore::utc_timestamp(2021, 1, 1)},
            "cashflow", {-1000.0, 1100.0});
        return fincore::xirr(frame);
    }();
    EXPECT_NEAR(owned, 0.09971358593414104, 1e-12);
}

TEST(PerformanceReturns, IrrResultsMeetNormalizedResidualBudget) {
    const std::vector<double> cashflows{-1.0, 0.5, 0.6};
    const std::vector<double> periods{0.0, 1.0, 2.0};
    const double expected = (0.5 + std::sqrt(2.65)) / 2.0 - 1.0;
    const auto mwr_frame = fincore::make_series({1, 2, 3}, "cashflow",
                                                cashflows);
    const double mwr_rate = fincore::mwr(mwr_frame);
    ASSERT_TRUE(std::isfinite(mwr_rate));
    EXPECT_NEAR(mwr_rate, expected, 1e-12);
    EXPECT_LE(normalized_irr_residual(cashflows, periods, mwr_rate),
              irr_residual_budget);

    const auto xirr_frame = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2021, 1, 1),
                                        fincore::utc_timestamp(2022, 1, 1),
                                        fincore::utc_timestamp(2023, 1, 1)},
        "cashflow", cashflows);
    const double xirr_rate = fincore::xirr(xirr_frame);
    ASSERT_TRUE(std::isfinite(xirr_rate));
    EXPECT_NEAR(xirr_rate, expected, 1e-12);
    EXPECT_LE(normalized_irr_residual(cashflows, periods, xirr_rate),
              irr_residual_budget);
}

TEST(PerformanceReturns, IrrSupportsRepresentableExtremeRoots) {
    const double large_amount = 1e100;
    const auto large =
        fincore::make_series({1, 2}, "cashflow", {-1.0, large_amount});
    const double large_rate = fincore::mwr(large);
    ASSERT_TRUE(std::isfinite(large_rate));
    EXPECT_NEAR(large_rate / large_amount, 1.0, 1e-10);
    EXPECT_LE(normalized_irr_residual({-1.0, large_amount}, {0.0, 1.0},
                                      large_rate),
              irr_residual_budget);

    const double smallest_gross = std::ldexp(1.0, -53);
    const auto near_minus_one = fincore::make_series(
        {1, 2}, "cashflow", {-1.0, smallest_gross});
    const double low_rate = fincore::mwr(near_minus_one);
    ASSERT_TRUE(std::isfinite(low_rate));
    EXPECT_DOUBLE_EQ(low_rate, std::nextafter(-1.0, 0.0));
    EXPECT_LE(normalized_irr_residual({-1.0, smallest_gross}, {0.0, 1.0},
                                      low_rate),
              irr_residual_budget);
}

TEST(PerformanceReturns, IrrRejectsUnrepresentableAndUnbracketedRoots) {
    const double unrepresentable_gross = 1.5 * std::ldexp(1.0, -53);
    const auto rounded_below_minus_one = fincore::make_series(
        {1, 2}, "cashflow", {-1.0, unrepresentable_gross});
    EXPECT_TRUE(std::isnan(fincore::mwr(rounded_below_minus_one)));

    const auto below_domain = fincore::make_series(
        {1, 2}, "cashflow",
        {-1.0, std::numeric_limits<double>::denorm_min()});
    EXPECT_TRUE(std::isnan(fincore::mwr(below_domain)));

    const auto above_domain = fincore::make_series(
        {1, 2}, "cashflow",
        {-1.0, std::numeric_limits<double>::max()});
    EXPECT_TRUE(std::isnan(fincore::mwr(above_domain, 0.5)));

    const auto nonfinite_solver_arithmetic =
        fincore::make_series({1, 2}, "cashflow", {-1.0, 2.0});
    EXPECT_TRUE(std::isnan(fincore::mwr(
        nonfinite_solver_arithmetic, std::numeric_limits<double>::max())));
}

TEST(PerformanceReturns, MwrUsesPositionWhileXirrSortsAndAggregatesUtcDays) {
    const auto positional = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2021, 1, 1),
                                        fincore::utc_timestamp(2020, 1, 1)},
        "cashflow", {-1000.0, 1100.0});
    EXPECT_NEAR(fincore::mwr(positional), 0.1, 1e-12);

    const auto duplicate_and_unsorted = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2022, 1, 1),
                                        fincore::utc_timestamp(2021, 1, 1),
                                        fincore::utc_timestamp(2021, 1, 1)},
        "cashflow", {1100.0, -600.0, -400.0});
    const auto sorted_two_flow = fincore::make_series(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2021, 1, 1),
                                        fincore::utc_timestamp(2022, 1, 1)},
        "cashflow", {-1000.0, 1100.0});
    EXPECT_DOUBLE_EQ(fincore::xirr(duplicate_and_unsorted),
                     fincore::xirr(sorted_two_flow));
}

TEST(PerformanceReturns, MwrMatchesPythonOracleForConventionalFlows) {
    const auto two_flows = fincore::make_series(
        {1, 2}, "cashflow", {-1000.0, 1100.0});
    EXPECT_NEAR(fincore::mwr(two_flows), 0.10000000000000002, 1e-12);
    EXPECT_NEAR(fincore::mwr(two_flows, 0.5), 0.21000000000000005, 1e-12);

    const auto three_flows =
        fincore::make_series({1, 2, 3}, "cashflow", {-1000.0, 300.0, 800.0});
    EXPECT_NEAR(fincore::mwr(three_flows), 0.05691785736085275, 1e-12);

    const auto zero_dropped = fincore::make_series(
        {1, 2, 3}, "cashflow", {-1000.0, 0.0, 1100.0});
    EXPECT_NEAR(fincore::mwr(zero_dropped), 0.048808848170151624, 1e-12);

    const auto loss = fincore::make_series({1, 2}, "cashflow",
                                           {-1000.0, 500.0});
    EXPECT_NEAR(fincore::mwr(loss), -0.5000000000000002, 1e-12);
}

TEST(PerformanceReturns, MwrRejectsNonConventionalAndInvalidInputs) {
    const auto non_conventional = fincore::make_series(
        {1, 2, 3}, "cashflow", {-1000.0, 2500.0, -1600.0});
    EXPECT_TRUE(std::isnan(fincore::mwr(non_conventional)));

    const auto all_negative =
        fincore::make_series({1, 2}, "cashflow", {-1000.0, -100.0});
    EXPECT_TRUE(std::isnan(fincore::mwr(all_negative)));

    const auto single = fincore::make_series({1}, "cashflow", {-1000.0});
    EXPECT_THROW(static_cast<void>(fincore::mwr(single)), fincore::input_error);

    const auto nonfinite = fincore::make_series(
        {1, 2}, "cashflow",
        {-1000.0, std::numeric_limits<double>::infinity()});
    EXPECT_THROW(static_cast<void>(fincore::mwr(nonfinite)),
                 fincore::input_error);

    const auto valid =
        fincore::make_series({1, 2}, "cashflow", {-1000.0, 1100.0});
    EXPECT_THROW(static_cast<void>(fincore::mwr(valid, 0.0)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::mwr(valid, -0.5)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::mwr(valid, nan)),
                 fincore::input_error);
}

TEST(PerformanceReturns, MwrSupportsSelectorsAndOwnsItsResult) {
    const auto named =
        fincore::make_series({1, 2}, "amount", {-1000.0, 1100.0});
    EXPECT_NEAR(fincore::mwr(named, std::nullopt, "amount"),
                0.10000000000000002, 1e-12);

    const std::array<char, 13> embedded_storage{
        'c', 'a', 's', 'h', 'f', 'l', 'o', 'w', '\0', 'x', '!', '!', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::mwr(named, std::nullopt,
                                                embedded_selector)),
                 fincore::input_error);

    const double owned = [] {
        const auto frame =
            fincore::make_series({1, 2}, "cashflow", {-1000.0, 1100.0});
        return fincore::mwr(frame);
    }();
    EXPECT_NEAR(owned, 0.10000000000000002, 1e-12);
}

}  // namespace
