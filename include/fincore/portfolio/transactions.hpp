#pragma once

#include <fincore/portfolio/positions.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/report/portfolio.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Default signed-quantity column in transaction frames.
inline constexpr std::string_view transaction_amount_column{"amount"};
/// Default execution-price column in transaction frames.
inline constexpr std::string_view transaction_price_column{"price"};
/// Default bar share-volume column used by apply_slippage_penalty().
inline constexpr std::string_view transaction_bar_volume_column{"volume"};

/// Divisor convention used by get_turnover().
enum class turnover_denominator {
    /// Average of the previous and current gross book, excluding cash; the
    /// first row uses half of the current gross book.
    actual_gross_book,
    /// Row sum of all numeric position columns, including cash.
    portfolio_value,
};

/// Returns daily transaction notional and share totals.
///
/// Rows are grouped by UTC calendar day, normalized to midnight and sorted
/// ascending. ``txn_volume`` sums ``|amount| * price`` and ``txn_shares`` sums
/// ``|amount|``; contributions that evaluate to NaN are skipped, and infinite
/// values are preserved.
///
/// @param transactions Frame with double amount and price columns.
/// @param amount_column_name Signed-quantity column.
/// @param price_column_name Execution-price column.
/// @return Frame indexed by UTC day with ``txn_volume`` and ``txn_shares``
///     double columns.
/// @throws input_error If a column name is empty or duplicated, a required
///     column is absent or not double, or its length differs from the frame
///     index length.
dataframe get_txn_vol(
    const dataframe& transactions,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

/// Adjusts daily returns for slippage on traded notional.
///
/// Every UTC day in the sorted union of return, position and transaction days
/// is processed. A day with both a return and a portfolio value supplies
/// ``pnl = return * portfolio_value``; a cost of ``traded notional *
/// slippage_bps / 10000`` is subtracted from that PnL, and the day's return is
/// rescaled by the adjusted-to-original PnL ratio. Days without a usable
/// return/PnL pair and days with zero PnL yield 0.0.
///
/// @param returns Frame with a double ``returns_column_name`` column whose UTC
///     days are strictly increasing and unique.
/// @param positions Wide position frame whose double columns are summed per
///     row to obtain portfolio value; its UTC days must be strictly increasing
///     and unique.
/// @param transactions Transaction frame consumed by get_txn_vol().
/// @param slippage_bps Slippage in basis points of traded notional.
/// @param returns_column_name Input and output return column name.
/// @param amount_column_name Signed-quantity column in ``transactions``.
/// @param price_column_name Execution-price column in ``transactions``.
/// @return Frame indexed by the sorted union of UTC days with adjusted decimal
///     returns under ``returns_column_name``.
/// @throws input_error If a column is invalid, a return or position day is
///     duplicated or out of order, or a required transaction column is
///     invalid.
dataframe adjust_returns_for_slippage(
    const dataframe& returns,
    const dataframe& positions,
    const dataframe& transactions,
    double slippage_bps,
    std::string_view returns_column_name = returns_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

/// Applies a quadratic market-impact penalty to daily returns.
///
/// Trades are rescaled from backtest to simulation capital by
/// ``simulate_starting_capital / backtest_starting_capital``. For each trade,
/// ``shares = |scaled amount|``, ``dollars = price * shares`` and the penalty
/// is ``(shares / volume)^2 * impact * dollars``; penalties are summed by UTC
/// day. Return days are compounded into wealth, and each daily penalty is
/// divided by the simulated portfolio value and subtracted from that day's
/// return.
///
/// @param returns Frame with a double ``returns_column_name`` column whose UTC
///     days are strictly increasing and unique.
/// @param transactions_with_bar_data Frame with double amount, price and
///     volume columns of matching lengths.
/// @param simulate_starting_capital Starting capital of the simulation.
/// @param backtest_starting_capital Starting capital of the backtest; must be
///     nonzero.
/// @param impact Quadratic impact coefficient.
/// @param returns_column_name Input and output return column name.
/// @param amount_column_name Signed-quantity column.
/// @param price_column_name Execution-price column.
/// @param volume_column_name Bar share-volume column.
/// @return Frame indexed by UTC return days with adjusted decimal returns
///     under ``returns_column_name``.
/// @throws input_error If ``backtest_starting_capital`` is zero, a column name
///     is empty or duplicated, a required column is absent or not double, its
///     length differs from the frame index length, or return days are
///     duplicated or out of order.
dataframe apply_slippage_penalty(
    const dataframe& returns,
    const dataframe& transactions_with_bar_data,
    double simulate_starting_capital,
    double backtest_starting_capital,
    double impact = 0.1,
    std::string_view returns_column_name = returns_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column,
    std::string_view volume_column_name = transaction_bar_volume_column);

/// Returns daily turnover using the selected denominator.
///
/// The output index is the sorted union of position days and transaction days,
/// both normalized to UTC midnight. Transaction notional comes from
/// get_txn_vol(), and position days must be strictly increasing and unique.
/// ``actual_gross_book`` divides by the average of the previous and current
/// gross book (excluding cash), using half the current gross book on the first
/// row. ``portfolio_value`` divides by the row sum of all double position
/// columns, including cash. Non-finite ratios are returned as 0.0, and values
/// are ratios rather than percentages.
///
/// @param positions Wide position frame with at least one double column.
/// @param transactions Transaction frame consumed by get_txn_vol().
/// @param denominator Denominator convention.
/// @param cash_column_name Cash column excluded from ``actual_gross_book``;
///     unused by ``portfolio_value``.
/// @param amount_column_name Signed-quantity column in ``transactions``.
/// @param price_column_name Execution-price column in ``transactions``.
/// @return Frame indexed by UTC day with a ``turnover`` double column.
/// @throws input_error If the denominator value is unknown, a column name is
///     invalid, the cash column is missing or not double for
///     ``actual_gross_book``, position days are duplicated or out of order, or
///     a required transaction column is invalid.
dataframe get_turnover(
    const dataframe& positions,
    const dataframe& transactions,
    turnover_denominator denominator = turnover_denominator::actual_gross_book,
    std::string_view cash_column_name = cash_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

/// One trade used by the liquidity helpers.
struct trade_record {
    timestamp date;      ///< Trade timestamp; bar helpers match it exactly.
    std::string symbol;  ///< Traded symbol.
    double amount;       ///< Signed quantity; absolute value is used.
};

/// Per-symbol maximum liquidation time.
struct ticker_liquidity {
    std::string symbol;              ///< Position column name.
    double days_to_liquidate;        ///< Maximum days in the scan; NaN if none.
    /// Position allocation on that date as a percentage (100 * fraction).
    double position_allocation_pct;
};

/// One aggregated symbol/day trade bar.
struct daily_txn_bar {
    timestamp date;      ///< Trade date.
    std::string symbol;  ///< Traded symbol.
    double amount;       ///< Sum of absolute trade amounts for the day.
    double price;        ///< Bar price on that date.
    double volume;       ///< Bar share volume on that date.
};

/// Per-symbol worst transaction bar consumption.
struct transaction_liquidity {
    std::string symbol;               ///< Traded symbol.
    timestamp date;                   ///< Date of maximum consumption.
    /// Traded amount as a percentage of bar volume.
    double max_percent_bar_consumed;
};

/// Returns days needed to liquidate each position asset.
///
/// Position allocations use the row total of all double columns. Daily dollar
/// volume is the mean of ``price * volume`` over the preceding
/// ``mean_volume_window`` bars, so the first window rows are warm-up and are
/// omitted. Each value is ``allocation * capital_base / (max_bar_consumption *
/// mean dollar volume)``; a NaN or zero mean dollar volume yields NaN for that
/// row. The named cash column is excluded when present but is not required.
///
/// @param positions Wide position frame with a double column per asset.
/// @param prices Bar price frame with the same index as ``positions`` and a
///     double column for every non-cash asset.
/// @param volumes Bar share-volume frame with the same index as ``positions``
///     and a double column for every non-cash asset.
/// @param max_bar_consumption Maximum fraction of a bar's dollar volume per
///     day; must be in (0, 1].
/// @param capital_base Capital in currency units used to scale positions; must
///     be positive.
/// @param mean_volume_window Number of preceding bars in the dollar-volume
///     mean; must be at least 1.
/// @param cash_column_name Cash column excluded from the output.
/// @return Frame indexed by position dates after warm-up with one days column
///     per non-cash asset; empty when there are no complete windows.
/// @throws input_error If a numeric parameter is out of range, the frames do
///     not share the same index, a position double column is malformed, or
///     ``cash_column_name`` is empty or contains an embedded NUL.
dataframe days_to_liquidate_positions(
    const dataframe& positions,
    const dataframe& prices,
    const dataframe& volumes,
    double max_bar_consumption = 0.2,
    double capital_base = 1'000'000.0,
    std::size_t mean_volume_window = 5U,
    std::string_view cash_column_name = cash_column);

/// Returns the maximum liquidation time per position symbol.
///
/// Uses days_to_liquidate_positions() with the supplied parameters. When
/// ``last_n_days`` is set, only metric rows at or after ``newest -
/// last_n_days`` days are considered. Each record reports the maximum days
/// value and the position allocation on that date as a percentage
/// (100 * fraction). Symbols with no valid value keep NaN fields. Results
/// are sorted by symbol.
///
/// @param positions Wide position frame with a double column per asset.
/// @param prices Bar price frame with the same index as ``positions`` and a
///     double column for every non-cash asset.
/// @param volumes Bar share-volume frame with the same index as ``positions``
///     and a double column for every non-cash asset.
/// @param max_bar_consumption Maximum fraction of a bar's dollar volume per
///     day; must be in (0, 1].
/// @param capital_base Capital in currency units used to scale positions; must
///     be positive.
/// @param mean_volume_window Number of preceding bars in the dollar-volume
///     mean; must be at least 1.
/// @param last_n_days Optional number of trailing days to scan.
/// @param cash_column_name Cash column excluded from the output.
/// @return One record per non-cash asset, sorted by symbol.
/// @throws input_error If days_to_liquidate_positions() rejects a parameter,
///     an index, or a position column.
std::vector<ticker_liquidity> get_max_days_to_liquidate_by_ticker(
    const dataframe& positions,
    const dataframe& prices,
    const dataframe& volumes,
    double max_bar_consumption = 0.2,
    double capital_base = 1'000'000.0,
    std::size_t mean_volume_window = 5U,
    std::optional<std::size_t> last_n_days = std::nullopt,
    std::string_view cash_column_name = cash_column);

/// Returns daily aggregated trade bars joined to price and volume panels.
///
/// Trades are summed by exact ``(symbol, date)`` using absolute amounts; dates
/// are not normalized. Each transaction date must exist in the price index,
/// and each traded symbol must have both price and volume columns. The result
/// is sorted by symbol and then date.
///
/// @param transactions Trade records.
/// @param prices Bar price panel.
/// @param volumes Bar share-volume panel with the same index as ``prices``.
/// @return One bar per symbol/day with absolute amount, price and volume.
/// @throws input_error If an amount is not finite, a trade date is absent from
///     the price index, a traded symbol lacks a price or volume column, or the
///     price and volume indices differ.
std::vector<daily_txn_bar> daily_txns_with_bar_data(
    std::span<const trade_record> transactions,
    const dataframe& prices,
    const dataframe& volumes);

/// Returns the worst bar consumption per symbol for recent trades.
///
/// Trades are summed by exact ``(symbol, date)`` using absolute amounts. When
/// ``last_n_days`` is set, only dates strictly after ``newest -
/// last_n_days`` days are scanned, where ``newest`` is the latest trade date.
/// For each symbol the date with the largest ``amount / volume * 100`` is
/// retained, and results are sorted by symbol.
///
/// @param transactions Trade records.
/// @param prices Price panel whose index defines valid trade dates.
/// @param volumes Bar share-volume panel with the same index as ``prices`` and
///     a volume column for every scanned symbol.
/// @param last_n_days Optional number of trailing days to scan.
/// @return One record per symbol with a scanned trade; empty when there are no
///     transactions.
/// @throws input_error If an amount is not finite, a trade date is absent from
///     the price index, or the price and volume indices differ.
std::vector<transaction_liquidity> get_low_liquidity_transactions(
    std::span<const trade_record> transactions,
    const dataframe& prices,
    const dataframe& volumes,
    std::optional<std::size_t> last_n_days = std::nullopt);

/// Validates and returns a copy of a portfolio transaction.
///
/// A valid transaction has a non-empty symbol and finite amount and price;
/// other fields are copied unchanged.
///
/// @param transaction Transaction to validate.
/// @return Copy of ``transaction``.
/// @throws fincore_error If the symbol is empty or the amount or price is not
///     finite.
portfolio_transaction map_transaction(
    const portfolio_transaction& transaction);

/// Builds an owning transaction frame from validated transactions.
///
/// Each input is validated with map_transaction(). The frame index holds the
/// transaction dates; columns are the string ``symbol`` and the double
/// ``amount`` and ``price`` columns. Input order and duplicate dates are
/// preserved.
///
/// @param transactions Transactions to convert.
/// @return Frame with symbol, amount and price columns indexed by date.
/// @throws fincore_error If any transaction fails validation.
dataframe make_transaction_frame(
    std::span<const portfolio_transaction> transactions);

}  // namespace fincore
