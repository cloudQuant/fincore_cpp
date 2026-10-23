# Phase 91 Completion Slice Acceptance — Portfolio Performance and Exposure

## Scope

Extends `build_portfolio_report` from performance/benchmark metrics into the
pinned section layout: performance series and drawdown table, rolling
benchmark beta, and an optional portfolio-exposure section from supplied
positions. No recalculation in renderers; the shared document schema is
unchanged.

## Performance section

- Metrics: annual/cumulative return, annual volatility, Sharpe, Calmar,
  stability, maximum drawdown, Omega, Sortino, tail ratio and historical VaR.
- Series: `returns` (decimal), `cumulative_returns` (growth multiple),
  `drawdown`, `rolling_sharpe`, `rolling_volatility` (annualized) and
  `monthly_returns`.
- Table `drawdowns`: the top five windows with rank, net drawdown, peak,
  valley, recovery and business-day duration; missing dates stay missing
  cells.

Rolling window defaults to 63 and is configurable through the report
argument; the report metadata records `rolling_window` and the observation
count.

## Benchmark section

Adds the `alpha` metric alongside information ratio, tracking error, up/down
capture, capture ratio and beta, plus the `rolling_beta` series computed from
the aligned strategy/benchmark pair. `benchmark_cumulative_returns` is
retained.

## Portfolio section

Optional `positions` input: a wide numeric frame indexed by timestamp with a
`cash` column. Every return timestamp must be covered, the index must be
unique, and all values must be finite. Publishes `asset_count`,
`average_gross_leverage`, `maximum_gross_leverage` and the
`gross_leverage` series from the accepted `gross_lev` kernel. The section
appears only when positions are supplied.

## Declared gaps

The transactions section remains open: the native frame has a unique index
and cannot represent multiple trades per timestamp, and turnover requires
the transaction ledger. Monthly returns use the native aggregation bucket
start rather than the pinned period-end label. Positions in dollars are not
scaled by capital. Native document bytes differ from Python and the HTML
renderer keeps its inline-SVG substitution.

## Tests and gates

`ReportPortfolio.BuildsPerformanceAndBenchmarkSections` now asserts the six
performance series, the drawdown table, the rolling-beta series, the alpha
metric, and a three-section document with a supplied positions frame
(gross leverage 1.0, two assets); it also asserts that incomplete positions
coverage fails with `input_error`. Full headless CTest: **710/710 passed**.
Sanitizer not re-run for this slice; visual NOT_RUN.
