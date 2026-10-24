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

inline constexpr std::string_view transaction_amount_column{"amount"};
inline constexpr std::string_view transaction_price_column{"price"};
inline constexpr std::string_view transaction_bar_volume_column{"volume"};

enum class turnover_denominator {
    actual_gross_book,
    portfolio_value,
};

dataframe get_txn_vol(
    const dataframe& transactions,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

dataframe adjust_returns_for_slippage(
    const dataframe& returns,
    const dataframe& positions,
    const dataframe& transactions,
    double slippage_bps,
    std::string_view returns_column_name = returns_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

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

dataframe get_turnover(
    const dataframe& positions,
    const dataframe& transactions,
    turnover_denominator denominator = turnover_denominator::actual_gross_book,
    std::string_view cash_column_name = cash_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

struct trade_record {
    timestamp date;
    std::string symbol;
    double amount;
};

struct ticker_liquidity {
    std::string symbol;
    double days_to_liquidate;
    double position_allocation_pct;
};

struct daily_txn_bar {
    timestamp date;
    std::string symbol;
    double amount;
    double price;
    double volume;
};

struct transaction_liquidity {
    std::string symbol;
    timestamp date;
    double max_percent_bar_consumed;
};

dataframe days_to_liquidate_positions(
    const dataframe& positions,
    const dataframe& prices,
    const dataframe& volumes,
    double max_bar_consumption = 0.2,
    double capital_base = 1'000'000.0,
    std::size_t mean_volume_window = 5U,
    std::string_view cash_column_name = cash_column);

std::vector<ticker_liquidity> get_max_days_to_liquidate_by_ticker(
    const dataframe& positions,
    const dataframe& prices,
    const dataframe& volumes,
    double max_bar_consumption = 0.2,
    double capital_base = 1'000'000.0,
    std::size_t mean_volume_window = 5U,
    std::optional<std::size_t> last_n_days = std::nullopt,
    std::string_view cash_column_name = cash_column);

std::vector<daily_txn_bar> daily_txns_with_bar_data(
    std::span<const trade_record> transactions,
    const dataframe& prices,
    const dataframe& volumes);

std::vector<transaction_liquidity> get_low_liquidity_transactions(
    std::span<const trade_record> transactions,
    const dataframe& prices,
    const dataframe& volumes,
    std::optional<std::size_t> last_n_days = std::nullopt);

// Native transaction normalization: finite amount/price and a non-empty
// symbol. The pinned source dict also carries sid/order_id/commission; the
// native record intentionally keeps the numeric/table fields owned by the
// portfolio report model.
portfolio_transaction map_transaction(
    const portfolio_transaction& transaction);

// Build an owning transaction frame indexed by date with symbol, amount and
// price columns. Duplicate dates and input order are preserved.
dataframe make_transaction_frame(
    std::span<const portfolio_transaction> transactions);

}  // namespace fincore
