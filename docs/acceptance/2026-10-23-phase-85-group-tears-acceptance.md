# Phase 85 Acceptance — Grouped Factor Tear Chart Data

## Scope

`factor_tear_charts` now carries a `grouped` vector with one
`factor_group_tear_charts` per group when the analysis model was built with
`by_group=true`. Each entry exposes the same render-neutral panels as the
aggregate charts: IC by period, quantile-return bars, primary-lag turnover,
top-minus-bottom spread and rank autocorrelation.

## Behaviour

- Aggregate charts are unchanged and keep their existing titles/labels.
- Group charts are derived only from stored `factor_group_analysis` fields;
  no kernel is re-entered while building chart data.
- Groups keep the model's sorted label order. The panels map directly to the
  accepted ImPlot series/bar functions; no GUI header enters the core.
- Visual rendering remains **NOT_RUN**; this slice accepts chart data only.

## Tests and gates

New `FactorTears.BuildsGroupedChartData`: two groups (G1/G2), one IC series
with four points each, Q1/Q2 quantile bars, two primary-lag turnover series,
two rank-autocorrelation series, and a spread of `0.01`. The existing neutral
chart test is unchanged. Full Release and ASan+UBSan suites:
**727/727 passed**.
