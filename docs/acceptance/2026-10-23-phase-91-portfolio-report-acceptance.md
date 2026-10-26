# Phase 91 Portfolio Report Projection Acceptance (reduced)

## Scope

Adds `build_portfolio_report(returns, benchmark?, title)` projecting accepted
native metrics into the shared `ReportDocument`: a `performance` section
(annual/cumulative return, annual volatility, Sharpe, Sortino, Calmar,
stability, max drawdown, Omega, tail ratio, historical VaR, cumulative
returns series) and an optional `benchmark` section (information ratio,
annualized active-return tracking error, up/down capture, capture ratio, beta,
benchmark cumulative returns), with notes and no render-time recalculation.

## Status

Partial Phase 91: the risk projection is accepted separately; this slice adds
the returns/benchmark portfolio builder. The pinned builder's positions,
transactions/turnover, rolling beta and leverage sections are **not** ported
here and remain open.

## Tests and gates

`report_portfolio_test.cpp` checks both sections, metric parity with direct
native calls, the cumulative growth series, tracking error positivity, beta
parity, JSON determinism and the no-benchmark shape. Full headless CTest:
**702/702 passed**. Sanitizer not re-run; ImPlot unchanged.
