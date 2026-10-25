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

/// Default symbol column name in transaction and round-trip frames.
inline constexpr std::string_view transaction_symbol_column{"symbol"};
/// Round-trip opening timestamp column.
inline constexpr std::string_view round_trip_open_datetime_column{"open_dt"};
/// Round-trip closing timestamp column.
inline constexpr std::string_view round_trip_close_datetime_column{"close_dt"};
/// Round-trip elapsed-nanoseconds column, stored as a timestamp.
inline constexpr std::string_view round_trip_duration_column{"duration"};
/// Round-trip profit/loss column.
inline constexpr std::string_view round_trip_pnl_column{"pnl"};
/// Round-trip return column.
inline constexpr std::string_view round_trip_return_column{"rt_returns"};
/// Round-trip long/short flag column.
inline constexpr std::string_view round_trip_long_column{"long"};

/// Default maximum gap (8 hours) between transactions merged into one block.
inline constexpr std::chrono::nanoseconds default_round_trip_max_delta{
    std::chrono::hours{8}};

/// Groups consecutive same-direction transactions into price/amount blocks.
///
/// Rows are ordered by symbol, timestamp and original position, then adjacent
/// rows of the same symbol with amounts both positive or both non-positive are
/// merged while the gap to the previous row does not exceed ``max_delta``. A
/// block's amount is the sum of its amounts, its price is the amount-weighted
/// average price, and its timestamp is the first row's timestamp. A negative
/// ``max_delta`` disables merging.
///
/// @param transactions Frame with a string symbol column and double amount and
///     price columns.
/// @param max_delta Maximum allowed gap between merged transactions.
/// @param symbol_column_name String symbol column.
/// @param amount_column_name Double signed-quantity column.
/// @param price_column_name Double execution-price column.
/// @return Frame indexed by each block's first timestamp with amount, symbol
///     and price columns under the supplied names.
/// @throws input_error If a field name is empty or duplicated, a required
///     column is absent or of the wrong type, or a column length differs from
///     the frame index length.
dataframe groupby_consecutive_transactions(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

/// Named statistic applied by agg_all_long_short().
struct round_trip_statistic {
    std::string name;  ///< Output statistic name.
    /// Callable statistic; must be set before aggregation.
    std::function<double(std::span<const double>)> compute;
};

/// Per-statistic results for all, long and short round trips.
struct long_short_aggregate {
    /// Statistic names in input order.
    std::vector<std::string> statistic_names;
    /// Statistic values over all round trips.
    std::vector<double> all_trades;
    /// Statistic values over long round trips; NaN when none exist.
    std::vector<double> long_trades;
    /// Statistic values over short round trips; NaN when none exist.
    std::vector<double> short_trades;
};

/// Applies caller-supplied statistics to all, long and short round trips.
///
/// Each statistic is evaluated over every value of ``column`` in row order,
/// then over the subset selected by the required ``long`` boolean column. A
/// statistic whose subset is empty yields NaN for that subset.
///
/// @param round_trips Frame containing a double value column and a bool
///     ``long`` column.
/// @param column Value column to aggregate.
/// @param statistics Statistics to apply, in output order.
/// @return Statistic names and per-statistic values for all, long and short
///     trades.
/// @throws input_error If ``column`` is not a valid double column, a column
///     length differs from the frame index length, or a statistic has no
///     callable ``compute``.
long_short_aggregate agg_all_long_short(
    const dataframe& round_trips, std::string_view column,
    std::span<const round_trip_statistic> statistics);

/// Transaction record used to build round-trip inputs and closing trades.
struct round_trip_transaction {
    timestamp date = 0;    ///< UTC nanosecond timestamp.
    std::string symbol;    ///< Traded symbol.
    double amount = 0.0;   ///< Signed quantity.
    double price = 0.0;    ///< Execution price per unit.
};

/// Appends one closing transaction per still-open position.
///
/// The net amount per symbol is computed from ``transactions``. For each
/// non-cash double column of ``positions`` with an absolute net amount of at
/// least 1e-6, a closing transaction is appended at the final position
/// timestamp plus one second. Its amount is the opposite of the net amount and
/// its price is the final position value divided by the net amount.
/// Zero-amount transactions are then removed from the combined result.
///
/// @param positions Frame with at least one row and a double ``cash`` column;
///     its other double columns are treated as positions.
/// @param transactions Existing transactions, preserved in input order.
/// @return Input transactions followed by generated closing transactions,
///     with zero-amount entries removed.
/// @throws input_error If ``positions`` is empty or has no double ``cash``
///     column.
std::vector<round_trip_transaction> add_closing_transactions(
    const dataframe& positions,
    std::span<const round_trip_transaction> transactions);

/// Copies round trips and adds a sector column from symbol mappings.
///
/// The output keeps the input index, every double column, and every string
/// column except an existing ``sector``; columns of other types are not
/// copied. When a string ``symbol`` column is present, a new ``sector`` column
/// is appended using ``sector_mappings``; symbols absent from the map get an
/// empty string. Without a symbol column no sector column is added.
///
/// @param round_trips Round-trip frame to copy.
/// @param sector_mappings Symbol-to-sector mapping.
/// @return Copied frame with the mapped sector column when possible.
dataframe apply_sector_mappings_to_round_trips(
    const dataframe& round_trips,
    const std::map<std::string, std::string>& sector_mappings);

/// Extracts FIFO round trips from transaction rows.
///
/// Transactions are first consolidated as in groupby_consecutive_transactions().
/// Each block is matched first-in, first-out against open lots of the opposite
/// direction; a block may both close existing lots and open a remainder.
/// Blocks with a negative price are skipped. The output index is the
/// sequential round-trip number starting at 0.
///
/// Output columns are ``pnl``, ``open_dt``, ``close_dt``, ``long``,
/// ``rt_returns``, ``symbol`` and ``duration`` (elapsed nanoseconds stored as
/// a timestamp). ``long`` is true for a round trip closed by a sell;
/// ``rt_returns`` is ``pnl`` divided by matched opening notional, or 0 when
/// that notional is zero.
///
/// @param transactions Frame with a string symbol column and double amount and
///     price columns.
/// @param max_delta Maximum allowed gap when consolidating transactions.
/// @param symbol_column_name String symbol column.
/// @param amount_column_name Double signed-quantity column.
/// @param price_column_name Double execution-price column.
/// @return Round-trip frame with a sequential index and the documented
///     columns.
/// @throws input_error If a field name or required column is invalid, or an
///     elapsed duration cannot be represented as a timestamp.
dataframe extract_round_trips(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

/// Rectangular statistic table with row and column labels.
struct round_trip_stat_table {
    std::vector<std::string> row_labels;     ///< Labels for the value rows.
    std::vector<std::string> column_labels;  ///< Labels for the value columns.
    /// Row-major statistic values.
    std::vector<std::vector<double>> values;
};

/// Round-trip tables and per-symbol return statistics.
///
/// The first four tables contain rows for the non-empty ``All trades``,
/// ``short`` and ``long`` groups in that order.
struct round_trip_stats {
    round_trip_stat_table pnl;       ///< Profit/loss statistics.
    round_trip_stat_table summary;   ///< Trade counts and profitability.
    /// Holding-duration statistics in days, plus trades per day and month.
    round_trip_stat_table duration;
    round_trip_stat_table returns;   ///< Round-trip return statistics.
    /// Symbols in ascending order.
    std::vector<std::string> symbol_labels;
    /// Fixed labels for the eight per-symbol return statistics.
    std::vector<std::string> symbol_stat_labels;
    /// One row per symbol statistic, one value per symbol in
    /// ``symbol_labels``.
    std::vector<std::vector<double>> symbol_values;
};

/// Computes round-trip performance tables and per-symbol statistics.
///
/// Durations are derived as ``close - open`` in days. Each table contains rows
/// for the non-empty ``All trades``, ``short`` and ``long`` groups in that
/// order, with fixed English column labels. Per-symbol values use the
/// statistics listed in ``symbol_stat_labels``, one column per symbol. An
/// empty frame yields default-constructed, empty statistics.
///
/// @param round_trips Frame containing the required pnl, return, long, symbol,
///     open and close columns.
/// @param pnl_column_name Double profit/loss column.
/// @param returns_column_name Double round-trip return column.
/// @param long_column_name Bool long/short flag column.
/// @param symbol_column_name String symbol column.
/// @param open_column_name Timestamp opening column.
/// @param close_column_name Timestamp closing column.
/// @return Round-trip statistic tables and per-symbol statistics.
/// @throws input_error If a column name contains an embedded NUL, a required
///     double column is invalid, or a required column length differs from the
///     frame index length.
round_trip_stats gen_round_trip_stats(
    const dataframe& round_trips,
    std::string_view pnl_column_name = round_trip_pnl_column,
    std::string_view returns_column_name = round_trip_return_column,
    std::string_view long_column_name = round_trip_long_column,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view open_column_name = round_trip_open_datetime_column,
    std::string_view close_column_name = round_trip_close_datetime_column);

}  // namespace fincore
