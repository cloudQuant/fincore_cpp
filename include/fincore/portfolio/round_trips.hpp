#pragma once

#include <fincore/portfolio/transactions.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>
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

struct round_trip_statistic {
    std::string name;
    std::function<double(std::span<const double>)> compute;
};

struct long_short_aggregate {
    std::vector<std::string> statistic_names;
    std::vector<double> all_trades;
    std::vector<double> long_trades;
    std::vector<double> short_trades;
};

/// Apply caller-supplied statistics to all, long and short round trips.
long_short_aggregate agg_all_long_short(
    const dataframe& round_trips, std::string_view column,
    std::span<const round_trip_statistic> statistics);

struct round_trip_transaction {
    timestamp date = 0;
    std::string symbol;
    double amount = 0.0;
    double price = 0.0;
};

/// Append one closing transaction per still-open position at the final
/// position timestamp plus one second.
std::vector<round_trip_transaction> add_closing_transactions(
    const dataframe& positions,
    std::span<const round_trip_transaction> transactions);

dataframe apply_sector_mappings_to_round_trips(
    const dataframe& round_trips,
    const std::map<std::string, std::string>& sector_mappings);

dataframe extract_round_trips(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

struct round_trip_stat_table {
    std::vector<std::string> row_labels;
    std::vector<std::string> column_labels;
    std::vector<std::vector<double>> values;
};

struct round_trip_stats {
    round_trip_stat_table pnl;
    round_trip_stat_table summary;
    round_trip_stat_table duration;
    round_trip_stat_table returns;
    std::vector<std::string> symbol_labels;
    std::vector<std::string> symbol_stat_labels;
    std::vector<std::vector<double>> symbol_values;
};

round_trip_stats gen_round_trip_stats(
    const dataframe& round_trips,
    std::string_view pnl_column_name = round_trip_pnl_column,
    std::string_view returns_column_name = round_trip_return_column,
    std::string_view long_column_name = round_trip_long_column,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view open_column_name = round_trip_open_datetime_column,
    std::string_view close_column_name = round_trip_close_datetime_column);

}  // namespace fincore
