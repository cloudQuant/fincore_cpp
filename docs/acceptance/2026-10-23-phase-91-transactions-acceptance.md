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
- When positions and transactions are both supplied, daily traded notional is
  aggregated into a one-row-per-day ledger and passed through the accepted
  `get_turnover` kernel. The portfolio section adds a `turnover` series and
  an `average_turnover` metric. Oracle values for the fixture: `2.0` and
  `0.08571428571428572` on the two days, mean `1.042857142857143`.

## Tests and gates

`ReportPortfolio.ProjectsTransactionActivity` builds three trades across two
days (two on day one), asserts the series (`2/1` trades and `150/12` notional),
the transaction/day metrics and the section order, and asserts that a
non-finite price fails with `input_error`. Full headless CTest:
**722/722 passed**. Sanitizer not re-run; visual NOT_RUN.
