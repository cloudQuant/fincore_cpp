#pragma once

#include <fincore/metrics/positions.hpp>

#include <string_view>

namespace fincore {

inline constexpr std::string_view transaction_amount_column{"amount"};
inline constexpr std::string_view transaction_price_column{"price"};

enum class turnover_denominator {
    actual_gross_book,
    portfolio_value,
};

dataframe get_txn_vol(
    const dataframe& transactions,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

dataframe get_turnover(
    const dataframe& positions,
    const dataframe& transactions,
    turnover_denominator denominator = turnover_denominator::actual_gross_book,
    std::string_view cash_column_name = cash_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

}  // namespace fincore
