#include <fincore/error.hpp>
#include <fincore/calendar.hpp>
#include <fincore/metrics/transactions.hpp>

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

fincore::dataframe slippage_volume_transactions() {
    fincore::dataframe transactions;
    transactions.load_index(std::vector<fincore::timestamp>{
        fincore::utc_timestamp(2024, 1, 5, 10),
        fincore::utc_timestamp(2024, 1, 2, 10),
        fincore::utc_timestamp(2024, 1, 4, 10),
        fincore::utc_timestamp(2024, 1, 3, 10),
    });
    transactions.load_column<double>("amount", std::vector<double>{10.0, 10.0, 10.0,
                                                                     10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", std::vector<double>{10.0, 10.0, 10.0,
                                                                    10.0},
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

    const auto transactions = slippage_volume_transactions();
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

TEST(Transactions, AggregatesDuplicateIntradayTransactionsForSlippage) {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2024, 1, 2, 9)}, "returns", {.1});
    fincore::dataframe positions;
    positions.load_index({fincore::utc_timestamp(2024, 1, 2, 16)});
    positions.load_column<double>("asset", {100.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", {900.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe transactions;
    transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 15),
                             fincore::utc_timestamp(2024, 1, 2, 9)});
    transactions.load_column<double>("amount", {5.0, 10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", {10.0, 5.0},
                                     hmdf::nan_policy::dont_pad_with_nans);

    const auto adjusted = fincore::adjust_returns_for_slippage(
        returns, positions, transactions, 10.0);

    expect_values(adjusted.get_column<double>("returns"), {.0999});
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

TEST(Transactions, AppliesQuadraticVolumeSharePenaltyGoldens) {
    const auto returns = fincore::make_series({fincore::utc_timestamp(2024, 1, 1, 9),
                                               fincore::utc_timestamp(2024, 1, 2, 9),
                                               fincore::utc_timestamp(2024, 1, 3, 9)},
                                              "returns", {1.0, 1.0, 1.0});
    fincore::dataframe transactions;
    transactions.load_index({fincore::utc_timestamp(2024, 1, 1, 15),
                             fincore::utc_timestamp(2024, 1, 2, 15),
                             fincore::utc_timestamp(2024, 1, 3, 15)});
    transactions.load_column<double>("amount", {100000.0, 100000.0, 100000.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", {1.0, 1.0, 1.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("volume", {1000000.0, 2000000.0, 3000000.0},
                                     hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_EQ(fincore::transaction_bar_volume_column, "volume");
    const auto equal_capitals = fincore::apply_slippage_penalty(
        returns, transactions, 1000000.0, 1000000.0, 1.0);
    EXPECT_EQ(equal_capitals.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 1),
                                               fincore::utc_timestamp(2024, 1, 2),
                                               fincore::utc_timestamp(2024, 1, 3)}));
    expect_double_columns(equal_capitals, {"returns"});
    expect_values(equal_capitals.get_column<double>("returns"),
                  {.9995, .9999375, .9999861111111111});

    expect_values(fincore::apply_slippage_penalty(returns, transactions, 10000000.0,
                                                  1000000.0, 1.0)
                      .get_column<double>("returns"),
                  {.95, .99375, .9986111111111111});
    expect_values(
        fincore::apply_slippage_penalty(returns, transactions, 1000000.0, 1000000.0)
            .get_column<double>("returns"),
        {.99995, .99999375, .9999986111111111});
}

TEST(Transactions, UsesAbsoluteSignedSimulatedSharesForQuadraticPenalty) {
    const auto returns =
        fincore::make_series({fincore::utc_timestamp(2024, 1, 2)}, "returns", {.1});

    fincore::dataframe negative_amount;
    negative_amount.load_index({fincore::utc_timestamp(2024, 1, 2, 15)});
    negative_amount.load_column<double>("amount", {-10.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    negative_amount.load_column<double>("price", {10.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    negative_amount.load_column<double>("volume", {100.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    expect_values(
        fincore::apply_slippage_penalty(returns, negative_amount, 1000.0, 1000.0,
                                        .1)
            .get_column<double>("returns"),
        {.09990909090909092});

    fincore::dataframe negative_simulation;
    negative_simulation.load_index({fincore::utc_timestamp(2024, 1, 2, 15)});
    negative_simulation.load_column<double>("amount", {10.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    negative_simulation.load_column<double>("price", {10.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    negative_simulation.load_column<double>("volume", {100.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    expect_values(
        fincore::apply_slippage_penalty(returns, negative_simulation, -1000.0,
                                        1000.0, .1)
            .get_column<double>("returns"),
        {.10009090909090909});
}

TEST(Transactions, AggregatesUnorderedDuplicateIntradayQuadraticPenalties) {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2024, 1, 2, 8), fincore::utc_timestamp(2024, 1, 3, 8),
         fincore::utc_timestamp(2024, 1, 4, 8), fincore::utc_timestamp(2024, 1, 5, 8),
         fincore::utc_timestamp(2024, 1, 6, 8)},
        "returns", {.1, .2, nan, -1.0, .3});
    fincore::dataframe transactions;
    transactions.load_index({fincore::utc_timestamp(2024, 1, 6, 15),
                             fincore::utc_timestamp(2024, 1, 2, 15),
                             fincore::utc_timestamp(2024, 1, 3, 12),
                             fincore::utc_timestamp(2024, 1, 2, 9)});
    transactions.load_column<double>("amount", {10.0, 10.0, 10.0, 10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", {10.0, 10.0, 10.0, 10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("volume", {100.0, 100.0, 200.0, 100.0},
                                     hmdf::nan_policy::dont_pad_with_nans);

    const auto adjusted =
        fincore::apply_slippage_penalty(returns, transactions, 1000.0, 1000.0, .1);

    EXPECT_EQ(adjusted.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2),
                                               fincore::utc_timestamp(2024, 1, 3),
                                               fincore::utc_timestamp(2024, 1, 4),
                                               fincore::utc_timestamp(2024, 1, 5),
                                               fincore::utc_timestamp(2024, 1, 6)}));
    expect_values(adjusted.get_column<double>("returns"),
                  {.09981818181818182, .1999810606060606, nan, -1.0, .3});
}

TEST(Transactions, SupportsCustomQuadraticPenaltyColumnNamesAndTypedEmptyOutput) {
    const auto custom_returns =
        fincore::make_series({fincore::utc_timestamp(2024, 1, 2, 9)}, "strategy", {.1});
    fincore::dataframe custom_transactions;
    custom_transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 15)});
    custom_transactions.load_column<double>("quantity", {10.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    custom_transactions.load_column<double>("execution_price", {10.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    custom_transactions.load_column<double>("bar_liquidity", {100.0},
                                            hmdf::nan_policy::dont_pad_with_nans);
    custom_transactions.load_column<int>("metadata", {42},
                                         hmdf::nan_policy::dont_pad_with_nans);

    const auto adjusted = fincore::apply_slippage_penalty(
        custom_returns, custom_transactions, 1000.0, 1000.0, .1, "strategy", "quantity",
        "execution_price", "bar_liquidity");
    expect_double_columns(adjusted, {"strategy"});
    expect_values(adjusted.get_column<double>("strategy"), {.09990909090909091});

    fincore::dataframe empty_bar_data;
    empty_bar_data.load_index({});
    empty_bar_data.load_column<double>("amount", {},
                                       hmdf::nan_policy::dont_pad_with_nans);
    empty_bar_data.load_column<double>("price", {},
                                       hmdf::nan_policy::dont_pad_with_nans);
    empty_bar_data.load_column<double>("volume", {},
                                       hmdf::nan_policy::dont_pad_with_nans);
    const auto empty = fincore::apply_slippage_penalty(
        fincore::make_series({}, "returns", {}), empty_bar_data, 1000.0, 1000.0);
    EXPECT_TRUE(empty.get_index().empty());
    expect_double_columns(empty, {"returns"});
}

TEST(Transactions, EmitsOnlyReturnDaysAndPreservesNanPenaltyBoundaries) {
    const auto returns =
        fincore::make_series({fincore::utc_timestamp(2024, 1, 2, 9)}, "returns", {.1});
    fincore::dataframe outside_transactions;
    outside_transactions.load_index({fincore::utc_timestamp(2024, 1, 1, 15),
                                     fincore::utc_timestamp(2024, 1, 3, 15)});
    outside_transactions.load_column<double>("amount", {10.0, 10.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    outside_transactions.load_column<double>("price", {10.0, 10.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    outside_transactions.load_column<double>("volume", {100.0, 100.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    const auto outside = fincore::apply_slippage_penalty(returns, outside_transactions,
                                                         1000.0, 1000.0, .1);
    EXPECT_EQ(outside.get_index(),
              (std::vector<fincore::timestamp>{fincore::utc_timestamp(2024, 1, 2)}));
    expect_values(outside.get_column<double>("returns"), {.1});

    fincore::dataframe nan_transactions;
    nan_transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 9),
                                 fincore::utc_timestamp(2024, 1, 2, 12),
                                 fincore::utc_timestamp(2024, 1, 2, 15)});
    nan_transactions.load_column<double>("amount", {nan, 10.0, 10.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    nan_transactions.load_column<double>("price", {10.0, nan, 10.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    nan_transactions.load_column<double>("volume", {100.0, 100.0, nan},
                                         hmdf::nan_policy::dont_pad_with_nans);
    expect_values(
        fincore::apply_slippage_penalty(returns, nan_transactions, 1000.0, 1000.0, .1)
            .get_column<double>("returns"),
        {.1});

    fincore::dataframe matching_transaction;
    matching_transaction.load_index({fincore::utc_timestamp(2024, 1, 2, 15)});
    matching_transaction.load_column<double>("amount", {10.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    matching_transaction.load_column<double>("price", {10.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    matching_transaction.load_column<double>("volume", {100.0},
                                             hmdf::nan_policy::dont_pad_with_nans);
    expect_values(fincore::apply_slippage_penalty(returns, matching_transaction, 1000.0,
                                                  1000.0, nan)
                      .get_column<double>("returns"),
                  {.1});
    expect_values(fincore::apply_slippage_penalty(returns, matching_transaction, nan,
                                                  1000.0, .1)
                      .get_column<double>("returns"),
                  {.1});
    expect_values(fincore::apply_slippage_penalty(returns, matching_transaction,
                                                  infinity, 1000.0, .1)
                      .get_column<double>("returns"),
                  {.1});
    expect_values(fincore::apply_slippage_penalty(returns, matching_transaction, 1000.0,
                                                  1000.0, infinity)
                      .get_column<double>("returns"),
                  {-infinity});
}

TEST(Transactions, PreservesOriginalReturnAtZeroPortfolioValueAndComputedInfinity) {
    const auto total_loss =
        fincore::make_series({fincore::utc_timestamp(2024, 1, 2)}, "returns", {-1.0});
    fincore::dataframe transactions;
    transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 10)});
    transactions.load_column<double>("amount", {10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("price", {10.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>("volume", {100.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    expect_values(
        fincore::apply_slippage_penalty(total_loss, transactions, 1000.0, 1000.0, .1)
            .get_column<double>("returns"),
        {-1.0});

    transactions.load_column<double>("volume", {0.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    expect_values(
        fincore::apply_slippage_penalty(
            fincore::make_series({fincore::utc_timestamp(2024, 1, 2)}, "returns", {.1}),
            transactions, 1000.0, 1000.0, .1)
            .get_column<double>("returns"),
        {-infinity});

    fincore::dataframe nan_daily_penalty;
    nan_daily_penalty.load_index({fincore::utc_timestamp(2024, 1, 2, 9),
                                  fincore::utc_timestamp(2024, 1, 2, 15)});
    nan_daily_penalty.load_column<double>("amount", {10.0, 10.0},
                                          hmdf::nan_policy::dont_pad_with_nans);
    nan_daily_penalty.load_column<double>("price", {10.0, -10.0},
                                          hmdf::nan_policy::dont_pad_with_nans);
    nan_daily_penalty.load_column<double>("volume", {0.0, 0.0},
                                          hmdf::nan_policy::dont_pad_with_nans);
    expect_values(
        fincore::apply_slippage_penalty(
            fincore::make_series({fincore::utc_timestamp(2024, 1, 2)}, "returns", {.1}),
            nan_daily_penalty, 1000.0, 1000.0, .1)
            .get_column<double>("returns"),
        {.1});
}

TEST(Transactions, RejectsInvalidQuadraticPenaltyReturnInputsAndCapital) {
    const auto valid_returns =
        fincore::make_series({fincore::utc_timestamp(2024, 1, 2)}, "returns", {.1});
    fincore::dataframe valid_transactions;
    valid_transactions.load_index({fincore::utc_timestamp(2024, 1, 2, 10)});
    valid_transactions.load_column<double>("amount", {1.0},
                                           hmdf::nan_policy::dont_pad_with_nans);
    valid_transactions.load_column<double>("price", {1.0},
                                           hmdf::nan_policy::dont_pad_with_nans);
    valid_transactions.load_column<double>("volume", {10.0},
                                           hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(
        fincore::apply_slippage_penalty(valid_returns, valid_transactions, 1000.0, 0.0),
        fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(valid_returns, valid_transactions,
                                                 1000.0, -0.0),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(valid_returns, valid_transactions,
                                                 1000.0, 1000.0, .1, ""),
                 fincore::input_error);
    EXPECT_THROW(
        fincore::apply_slippage_penalty(
            fincore::make_series({fincore::utc_timestamp(2024, 1, 2)}, "other", {.1}),
            valid_transactions, 1000.0, 1000.0),
        fincore::input_error);

    fincore::dataframe wrong_returns;
    wrong_returns.load_index({fincore::utc_timestamp(2024, 1, 2)});
    wrong_returns.load_column<int>("returns", {1},
                                   hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::apply_slippage_penalty(wrong_returns, valid_transactions,
                                                 1000.0, 1000.0),
                 fincore::input_error);
    fincore::dataframe malformed_returns;
    malformed_returns.load_index(
        {fincore::utc_timestamp(2024, 1, 2), fincore::utc_timestamp(2024, 1, 3)});
    malformed_returns.load_column<double>("returns", {.1},
                                          hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::apply_slippage_penalty(malformed_returns, valid_transactions,
                                                 1000.0, 1000.0),
                 fincore::input_error);
    fincore::dataframe duplicate_return_days;
    duplicate_return_days.load_index({fincore::utc_timestamp(2024, 1, 2, 9),
                                      fincore::utc_timestamp(2024, 1, 2, 15)});
    duplicate_return_days.load_column<double>("returns", {.1, .2},
                                              hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::apply_slippage_penalty(duplicate_return_days,
                                                 valid_transactions, 1000.0, 1000.0),
                 fincore::input_error);
    fincore::dataframe out_of_order_returns;
    out_of_order_returns.load_index({fincore::utc_timestamp(2024, 1, 3, 9),
                                     fincore::utc_timestamp(2024, 1, 2, 15)});
    out_of_order_returns.load_column<double>("returns", {.1, .2},
                                             hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::apply_slippage_penalty(out_of_order_returns,
                                                 valid_transactions, 1000.0, 1000.0),
                 fincore::input_error);
}

TEST(Transactions, RejectsInvalidQuadraticPenaltyTransactionFields) {
    const auto returns =
        fincore::make_series({fincore::utc_timestamp(2024, 1, 2)}, "returns", {.1});
    fincore::dataframe valid;
    valid.load_index({fincore::utc_timestamp(2024, 1, 2, 10)});
    valid.load_column<double>("amount", {1.0}, hmdf::nan_policy::dont_pad_with_nans);
    valid.load_column<double>("price", {1.0}, hmdf::nan_policy::dont_pad_with_nans);
    valid.load_column<double>("volume", {10.0}, hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "", "price", "volume"),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "", "volume"),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "price", ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "amount",
                                                 "volume"),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "price",
                                                 "amount"),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "price", "price"),
                 fincore::input_error);

    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "missing", "price",
                                                 "volume"),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "missing",
                                                 "volume"),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, valid, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "price",
                                                 "missing"),
                 fincore::input_error);

    fincore::dataframe wrong = valid;
    wrong.load_column<int>("wrong", {1}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, wrong, 1000.0, 1000.0, .1,
                                                 "returns", "wrong", "price", "volume"),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, wrong, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "wrong",
                                                 "volume"),
                 fincore::input_error);
    EXPECT_THROW(fincore::apply_slippage_penalty(returns, wrong, 1000.0, 1000.0, .1,
                                                 "returns", "amount", "price", "wrong"),
                 fincore::input_error);

    fincore::dataframe malformed_amount;
    malformed_amount.load_index(
        {fincore::utc_timestamp(2024, 1, 2), fincore::utc_timestamp(2024, 1, 3)});
    malformed_amount.load_column<double>("amount", {1.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    malformed_amount.load_column<double>("price", {1.0, 1.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    malformed_amount.load_column<double>("volume", {10.0, 10.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        fincore::apply_slippage_penalty(returns, malformed_amount, 1000.0, 1000.0),
        fincore::input_error);
    fincore::dataframe malformed_price;
    malformed_price.load_index(
        {fincore::utc_timestamp(2024, 1, 2), fincore::utc_timestamp(2024, 1, 3)});
    malformed_price.load_column<double>("amount", {1.0, 1.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    malformed_price.load_column<double>("price", {1.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    malformed_price.load_column<double>("volume", {10.0, 10.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        fincore::apply_slippage_penalty(returns, malformed_price, 1000.0, 1000.0),
        fincore::input_error);
    fincore::dataframe malformed_volume;
    malformed_volume.load_index(
        {fincore::utc_timestamp(2024, 1, 2), fincore::utc_timestamp(2024, 1, 3)});
    malformed_volume.load_column<double>("amount", {1.0, 1.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    malformed_volume.load_column<double>("price", {1.0, 1.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    malformed_volume.load_column<double>("volume", {10.0},
                                         hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        fincore::apply_slippage_penalty(returns, malformed_volume, 1000.0, 1000.0),
        fincore::input_error);
}

std::vector<fincore::timestamp> liquidity_index() {
    return {fincore::utc_timestamp(2021, 1, 1),
            fincore::utc_timestamp(2021, 1, 2),
            fincore::utc_timestamp(2021, 1, 3),
            fincore::utc_timestamp(2021, 1, 4),
            fincore::utc_timestamp(2021, 1, 5),
            fincore::utc_timestamp(2021, 1, 6),
            fincore::utc_timestamp(2021, 1, 7)};
}

fincore::dataframe wide_frame(const std::vector<fincore::timestamp>& index,
                              const std::string& first_name,
                              std::vector<double> first,
                              const std::string& second_name,
                              std::vector<double> second) {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>(index));
    frame.load_column<double>(first_name.c_str(), std::move(first),
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(second_name.c_str(), std::move(second),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(Transactions, DaysToLiquidateMatchesPythonOracle) {
    auto positions = wide_frame(
        liquidity_index(), "AAA",
        {100.0, 120.0, 90.0, 150.0, 160.0, 140.0, 170.0}, "BBB",
        {50.0, 40.0, 60.0, 55.0, 70.0, 80.0, 75.0});
    positions.load_column<double>(
        "cash", std::vector<double>(7, 1000.0),
        hmdf::nan_policy::dont_pad_with_nans);
    const auto prices = wide_frame(
        liquidity_index(), "AAA",
        {10.0, 11.0, 12.0, 11.5, 13.0, 12.5, 13.5}, "BBB",
        {20.0, 19.0, 21.0, 22.0, 20.0, 23.0, 22.0});
    const auto volumes = wide_frame(
        liquidity_index(), "AAA",
        {1000.0, 1200.0, 900.0, 1100.0, 1300.0, 1000.0, 1500.0}, "BBB",
        {500.0, 600.0, 550.0, 700.0, 650.0, 800.0, 750.0});

    const auto days = fincore::days_to_liquidate_positions(
        positions, prices, volumes, 0.2, 1'000'000.0, 2U);
    EXPECT_EQ(days.get_index(),
              (std::vector<fincore::timestamp>{
                  fincore::utc_timestamp(2021, 1, 3),
                  fincore::utc_timestamp(2021, 1, 4),
                  fincore::utc_timestamp(2021, 1, 5),
                  fincore::utc_timestamp(2021, 1, 6),
                  fincore::utc_timestamp(2021, 1, 7)}));
    const auto& aaa = days.get_column<double>("AAA");
    EXPECT_NEAR(aaa[0], 33.733133433283356, 1e-12);
    EXPECT_NEAR(aaa[1], 51.86721991701245, 1e-12);
    EXPECT_NEAR(aaa[2], 55.47177006951307, 1e-12);
    EXPECT_NEAR(aaa[3], 38.833874233724444, 1e-12);
    EXPECT_NEAR(aaa[4], 46.44428052345437, 1e-12);
    const auto& bbb = days.get_column<double>("BBB");
    EXPECT_NEAR(bbb[0], 24.380333197887037, 1e-12);
    EXPECT_NEAR(bbb[1], 19.88808432547754, 1e-12);
    EXPECT_NEAR(bbb[2], 21.117094287825992, 1e-12);
    EXPECT_NEAR(bbb[3], 23.089355806972986, 1e-12);
    EXPECT_NEAR(bbb[4], 19.185020336121557, 1e-12);
    EXPECT_FALSE(days.has_column("cash"));

    const auto maximums = fincore::get_max_days_to_liquidate_by_ticker(
        positions, prices, volumes, 0.2, 1'000'000.0, 2U);
    ASSERT_EQ(maximums.size(), 2U);
    EXPECT_EQ(maximums[0].symbol, "AAA");
    EXPECT_NEAR(maximums[0].days_to_liquidate, 55.47177006951307, 1e-12);
    EXPECT_NEAR(maximums[0].position_allocation_pct,
                13.008130081300814, 1e-12);
    EXPECT_EQ(maximums[1].symbol, "BBB");
    EXPECT_NEAR(maximums[1].days_to_liquidate, 24.380333197887037, 1e-12);
    EXPECT_NEAR(maximums[1].position_allocation_pct, 5.217391304347826,
                1e-12);

    const auto recent = fincore::get_max_days_to_liquidate_by_ticker(
        positions, prices, volumes, 0.2, 1'000'000.0, 2U, 2U);
    ASSERT_EQ(recent.size(), 2U);
    EXPECT_NEAR(recent[0].days_to_liquidate, 55.47177006951307, 1e-12);
    EXPECT_NEAR(recent[0].position_allocation_pct, 13.008130081300814,
                1e-12);
    EXPECT_NEAR(recent[1].days_to_liquidate, 23.089355806972986, 1e-12);
    EXPECT_NEAR(recent[1].position_allocation_pct, 6.557377049180328,
                1e-12);
}

TEST(Transactions, LowLiquidityTransactionsMatchesPythonOracle) {
    const auto prices = wide_frame(
        liquidity_index(), "AAA",
        {10.0, 11.0, 12.0, 11.5, 13.0, 12.5, 13.5}, "BBB",
        {20.0, 19.0, 21.0, 22.0, 20.0, 23.0, 22.0});
    const auto volumes = wide_frame(
        liquidity_index(), "AAA",
        {1000.0, 1200.0, 900.0, 1100.0, 1300.0, 1000.0, 1500.0}, "BBB",
        {500.0, 600.0, 550.0, 700.0, 650.0, 800.0, 750.0});
    const std::vector<fincore::trade_record> trades{
        {fincore::utc_timestamp(2021, 1, 2), "AAA", 10.0},
        {fincore::utc_timestamp(2021, 1, 2), "AAA", -30.0},
        {fincore::utc_timestamp(2021, 1, 3), "BBB", 40.0},
        {fincore::utc_timestamp(2021, 1, 5), "AAA", 20.0},
        {fincore::utc_timestamp(2021, 1, 5), "BBB", -15.0}};

    const auto low = fincore::get_low_liquidity_transactions(
        trades, prices, volumes);
    ASSERT_EQ(low.size(), 2U);
    EXPECT_EQ(low[0].symbol, "AAA");
    EXPECT_EQ(low[0].date, fincore::utc_timestamp(2021, 1, 2));
    EXPECT_NEAR(low[0].max_percent_bar_consumed, 3.3333333333333335,
                1e-12);
    EXPECT_EQ(low[1].symbol, "BBB");
    EXPECT_EQ(low[1].date, fincore::utc_timestamp(2021, 1, 3));
    EXPECT_NEAR(low[1].max_percent_bar_consumed, 7.2727272727272725,
                1e-12);

    const auto recent = fincore::get_low_liquidity_transactions(
        trades, prices, volumes, 2U);
    ASSERT_EQ(recent.size(), 2U);
    EXPECT_EQ(recent[0].date, fincore::utc_timestamp(2021, 1, 5));
    EXPECT_NEAR(recent[0].max_percent_bar_consumed, 1.5384615384615385,
                1e-12);
    EXPECT_NEAR(recent[1].max_percent_bar_consumed, 2.307692307692308,
                1e-12);
}

TEST(Transactions, LiquiditiyHelpersValidateInputs) {
    auto positions = wide_frame(liquidity_index(), "AAA",
                                std::vector<double>(7, 100.0), "BBB",
                                std::vector<double>(7, 50.0));
    positions.load_column<double>("cash", std::vector<double>(7, 1000.0),
                                  hmdf::nan_policy::dont_pad_with_nans);
    auto prices = wide_frame(liquidity_index(), "AAA",
                             std::vector<double>(7, 10.0), "BBB",
                             std::vector<double>(7, 20.0));
    auto volumes = wide_frame(liquidity_index(), "AAA",
                              std::vector<double>(7, 1000.0), "BBB",
                              std::vector<double>(7, 500.0));

    EXPECT_THROW(static_cast<void>(fincore::days_to_liquidate_positions(
                     positions, prices, volumes, 0.0)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::days_to_liquidate_positions(
                     positions, prices, volumes, 0.2, 1e6, 0U)),
                 fincore::input_error);

    auto shifted_prices = wide_frame(
        {fincore::utc_timestamp(2021, 1, 2),
         fincore::utc_timestamp(2021, 1, 3),
         fincore::utc_timestamp(2021, 1, 4),
         fincore::utc_timestamp(2021, 1, 5),
         fincore::utc_timestamp(2021, 1, 6),
         fincore::utc_timestamp(2021, 1, 7),
         fincore::utc_timestamp(2021, 1, 8)},
        "AAA", std::vector<double>(7, 10.0), "BBB",
        std::vector<double>(7, 20.0));
    EXPECT_THROW(static_cast<void>(fincore::days_to_liquidate_positions(
                     positions, shifted_prices, volumes)),
                 fincore::input_error);

    const std::array<char, 13> embedded_storage{
        'c', 'a', 's', 'h', ' ', 'x', '!', '!', '!', '!', '!', '!', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::days_to_liquidate_positions(
                     positions, prices, volumes, 0.2, 1e6, 2U,
                     embedded_selector)),
                 fincore::input_error);
}

TEST(Transactions, DailyTransactionBarsMatchPythonOracle) {
    const std::vector<fincore::timestamp> bar_index{
        fincore::utc_timestamp(2021, 1, 1),
        fincore::utc_timestamp(2021, 1, 2),
        fincore::utc_timestamp(2021, 1, 3),
        fincore::utc_timestamp(2021, 1, 4)};
    auto price_panel = wide_frame(bar_index, "AAA", {10.0, 11.0, 12.0, 13.0},
                                  "BBB", {20.0, 19.0, 21.0, 22.0});
    price_panel.load_column<double>("CCC", {14.0, 15.0, 16.0, 17.0},
                                    hmdf::nan_policy::dont_pad_with_nans);
    auto volume_panel = price_panel;
    volume_panel.load_column<double>(
        "AAA", {100.0, 200.0, 150.0, 120.0},
        hmdf::nan_policy::dont_pad_with_nans);
    volume_panel.load_column<double>(
        "BBB", {50.0, 60.0, 55.0, 70.0},
        hmdf::nan_policy::dont_pad_with_nans);
    volume_panel.load_column<double>(
        "CCC", {30.0, 40.0, 35.0, 45.0},
        hmdf::nan_policy::dont_pad_with_nans);
    const std::vector<fincore::trade_record> trades{
        {fincore::utc_timestamp(2021, 1, 2), "AAA", 10.0},
        {fincore::utc_timestamp(2021, 1, 2), "BBB", -5.0},
        {fincore::utc_timestamp(2021, 1, 3), "AAA", -20.0},
        {fincore::utc_timestamp(2021, 1, 4), "CCC", 3.0}};

    const auto bars =
        fincore::daily_txns_with_bar_data(trades, price_panel, volume_panel);
    ASSERT_EQ(bars.size(), 4U);
    EXPECT_EQ(bars[0].symbol, "AAA");
    EXPECT_EQ(bars[0].date, fincore::utc_timestamp(2021, 1, 2));
    EXPECT_DOUBLE_EQ(bars[0].amount, 10.0);
    EXPECT_DOUBLE_EQ(bars[0].price, 11.0);
    EXPECT_DOUBLE_EQ(bars[0].volume, 200.0);
    EXPECT_EQ(bars[1].symbol, "AAA");
    EXPECT_EQ(bars[1].date, fincore::utc_timestamp(2021, 1, 3));
    EXPECT_DOUBLE_EQ(bars[1].amount, 20.0);
    EXPECT_EQ(bars[2].symbol, "BBB");
    EXPECT_DOUBLE_EQ(bars[2].amount, 5.0);
    EXPECT_EQ(bars[3].symbol, "CCC");
    EXPECT_DOUBLE_EQ(bars[3].price, 17.0);
    EXPECT_DOUBLE_EQ(bars[3].volume, 45.0);
}

}  // namespace
