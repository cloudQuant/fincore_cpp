#include <fincore/attribution/brinson.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using fincore::timestamp;

constexpr std::array<std::string_view, 3U> sectors{"tech", "health",
                                                    "finance"};

fincore::dataframe make_frame(std::vector<timestamp> index,
                              const std::vector<double>& tech,
                              const std::vector<double>& health,
                              const std::vector<double>& finance) {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>("tech", tech, hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("health", health,
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("finance", finance,
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

struct brinson_fixture {
    fincore::dataframe portfolio_returns;
    fincore::dataframe benchmark_returns;
    fincore::dataframe portfolio_weights;
    fincore::dataframe benchmark_weights;
};

brinson_fixture oracle_fixture(
    std::vector<timestamp> index = {1704153600000000000LL,
                                    1704240000000000000LL}) {
    const std::vector<double> empty_or_two =
        index.empty() ? std::vector<double>{} : std::vector<double>{.10, .03};
    if (index.empty()) {
        return {make_frame(index, {}, {}, {}), make_frame(index, {}, {}, {}),
                make_frame(index, {}, {}, {}), make_frame(index, {}, {}, {})};
    }
    return {make_frame(index, empty_or_two, {.05, -.01}, {-.02, .04}),
            make_frame(index, {.08, .02}, {.04, .00}, {-.01, .03}),
            make_frame(index, {.50, .45}, {.30, .35}, {.20, .20}),
            make_frame(index, {.40, .40}, {.40, .40}, {.20, .20})};
}

void expect_result_columns(const fincore::dataframe& result) {
    constexpr std::array<std::string_view, 6U> expected{
        fincore::brinson_allocation_column, fincore::brinson_selection_column,
        fincore::brinson_interaction_column, fincore::brinson_total_column,
        fincore::brinson_portfolio_return_column,
        fincore::brinson_benchmark_return_column};
    const auto columns = result.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected.size());
    for (std::size_t position = 0; position < expected.size(); ++position) {
        EXPECT_STREQ(std::get<0>(columns[position]).c_str(),
                     std::string{expected[position]}.c_str());
        EXPECT_EQ(std::get<2>(columns[position]), typeid(double));
    }
}

TEST(Brinson, MatchesPythonBhbOracleAndPublicSummaryContract) {
    auto fixture = oracle_fixture();
    fixture.portfolio_returns.load_column<int>(
        "metadata", {7, 8}, hmdf::nan_policy::dont_pad_with_nans);

    const auto result = fincore::brinson_results(
        fixture.portfolio_returns, fixture.benchmark_returns,
        fixture.portfolio_weights, fixture.benchmark_weights, sectors);

    expect_result_columns(result);
    EXPECT_EQ(result.get_index(), fixture.portfolio_returns.get_index());

    const auto& allocation = result.get_column<double>("allocation");
    const auto& selection = result.get_column<double>("selection");
    const auto& interaction = result.get_column<double>("interaction");
    const auto& total = result.get_column<double>("total");
    const auto& portfolio = result.get_column<double>("portfolio_return");
    const auto& benchmark = result.get_column<double>("benchmark_return");
    ASSERT_EQ(allocation.size(), 2U);
    EXPECT_NEAR(allocation[0], .004, 1.0e-12);
    EXPECT_NEAR(allocation[1], .001, 1.0e-12);
    EXPECT_NEAR(selection[0], .010, 1.0e-12);
    EXPECT_NEAR(selection[1], .002, 1.0e-12);
    EXPECT_NEAR(interaction[0], .001, 1.0e-12);
    EXPECT_NEAR(interaction[1], .001, 1.0e-12);
    EXPECT_NEAR(total[0], .015, 1.0e-12);
    EXPECT_NEAR(total[1], .004, 1.0e-12);
    EXPECT_NEAR(portfolio[0], .061, 1.0e-12);
    EXPECT_NEAR(portfolio[1], .018, 1.0e-12);
    EXPECT_NEAR(benchmark[0], .046, 1.0e-12);
    EXPECT_NEAR(benchmark[1], .014, 1.0e-12);
    for (std::size_t position = 0; position < total.size(); ++position) {
        EXPECT_NEAR(total[position], portfolio[position] - benchmark[position],
                    1.0e-12);
    }

    const auto period = fincore::brinson_attribution(
        make_frame({1704153600000000000LL}, {.10}, {.05}, {-.02}),
        make_frame({1704153600000000000LL}, {.08}, {.04}, {-.01}),
        make_frame({1704153600000000000LL}, {.50}, {.30}, {.20}),
        make_frame({1704153600000000000LL}, {.40}, {.40}, {.20}), sectors);
    EXPECT_NEAR(period.allocation, allocation[0], 1.0e-12);
    EXPECT_NEAR(period.selection, selection[0], 1.0e-12);
    EXPECT_NEAR(period.interaction, interaction[0], 1.0e-12);
    EXPECT_NEAR(period.total, total[0], 1.0e-12);
    EXPECT_NEAR(period.portfolio_return, portfolio[0], 1.0e-12);
    EXPECT_NEAR(period.benchmark_return, benchmark[0], 1.0e-12);

    const auto cumulative = fincore::brinson_cumulative(
        fixture.portfolio_returns, fixture.benchmark_returns,
        fixture.portfolio_weights, fixture.benchmark_weights, sectors);
    EXPECT_NEAR(cumulative.allocation, .005, 1.0e-12);
    EXPECT_NEAR(cumulative.selection, .012, 1.0e-12);
    EXPECT_NEAR(cumulative.interaction, .002, 1.0e-12);
    EXPECT_NEAR(cumulative.total, .019, 1.0e-12);
    EXPECT_NEAR(cumulative.portfolio_cumulative, .080098, 1.0e-12);
    EXPECT_NEAR(cumulative.benchmark_cumulative, .06064400000000014, 1.0e-12);
}

TEST(Brinson, ValidatesSelectorsIndexesAndSelectedColumns) {
    const auto fixture = oracle_fixture();
    constexpr std::array<std::string_view, 0U> none{};
    constexpr std::array<std::string_view, 1U> empty{""};
    constexpr std::array<std::string_view, 2U> duplicate{"tech", "tech"};
    constexpr std::array<std::string_view, 1U> absent{"absent"};
    EXPECT_THROW(fincore::brinson_results(fixture.portfolio_returns,
                                          fixture.benchmark_returns,
                                          fixture.portfolio_weights,
                                          fixture.benchmark_weights, none),
                 fincore::input_error);
    EXPECT_THROW(fincore::brinson_results(fixture.portfolio_returns,
                                          fixture.benchmark_returns,
                                          fixture.portfolio_weights,
                                          fixture.benchmark_weights, empty),
                 fincore::input_error);
    EXPECT_THROW(fincore::brinson_results(fixture.portfolio_returns,
                                          fixture.benchmark_returns,
                                          fixture.portfolio_weights,
                                          fixture.benchmark_weights, duplicate),
                 fincore::input_error);
    EXPECT_THROW(fincore::brinson_results(fixture.portfolio_returns,
                                          fixture.benchmark_returns,
                                          fixture.portfolio_weights,
                                          fixture.benchmark_weights, absent),
                 fincore::input_error);

    auto wrong_type = fixture.portfolio_returns;
    wrong_type.remove_column<double>("tech");
    wrong_type.load_column<int>("tech", {1, 2},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::brinson_results(wrong_type, fixture.benchmark_returns,
                                          fixture.portfolio_weights,
                                          fixture.benchmark_weights, sectors),
                 fincore::input_error);

    auto malformed = fixture.portfolio_returns;
    malformed.remove_column<double>("tech");
    malformed.load_column<double>("tech", {.10},
                                   hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::brinson_results(malformed, fixture.benchmark_returns,
                                          fixture.portfolio_weights,
                                          fixture.benchmark_weights, sectors),
                 fincore::input_error);

    auto duplicate_index = fixture.benchmark_returns;
    duplicate_index.load_index({1704153600000000000LL, 1704153600000000000LL});
    EXPECT_THROW(fincore::brinson_results(fixture.portfolio_returns,
                                          duplicate_index, fixture.portfolio_weights,
                                          fixture.benchmark_weights, sectors),
                 fincore::input_error);

    // The same two labels in swapped order: index matching compares sequences,
    // so a permutation is a rejection, not something the kernel re-sorts.
    auto mismatched_index = fixture.benchmark_weights;
    mismatched_index.load_index({1704240000000000000LL, 1704153600000000000LL});
    EXPECT_THROW(fincore::brinson_results(fixture.portfolio_returns,
                                          fixture.benchmark_returns,
                                          fixture.portfolio_weights,
                                          mismatched_index, sectors),
                 fincore::input_error);
}

TEST(Brinson, SupportsTypedEmptySchemasAndRejectsEmptySinglePeriod) {
    const auto fixture = oracle_fixture({});
    const auto result = fincore::brinson_results(
        fixture.portfolio_returns, fixture.benchmark_returns,
        fixture.portfolio_weights, fixture.benchmark_weights, sectors);
    EXPECT_TRUE(result.get_index().empty());
    expect_result_columns(result);
    for (const auto name : std::array<std::string_view, 6U>{
             "allocation", "selection", "interaction", "total",
             "portfolio_return", "benchmark_return"}) {
        EXPECT_TRUE(result.get_column<double>(std::string{name}.c_str()).empty());
    }
    const auto cumulative = fincore::brinson_cumulative(
        fixture.portfolio_returns, fixture.benchmark_returns,
        fixture.portfolio_weights, fixture.benchmark_weights, sectors);
    EXPECT_DOUBLE_EQ(cumulative.allocation, .0);
    EXPECT_DOUBLE_EQ(cumulative.selection, .0);
    EXPECT_DOUBLE_EQ(cumulative.interaction, .0);
    EXPECT_DOUBLE_EQ(cumulative.total, .0);
    EXPECT_DOUBLE_EQ(cumulative.portfolio_cumulative, .0);
    EXPECT_DOUBLE_EQ(cumulative.benchmark_cumulative, .0);
    EXPECT_THROW(fincore::brinson_attribution(
                     fixture.portfolio_returns, fixture.benchmark_returns,
                     fixture.portfolio_weights, fixture.benchmark_weights, sectors),
                 fincore::input_error);
}

TEST(Brinson, PreservesUnsortedOrderAndDirectNonUnitWeightArithmetic) {
    const auto unsorted = oracle_fixture({1704240000000000000LL,
                                          1704153600000000000LL});
    const auto unsorted_result = fincore::brinson_results(
        unsorted.portfolio_returns, unsorted.benchmark_returns,
        unsorted.portfolio_weights, unsorted.benchmark_weights, sectors);
    EXPECT_EQ(unsorted_result.get_index(),
              (std::vector<timestamp>{1704240000000000000LL,
                                      1704153600000000000LL}));
    EXPECT_NEAR(unsorted_result.get_column<double>("total")[0], .015, 1.0e-12);

    constexpr std::array<std::string_view, 2U> direct_sectors{"tech", "health"};
    const auto direct = fincore::brinson_results(
        make_frame({1}, {.10}, {-.20}, {.0}),
        make_frame({1}, {.05}, {.04}, {.0}),
        make_frame({1}, {1.5}, {-.5}, {.0}),
        make_frame({1}, {.25}, {1.25}, {.0}), direct_sectors);
    EXPECT_NEAR(direct.get_column<double>("allocation")[0], -.0075, 1.0e-12);
    EXPECT_NEAR(direct.get_column<double>("selection")[0], -.2875, 1.0e-12);
    EXPECT_NEAR(direct.get_column<double>("interaction")[0], .4825, 1.0e-12);
    EXPECT_NEAR(direct.get_column<double>("total")[0], .1875, 1.0e-12);
    EXPECT_NEAR(direct.get_column<double>("portfolio_return")[0], .25, 1.0e-12);
    EXPECT_NEAR(direct.get_column<double>("benchmark_return")[0], .0625, 1.0e-12);
}

TEST(Brinson, PreservesNanAndSignedInfinityPropagation) {
    constexpr std::array<std::string_view, 1U> one_sector{"tech"};
    const auto nan_result = fincore::brinson_results(
        make_frame({1}, {std::numeric_limits<double>::quiet_NaN()}, {.0}, {.0}),
        make_frame({1}, {.01}, {.0}, {.0}),
        make_frame({1}, {1.0}, {.0}, {.0}),
        make_frame({1}, {1.0}, {.0}, {.0}), one_sector);
    EXPECT_TRUE(std::isnan(nan_result.get_column<double>("selection")[0]));
    EXPECT_TRUE(std::isnan(nan_result.get_column<double>("total")[0]));
    EXPECT_TRUE(std::isnan(nan_result.get_column<double>("portfolio_return")[0]));

    // A zero benchmark weight makes selection the undefined 0 * (-inf), hence
    // NaN, and total inherits it, so only the unit-weighted terms are asserted.
    const auto infinity_result = fincore::brinson_results(
        make_frame({1}, {-std::numeric_limits<double>::infinity()}, {.0}, {.0}),
        make_frame({1}, {.01}, {.0}, {.0}),
        make_frame({1}, {1.0}, {.0}, {.0}),
        make_frame({1}, {.0}, {.0}, {.0}), one_sector);
    EXPECT_TRUE(std::isinf(infinity_result.get_column<double>("interaction")[0]));
    EXPECT_LT(infinity_result.get_column<double>("interaction")[0], .0);
    EXPECT_TRUE(std::isinf(infinity_result.get_column<double>("portfolio_return")[0]));
    EXPECT_LT(infinity_result.get_column<double>("portfolio_return")[0], .0);
}

}  // namespace
