# Iteration 5 slice 3 Acceptance — Research, Tears and Factor HTML

## Scope

Adds a bounded research evaluator (`factor/research.hpp`,
`src/factor/research.cpp`), factor tear chart data (`factor/tears.hpp`,
`src/factor/tears.cpp`) and factor HTML rendering through the shared renderer.
Four new paths plus tests and CMake registrations; no dependency change.

## Phase 83 research (bounded)

- `research_capabilities_report()` publishes the API/schema versions, label/
  preprocess/metric/table/export/status vocabularies from the pinned module.
- `research_spec` carries targets, date window, periods, quantiles,
  min-asset/min-quantile settings, preprocess, max_loss, stability lags,
  direction overrides and provenance strings.
- `evaluate_factor_research(factors, prices, spec)` filters the window and
  targets, applies +/- direction overrides, applies cross-section rank or
  z-score preprocessing, prepares each horizon with the accepted
  prepare-by-horizon kernel, and returns IC (date x horizon), IC means,
  quantile means/SE, per-quantile turnover, rank-autocorrelation stability,
  top-minus-bottom spread, coverage, diagnostics, manifest and status
  (`OK`/`PARTIAL`/`UNEVALUABLE`).
- Oracle fixture (60 days x 5 assets, periods 1/5, 3 quantiles): status OK,
  two horizons, 60 IC dates, first IC `0.9999999999999999`, mean IC
  `-0.1783333`, 3 quantile rows, 3 turnover series, 2 stability lags,
  negative spread (the factor's high quantile underperforms), coverage 300
  per horizon, manifest preprocess/periods. Direction reversal flips the IC
  sign; invalid preprocessing and empty windows fail with
  `factor_data_error`.

Declared gaps: the pinned grid/next-open price basis, universe/membership
masks, group inputs, precomputed-return label validation, factor directions
from `factors.attrs`, paired comparisons/segments, factor correlations,
`min_assets` enforcement, and JSON/CSV export are not ported in this slice.
The native label mode is close-to-close from the supplied prices.

## Phase 85 tears / Phase 86 HTML

- `build_factor_tear_charts` returns render-neutral `series_chart`/`bar_chart`
  data: IC by horizon (zero line), quantile-return bars, turnover by quantile,
  top-minus-bottom spread and rank autocorrelation. These map to the already
  accepted `plot_series_chart`/`plot_bar_chart` ImPlot functions; no GUI
  header enters the core. Visual rendering remains **NOT_RUN**.
- Factor HTML uses `render_html(build_factor_report(model))`: escaped
  semantic HTML, zh/en labels, inline SVG charts, deterministic bytes; the
  ECharts asset requirement is substituted by inline SVG and is not claimed
  as pixel-identical.

## Tests and gates

`factor_research_test.cpp` (2), `factor_tears_test.cpp` (1), and the extended
`factor_report_test.cpp` HTML assertions. Full headless CTest: **705/705
passed**. Sanitizer not re-run; ImPlot compile/link unchanged.
