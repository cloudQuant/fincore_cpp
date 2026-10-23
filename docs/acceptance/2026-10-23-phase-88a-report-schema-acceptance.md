# Phase 88a / 84-slice Acceptance — Report Schema and Factor Projection

## Scope

Extracts the deterministic JSON writer to `include/fincore/detail/json.hpp`,
adds the shared `report_document`/section/metric/table/series/cell schema
(`include/fincore/report/models.hpp`, `src/report/models.cpp`) and a reduced
factor projection (`include/fincore/factor/report.hpp`,
`src/factor/report.cpp`). Eight new/changed paths plus tests; no dependency
change.

## Evidence

- JSON writer extraction is regression-free: the eight `RiskReport` tests
  still pass unchanged (including the exact unsupported-report JSON and
  float/timestamp/escaping cases).
- `report_models_test.cpp` pins the full deterministic JSON for a document
  with number/text/missing metrics, a mixed table, a series and sorted
  metadata; malformed table rows, mismatched series lengths and NaN metrics
  raise `input_error`.
- `factor_report_test.cpp` builds a tiny compute-once model and verifies the
  `factor_summary`/`factor_performance` sections, fingerprint metadata,
  four tables (IC, quantile returns, alpha/beta, turnover), one factor-return
  series, two notes, `mean_ic.1D` and deterministic JSON.

## Status

- **Phase 88a:** accepted for the native document schema and JSON
  serialization.
- **Phase 84:** partial. The reduced factor projection is renderer-ready,
  but cumulative/quantile return views, positions/spreads, grouped and
  time-aggregated results and the full factor tear set are not yet projected.
- **Phase 86/89:** rendering is not started in this slice.
- **Phase 92:** runtime registration is not started.

## Gates

Full headless CTest: **690/690 passed**. Sanitizer: not re-run (no claim).
ImPlot compile/link unchanged.

## Limits

The schema is a versioned compute model, not a renderer. Missing values stay
explicit; no metric is recalculated during serialization.
