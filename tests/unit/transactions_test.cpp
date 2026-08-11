#include <fincore/error.hpp>
#include <fincore/calendar.hpp>
#include <fincore/metrics/transactions.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

void expect_values(const std::vector<double>& actual,
                   const std::vector<double>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t position = 0U; position < expected.size(); ++position) {
        if (std::isnan(expected[position])) {
            EXPECT_TRUE(std::isnan(actual[position])) << "position " << position;
        } else if (std::isinf(expected[position])) {
            EXPECT_EQ(actual[position], expected[position])
                << "position " << position;
        } else {
            EXPECT_NEAR(actual[position], expected[position], 1.0e-12)
                << "position " << position;
        }
    }
}

void expect_double_columns(
    const fincore::dataframe& actual,
    const std::vector<std::string_view>& expected) {
    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected.size());
    EXPECT_EQ(actual.shape().second, expected.size());
    for (std::size_t position = 0U; position < expected.size(); ++position) {
        EXPECT_STREQ(std::get<0>(columns[position]).c_str(),
                     std::string{expected[position]}.c_str())
            << "position " << position;
    }
}

fincore::dataframe reference_transactions() {
    fincore::dataframe transactions;
    transactions.load_index(std::vector<fincore::timestamp>{
        fincore::utc_timestamp(2024, 1, 2, 15, 45),
        fincore::utc_timestamp(2024, 1, 5, 11),
        fincore::utc_timestamp(2024, 1, 2, 9, 30),
        fincore::utc_timestamp(2024, 1, 3, 10),
    });
    transactions.load_column<double>(
        "amount", std::vector<double>{-4.0, 3.0, 10.0, nan},
        hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<int>("metadata", std::vector<int>{4, 5, 6, 7},
                                  hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>(
        "price", std::vector<double>{10.0, infinity, 5.0, 7.0},
        hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("unrelated_malformed", std::vector<double>{42.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    return transactions;
}

fincore::dataframe empty_transactions() {
    fincore::dataframe transactions;
    transactions.load_index(std::vector<fincore::timestamp>{});
    transactions.load_column<double>("amount", std::vector<double>{},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", std::vector<double>{},
                                     hmdf::nan_policy::dont_pad_with_nans);
    return transactions;
}

fincore::dataframe full_volume_transactions() {
    fincore::dataframe transactions;
    transactions.load_index(std::vector<fincore::timestamp>{
        fincore::utc_timestamp(2024, 1, 3, 10),
        fincore::utc_timestamp(2024, 1, 4, 10),
        fincore::utc_timestamp(2024, 1, 2, 10),
    });
    transactions.load_column<double>("amount", std::vector<double>{10.0, 10.0, 10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", std::vector<double>{10.0, 10.0, 10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    return transactions;
}

fincore::dataframe reference_positions() {
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{
        fincore::utc_timestamp(2024, 1, 2, 17),
        fincore::utc_timestamp(2024, 1, 3, 11),
        fincore::utc_timestamp(2024, 1, 4, 7),
    });
    positions.load_column<double>("asset", std::vector<double>{100.0, -50.0, 20.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<int>("metadata", std::vector<int>{1, 2, 3},
                               hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", std::vector<double>{900.0, 1050.0, 980.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    return positions;
}

fincore::dataframe empty_positions() {
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{});
    positions.load_column<double>("asset", std::vector<double>{},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", std::vector<double>{},
                                  hmdf::nan_policy::dont_pad_with_nans);
    return positions;
}

std::vector<fincore::timestamp> expected_days() {
    return {fincore::utc_timestamp(2024, 1, 2),
            fincore::utc_timestamp(2024, 1, 3),
            fincore::utc_timestamp(2024, 1, 5)};
}

TEST(Transactions, AggregatesUnorderedIntradayRowsToSortedDailySchema) {
    const auto daily = fincore::get_txn_vol(reference_transactions());

    EXPECT_EQ(daily.get_index(), expected_days());
    expect_double_columns(daily, {"txn_volume", "txn_shares"});
    expect_values(daily.get_column<double>("txn_volume"), {90.0, 0.0, infinity});
    expect_values(daily.get_column<double>("txn_shares"), {14.0, 0.0, 3.0});
    EXPECT_FALSE(daily.has_column("metadata"));
}

TEST(Transactions, IgnoresMalformedUnrelatedDoubleTransactionColumns) {
    const auto transactions = reference_transactions();
    EXPECT_EQ(transactions.get_column<double>("unrelated_malformed").size(), 1U);
    EXPECT_NE(transactions.get_column<double>("unrelated_malformed").size(),
              transactions.get_index().size());

    const auto daily = fincore::get_txn_vol(transactions);
    expect_double_columns(daily, {"txn_volume", "txn_shares"});
    expect_values(daily.get_column<double>("txn_volume"), {90.0, 0.0, infinity});
    expect_values(daily.get_column<double>("txn_shares"), {14.0, 0.0, 3.0});

    const auto turnover = fincore::get_turnover(reference_positions(), transactions);
    expect_double_columns(turnover, {"turnover"});
    expect_values(turnover.get_column<double>("turnover"),
                  {1.8, 0.0, 0.0, 0.0});
}

TEST(Transactions, PreservesPriceSignAndSkipsOnlyNaNContributions) {
    fincore::dataframe transactions;
    transactions.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 6),
                                         fincore::utc_timestamp(2024, 1, 6),
                                         fincore::utc_timestamp(2024, 1, 6)});
    transactions.load_column<double>("amount", std::vector<double>{-2.0, nan, 0.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", std::vector<double>{-5.0, 7.0, infinity},
                                     hmdf::nan_policy::dont_pad_with_nans);

    const auto daily = fincore::get_txn_vol(transactions);
    EXPECT_EQ(daily.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 6)}));
    expect_values(daily.get_column<double>("txn_volume"), {-10.0});
    expect_values(daily.get_column<double>("txn_shares"), {2.0});
}

TEST(Transactions, ComputesActualGrossBookAndPortfolioValueTurnoverOnDateUnion) {
    const auto positions = reference_positions();
    const auto transactions = reference_transactions();
    const std::vector<fincore::timestamp> expected{
        fincore::utc_timestamp(2024, 1, 2),
        fincore::utc_timestamp(2024, 1, 3),
        fincore::utc_timestamp(2024, 1, 4),
        fincore::utc_timestamp(2024, 1, 5),
    };

    const auto actual_gross = fincore::get_turnover(positions, transactions);
    EXPECT_EQ(actual_gross.get_index(), expected);
    expect_double_columns(actual_gross, {"turnover"});
    expect_values(actual_gross.get_column<double>("turnover"),
                  {1.8, 0.0, 0.0, 0.0});

    const auto portfolio_value = fincore::get_turnover(
        positions, transactions, fincore::turnover_denominator::portfolio_value);
    EXPECT_EQ(portfolio_value.get_index(), expected);
    expect_double_columns(portfolio_value, {"turnover"});
    expect_values(portfolio_value.get_column<double>("turnover"),
                  {.09, 0.0, 0.0, 0.0});
}

TEST(Transactions, UsesPreviousAndCurrentGrossBookForLaterTurnoverDays) {
    const auto turnover =
        fincore::get_turnover(reference_positions(), full_volume_transactions());

    EXPECT_EQ(turnover.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2),
                                                fincore::utc_timestamp(2024, 1, 3),
                                                fincore::utc_timestamp(2024, 1, 4)}));
    expect_values(turnover.get_column<double>("turnover"),
                  {2.0, 100.0 / 75.0, 100.0 / 35.0});
}

TEST(Transactions, SupportsCustomAmountPriceAndCashNames) {
    fincore::dataframe transactions;
    transactions.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2, 13)});
    transactions.load_column<double>("quantity", std::vector<double>{2.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("execution_price", std::vector<double>{4.0},
                                     hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe positions;
    positions.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2, 8)});
    positions.load_column<double>("asset", std::vector<double>{10.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("reserve", std::vector<double>{90.0},
                                  hmdf::nan_policy::dont_pad_with_nans);

    const auto daily =
        fincore::get_txn_vol(transactions, "quantity", "execution_price");
    expect_values(daily.get_column<double>("txn_volume"), {8.0});
    expect_values(daily.get_column<double>("txn_shares"), {2.0});

    const auto turnover = fincore::get_turnover(
        positions, transactions, fincore::turnover_denominator::actual_gross_book,
        "reserve", "quantity", "execution_price");
    expect_values(turnover.get_column<double>("turnover"), {1.6});
}

TEST(Transactions, ProducesTypedEmptyDailyAndTurnoverFrames) {
    const auto daily = fincore::get_txn_vol(empty_transactions());
    EXPECT_TRUE(daily.get_index().empty());
    expect_double_columns(daily, {"txn_volume", "txn_shares"});

    const auto turnover = fincore::get_turnover(empty_positions(), empty_transactions());
    EXPECT_TRUE(turnover.get_index().empty());
    expect_double_columns(turnover, {"turnover"});
}

TEST(Transactions, SanitizesNonfiniteFinalTurnoverButNotDailyVolume) {
    fincore::dataframe positions;
    positions.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2)});
    positions.load_column<double>("asset", std::vector<double>{100.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", std::vector<double>{900.0},
                                  hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe transactions;
    transactions.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2, 10)});
    transactions.load_column<double>("amount", std::vector<double>{1.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", std::vector<double>{infinity},
                                     hmdf::nan_policy::dont_pad_with_nans);

    const auto daily = fincore::get_txn_vol(transactions);
    expect_values(daily.get_column<double>("txn_volume"), {infinity});
    const auto actual_gross = fincore::get_turnover(positions, transactions);
    const auto portfolio_value = fincore::get_turnover(
        positions, transactions, fincore::turnover_denominator::portfolio_value);
    expect_values(actual_gross.get_column<double>("turnover"), {0.0});
    expect_values(portfolio_value.get_column<double>("turnover"), {0.0});
}

TEST(Transactions, RejectsInvalidRequiredTransactionFields) {
    const auto valid = reference_transactions();
    EXPECT_THROW(fincore::get_txn_vol(valid, "", "price"), fincore::input_error);
    EXPECT_THROW(fincore::get_txn_vol(valid, "amount", ""), fincore::input_error);
    EXPECT_THROW(fincore::get_txn_vol(valid, "amount", "amount"),
                 fincore::input_error);

    fincore::dataframe missing_amount;
    missing_amount.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2)});
    missing_amount.load_column<double>("price", std::vector<double>{1.0},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_txn_vol(missing_amount), fincore::input_error);

    fincore::dataframe wrong_amount;
    wrong_amount.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2)});
    wrong_amount.load_column<int>("amount", std::vector<int>{1},
                                  hmdf::nan_policy::dont_pad_with_nans);
    wrong_amount.load_column<double>("price", std::vector<double>{1.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_txn_vol(wrong_amount), fincore::input_error);

    fincore::dataframe wrong_price;
    wrong_price.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2)});
    wrong_price.load_column<double>("amount", std::vector<double>{1.0},
                                    hmdf::nan_policy::dont_pad_with_nans);
    wrong_price.load_column<int>("price", std::vector<int>{1},
                                 hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_txn_vol(wrong_price), fincore::input_error);

    fincore::dataframe malformed_amount;
    malformed_amount.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2),
                                         fincore::utc_timestamp(2024, 1, 3)});
    malformed_amount.load_column<double>("amount", std::vector<double>{1.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    malformed_amount.load_column<double>("price", std::vector<double>{1.0, 2.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_txn_vol(malformed_amount), fincore::input_error);

    fincore::dataframe malformed_price;
    malformed_price.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2),
                                         fincore::utc_timestamp(2024, 1, 3)});
    malformed_price.load_column<double>("amount", std::vector<double>{1.0, 2.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    malformed_price.load_column<double>("price", std::vector<double>{1.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_txn_vol(malformed_price), fincore::input_error);
}

TEST(Transactions, EnforcesNumericPositionAndActualGrossBookCashContracts) {
    const auto transactions = empty_transactions();

    fincore::dataframe metadata_only;
    metadata_only.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2)});
    metadata_only.load_column<int>("metadata", std::vector<int>{1},
                                   hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_turnover(metadata_only, transactions),
                 fincore::input_error);

    fincore::dataframe cashless;
    cashless.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2)});
    cashless.load_column<double>("asset", std::vector<double>{10.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_turnover(cashless, transactions), fincore::input_error);

    fincore::dataframe cashless_transactions;
    cashless_transactions.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2, 10)});
    cashless_transactions.load_column<double>("amount", std::vector<double>{1.0},
                                              hmdf::nan_policy::dont_pad_with_nans);
    cashless_transactions.load_column<double>("price", std::vector<double>{5.0},
                                              hmdf::nan_policy::dont_pad_with_nans);
    const auto portfolio_value = fincore::get_turnover(
        cashless, cashless_transactions,
        fincore::turnover_denominator::portfolio_value);
    expect_values(portfolio_value.get_column<double>("turnover"), {.5});

    fincore::dataframe wrong_cash;
    wrong_cash.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2)});
    wrong_cash.load_column<double>("asset", std::vector<double>{10.0},
                                   hmdf::nan_policy::dont_pad_with_nans);
    wrong_cash.load_column<int>("cash", std::vector<int>{90},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_turnover(wrong_cash, cashless_transactions),
                 fincore::input_error);
    const auto wrong_cash_portfolio_value = fincore::get_turnover(
        wrong_cash, cashless_transactions,
        fincore::turnover_denominator::portfolio_value, "cash");
    expect_values(wrong_cash_portfolio_value.get_column<double>("turnover"), {.5});
    EXPECT_THROW(fincore::get_turnover(reference_positions(), transactions,
                                       fincore::turnover_denominator::actual_gross_book,
                                       ""),
                 fincore::input_error);

    fincore::dataframe malformed_cash;
    malformed_cash.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2),
                                         fincore::utc_timestamp(2024, 1, 3)});
    malformed_cash.load_column<double>("asset", std::vector<double>{1.0, 2.0},
                                       hmdf::nan_policy::dont_pad_with_nans);
    malformed_cash.load_column<double>("cash", std::vector<double>{9.0},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_turnover(malformed_cash, transactions),
                 fincore::input_error);
}

TEST(Transactions, RejectsMalformedAndNonascendingNormalizedPositionDays) {
    const auto transactions = empty_transactions();

    fincore::dataframe malformed_positions;
    malformed_positions.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2),
                                         fincore::utc_timestamp(2024, 1, 3)});
    malformed_positions.load_column<double>("asset", std::vector<double>{1.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    malformed_positions.load_column<double>("cash", std::vector<double>{9.0, 9.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_turnover(malformed_positions, transactions),
                 fincore::input_error);

    fincore::dataframe duplicate_days;
    duplicate_days.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2, 9),
                                         fincore::utc_timestamp(2024, 1, 2, 15)});
    duplicate_days.load_column<double>("asset", std::vector<double>{1.0, 2.0},
                                       hmdf::nan_policy::dont_pad_with_nans);
    duplicate_days.load_column<double>("cash", std::vector<double>{9.0, 8.0},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_turnover(duplicate_days, transactions),
                 fincore::input_error);

    fincore::dataframe out_of_order_days;
    out_of_order_days.load_index(
        std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 3, 9),
                                         fincore::utc_timestamp(2024, 1, 2, 15)});
    out_of_order_days.load_column<double>("asset", std::vector<double>{1.0, 2.0},
                                          hmdf::nan_policy::dont_pad_with_nans);
    out_of_order_days.load_column<double>("cash", std::vector<double>{9.0, 8.0},
                                          hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_turnover(out_of_order_days, transactions),
                 fincore::input_error);
}

TEST(Transactions, RejectsUnknownTurnoverDenominator) {
    EXPECT_THROW(fincore::get_turnover(
                     reference_positions(), empty_transactions(),
                     static_cast<fincore::turnover_denominator>(42)),
                 fincore::input_error);
}

TEST(Transactions, AdjustsReturnsForSlippageOnTheSortedDailyUnion) {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2024, 1, 2, 14),
         fincore::utc_timestamp(2024, 1, 3, 14),
         fincore::utc_timestamp(2024, 1, 5, 14),
         fincore::utc_timestamp(2024, 1, 6, 14)},
        "returns", {.1, .2, nan, .3});

    fincore::dataframe positions;
    positions.load_index(
        {fincore::utc_timestamp(2024, 1, 2, 16),
         fincore::utc_timestamp(2024, 1, 4, 16),
         fincore::utc_timestamp(2024, 1, 5, 16)});
    positions.load_column<double>("asset", {100.0, 200.0, 300.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", {900.0, 800.0, 700.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<int>("metadata", {1, 2, 3},
                               hmdf::nan_policy::dont_pad_with_nans);

    const auto transactions = full_volume_transactions();
    const auto adjusted = fincore::adjust_returns_for_slippage(
        returns, positions, transactions, 10.0);

    EXPECT_EQ(adjusted.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2),
                                                fincore::utc_timestamp(2024, 1, 3),
                                                fincore::utc_timestamp(2024, 1, 4),
                                                fincore::utc_timestamp(2024, 1, 5),
                                                fincore::utc_timestamp(2024, 1, 6)}));
    expect_double_columns(adjusted, {"returns"});
    expect_values(adjusted.get_column<double>("returns"), {.0999, 0.0, 0.0, 0.0,
                                                            0.0});
}

TEST(Transactions, AdjustsReturnsForSlippageWithCustomColumnNames) {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2024, 1, 2, 9)}, "strategy", {.1});
    fincore::dataframe positions;
    positions.load_index({fincore::utc_timestamp(2024, 1, 2, 16)});
    positions.load_column<double>("asset", {100.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("reserve", {900.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe transactions;
    transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 10)});
    transactions.load_column<double>("quantity", {10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("execution_price", {10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);

    const auto adjusted = fincore::adjust_returns_for_slippage(
        returns, positions, transactions, 10.0, "strategy", "quantity",
        "execution_price");

    expect_double_columns(adjusted, {"strategy"});
    expect_values(adjusted.get_column<double>("strategy"), {.0999});
}

TEST(Transactions, ProducesTypedEmptySlippageAdjustedReturns) {
    const auto adjusted = fincore::adjust_returns_for_slippage(
        fincore::make_series({}, "returns", {}), empty_positions(),
        empty_transactions(), 10.0);

    EXPECT_TRUE(adjusted.get_index().empty());
    expect_double_columns(adjusted, {"returns"});
}

TEST(Transactions, ProducesZeroSlippageAdjustedReturnsForPositionAndTransactionDays) {
    fincore::dataframe positions;
    positions.load_index({fincore::utc_timestamp(2024, 1, 2, 16),
                          fincore::utc_timestamp(2024, 1, 4, 16)});
    positions.load_column<double>("asset", {100.0, 100.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", {900.0, 900.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    const auto adjusted = fincore::adjust_returns_for_slippage(
        fincore::make_series({}, "returns", {}), positions,
        full_volume_transactions(), 10.0);

    EXPECT_EQ(adjusted.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2),
                                                fincore::utc_timestamp(2024, 1, 3),
                                                fincore::utc_timestamp(2024, 1, 4)}));
    expect_values(adjusted.get_column<double>("returns"), {0.0, 0.0, 0.0});
}

TEST(Transactions, HandlesSlippageArithmeticBoundaries) {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2024, 1, 2)}, "returns", {.1});
    fincore::dataframe full_value_positions;
    full_value_positions.load_index({fincore::utc_timestamp(2024, 1, 2, 16)});
    full_value_positions.load_column<double>("asset", {100.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    full_value_positions.load_column<double>("cash", {900.0},
                                             hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe zero_value_positions;
    zero_value_positions.load_index({fincore::utc_timestamp(2024, 1, 2, 16)});
    zero_value_positions.load_column<double>("asset", {0.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    zero_value_positions.load_column<double>("cash", {0.0},
                                             hmdf::nan_policy::dont_pad_with_nans);

    fincore::dataframe finite_transactions;
    finite_transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 10)});
    finite_transactions.load_column<double>("amount", {10.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    finite_transactions.load_column<double>("price", {10.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    expect_values(fincore::adjust_returns_for_slippage(
                      returns, zero_value_positions, finite_transactions, 10.0)
                      .get_column<double>("returns"),
                  {0.0});

    fincore::dataframe nan_amount_transactions;
    nan_amount_transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 10)});
    nan_amount_transactions.load_column<double>("amount", {nan},
                                                hmdf::nan_policy::dont_pad_with_nans);
    nan_amount_transactions.load_column<double>("price", {10.0},
                                                hmdf::nan_policy::dont_pad_with_nans);
    expect_values(fincore::adjust_returns_for_slippage(
                      returns, full_value_positions, nan_amount_transactions, 10.0)
                      .get_column<double>("returns"),
                  {.1});

    expect_values(fincore::adjust_returns_for_slippage(
                      returns, full_value_positions, finite_transactions, nan)
                      .get_column<double>("returns"),
                  {.1});

    fincore::dataframe infinite_volume_transactions;
    infinite_volume_transactions.load_index(
        {fincore::utc_timestamp(2024, 1, 2, 10)});
    infinite_volume_transactions.load_column<double>("amount", {1.0},
                                                     hmdf::nan_policy::dont_pad_with_nans);
    infinite_volume_transactions.load_column<double>("price", {infinity},
                                                     hmdf::nan_policy::dont_pad_with_nans);
    expect_values(fincore::adjust_returns_for_slippage(
                      returns, full_value_positions, infinite_volume_transactions, 10.0)
                      .get_column<double>("returns"),
                  {-infinity});
}

TEST(Transactions, RejectsInvalidSlippageAdjustedReturnInputs) {
    const auto valid_returns = fincore::make_series(
        {fincore::utc_timestamp(2024, 1, 2)}, "returns", {.1});
    fincore::dataframe valid_positions;
    valid_positions.load_index({fincore::utc_timestamp(2024, 1, 2, 16)});
    valid_positions.load_column<double>("asset", {100.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe valid_transactions;
    valid_transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 10)});
    valid_transactions.load_column<double>("amount", {1.0},
                                           hmdf::nan_policy::dont_pad_with_nans);
    valid_transactions.load_column<double>("price", {1.0},
                                           hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, valid_transactions, 10.0, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     fincore::make_series({fincore::utc_timestamp(2024, 1, 2)},
                                          "other", {.1}),
                     valid_positions, valid_transactions, 10.0),
                 fincore::input_error);
    fincore::dataframe wrong_type_returns;
    wrong_type_returns.load_index({fincore::utc_timestamp(2024, 1, 2)});
    wrong_type_returns.load_column<int>("returns", {1},
                                        hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     wrong_type_returns, valid_positions, valid_transactions, 10.0),
                 fincore::input_error);
    fincore::dataframe malformed_returns;
    malformed_returns.load_index({fincore::utc_timestamp(2024, 1, 2),
                                  fincore::utc_timestamp(2024, 1, 3)});
    malformed_returns.load_column<double>("returns", {.1},
                                            hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     malformed_returns, valid_positions, valid_transactions, 10.0),
                 fincore::input_error);

    fincore::dataframe metadata_only_positions;
    metadata_only_positions.load_index({fincore::utc_timestamp(2024, 1, 2)});
    metadata_only_positions.load_column<int>("metadata", {1},
                                             hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, metadata_only_positions, valid_transactions, 10.0),
                 fincore::input_error);
    fincore::dataframe malformed_positions;
    malformed_positions.load_index({fincore::utc_timestamp(2024, 1, 2),
                                    fincore::utc_timestamp(2024, 1, 3)});
    malformed_positions.load_column<double>("asset", {100.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, malformed_positions, valid_transactions, 10.0),
                 fincore::input_error);
    fincore::dataframe duplicate_position_days;
    duplicate_position_days.load_index({fincore::utc_timestamp(2024, 1, 2, 9),
                                        fincore::utc_timestamp(2024, 1, 2, 16)});
    duplicate_position_days.load_column<double>("asset", {100.0, 100.0},
                                                 hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, duplicate_position_days, valid_transactions, 10.0),
                 fincore::input_error);
    fincore::dataframe out_of_order_position_days;
    out_of_order_position_days.load_index({fincore::utc_timestamp(2024, 1, 3, 9),
                                           fincore::utc_timestamp(2024, 1, 2, 16)});
    out_of_order_position_days.load_column<double>("asset", {100.0, 100.0},
                                                    hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, out_of_order_position_days, valid_transactions, 10.0),
                 fincore::input_error);
    fincore::dataframe out_of_order_returns;
    out_of_order_returns.load_index({fincore::utc_timestamp(2024, 1, 3, 9),
                                    fincore::utc_timestamp(2024, 1, 2, 9)});
    out_of_order_returns.load_column<double>("returns", {.1, .1},
                                              hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     out_of_order_returns, valid_positions, valid_transactions, 10.0),
                 fincore::input_error);
    fincore::dataframe duplicate_return_days;
    duplicate_return_days.load_index({fincore::utc_timestamp(2024, 1, 2, 9),
                                      fincore::utc_timestamp(2024, 1, 2, 16)});
    duplicate_return_days.load_column<double>("returns", {.1, .1},
                                               hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     duplicate_return_days, valid_positions, valid_transactions, 10.0),
                 fincore::input_error);

    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, valid_transactions, 10.0, "returns",
                     "amount", "amount"),
                 fincore::input_error);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, valid_transactions, 10.0, "returns", "",
                     "price"),
                 fincore::input_error);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, valid_transactions, 10.0, "returns",
                     "amount", ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, valid_transactions, 10.0, "returns",
                     "missing", "price"),
                 fincore::input_error);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, valid_transactions, 10.0, "returns",
                     "amount", "missing"),
                 fincore::input_error);
    fincore::dataframe wrong_amount_transactions = valid_transactions;
    wrong_amount_transactions.load_column<int>("wrong_amount", {1},
                                               hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, wrong_amount_transactions, 10.0,
                     "returns", "wrong_amount", "price"),
                 fincore::input_error);
    fincore::dataframe wrong_price_transactions = valid_transactions;
    wrong_price_transactions.load_column<int>("wrong_price", {1},
                                              hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, wrong_price_transactions, 10.0,
                     "returns", "amount", "wrong_price"),
                 fincore::input_error);
    fincore::dataframe malformed_amount_transactions;
    malformed_amount_transactions.load_index(
        {fincore::utc_timestamp(2024, 1, 2, 10),
         fincore::utc_timestamp(2024, 1, 3, 10)});
    malformed_amount_transactions.load_column<double>("amount", {1.0},
                                                      hmdf::nan_policy::dont_pad_with_nans);
    malformed_amount_transactions.load_column<double>("price", {1.0, 1.0},
                                                      hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, malformed_amount_transactions, 10.0),
                 fincore::input_error);
    fincore::dataframe malformed_price_transactions;
    malformed_price_transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 10),
                                             fincore::utc_timestamp(2024, 1, 3, 10)});
    malformed_price_transactions.load_column<double>("amount", {1.0, 1.0},
                                                     hmdf::nan_policy::dont_pad_with_nans);
    malformed_price_transactions.load_column<double>("price", {1.0},
                                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::adjust_returns_for_slippage(
                     valid_returns, valid_positions, malformed_price_transactions, 10.0),
                 fincore::input_error);
}

}  // namespace
