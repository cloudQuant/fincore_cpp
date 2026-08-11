#pragma once

#include <fincore/metrics/transactions.hpp>

#include <chrono>
#include <string_view>

namespace fincore {

inline constexpr std::string_view transaction_symbol_column{"symbol"};
inline constexpr std::string_view round_trip_open_datetime_column{"open_dt"};
inline constexpr std::string_view round_trip_close_datetime_column{"close_dt"};
inline constexpr std::string_view round_trip_duration_column{"duration"};
inline constexpr std::string_view round_trip_pnl_column{"pnl"};
inline constexpr std::string_view round_trip_return_column{"rt_returns"};
inline constexpr std::string_view round_trip_long_column{"long"};

inline constexpr std::chrono::nanoseconds default_round_trip_max_delta{
    std::chrono::hours{8}};

dataframe groupby_consecutive_transactions(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

dataframe extract_round_trips(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

}  // namespace fincore
