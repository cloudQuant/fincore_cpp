#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/report/models.hpp>

#include <cstddef>
#include <span>
#include <string>

namespace fincore {

/// One executed trade used by the transaction activity projection.
struct portfolio_transaction {
    /// UTC nanosecond timestamp; trades are grouped by calendar day.
    timestamp date = 0;
    std::string symbol;  ///< Traded symbol.
    /// Signed quantity; notional is computed as `|amount| * price`.
    double amount = 0.0;
    double price = 0.0;  ///< Execution price per unit.
};

/// Builds a portfolio report document from prepared return and position data.
///
/// The document has domain "portfolio". It always contains a "performance"
/// section, adds a "benchmark" section when `benchmark` is supplied, adds a
/// "portfolio" section when `positions` is supplied, and adds a "transactions"
/// section when `transactions` is non-empty. Metrics are computed once from
/// the supplied inputs; renderers do not recompute them.
///
/// @param returns Daily simple returns with a required "returns" double
///     column.
/// @param benchmark Optional benchmark frame with a "benchmark" double
///     column; enables the benchmark comparison section.
/// @param title Document title.
/// @param positions Optional positions frame with a "cash" column, a unique
///     index, finite values, and a row for every return timestamp; enables the
///     portfolio exposure section.
/// @param rolling_window Rolling window for the rolling Sharpe, volatility and
///     beta series; must be positive.
/// @param transactions Optional transactions used for the transaction section
///     and, with `positions`, for turnover.
/// @return Report document with observations, daily period and rolling-window
///     metadata.
/// @throws input_error If `rolling_window` is zero, a required column is
///     missing or has the wrong type, positions are empty, duplicated, not
///     finite or do not cover the return timestamps, or a transaction has a
///     non-finite amount or price.
report_document build_portfolio_report(
    const dataframe& returns, const dataframe* benchmark = nullptr,
    std::string title = "Portfolio Report",
    const dataframe* positions = nullptr,
    std::size_t rolling_window = 63U,
    std::span<const portfolio_transaction> transactions = {});

}  // namespace fincore
