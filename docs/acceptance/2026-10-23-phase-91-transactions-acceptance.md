# Phase 91 Transactions Slice Acceptance — Typed Trade Activity Projection

## Scope

Closes the transactions section of the portfolio report. Because the native
`dataframe` index is unique and cannot hold several trades per timestamp, the
builder accepts a typed `std::span<const portfolio_transaction>` ledger
(`date`, `symbol`, `amount`, `price`) instead of a transaction DataFrame.

## Behavior

- Each transaction must carry finite amount and price values; otherwise
  `input_error`.
- Activity is grouped by UTC calendar day (`date / 86_400e9`), so multiple
  trades on one day share a count/notional point.
- Metrics: `transaction_count`, `trading_days`,
  `average_daily_transactions`, `average_daily_notional` (absolute amount
  times price) and `symbol_count`.
- Series: `daily_transaction_count` (count) and
  `daily_transaction_notional` (notional), in date order.
- The `transactions` section appears only when the ledger is non-empty and is
  appended after performance/benchmark/portfolio.

Declared gap: the source portfolio section also reports turnover when both
positions and transactions are supplied; the native report does not yet
derive turnover from the typed ledger.

## Tests and gates

`ReportPortfolio.ProjectsTransactionActivity` builds three trades across two
days (two on day one), asserts the series (`2/1` trades and `150/12` notional),
the transaction/day metrics and the section order, and asserts that a
non-finite price fails with `input_error`. Full headless CTest:
**715/715 passed**. Sanitizer not re-run; visual NOT_RUN.
