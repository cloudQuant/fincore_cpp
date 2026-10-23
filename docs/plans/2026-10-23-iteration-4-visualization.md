# Iteration 4 Plan — Visualization (Phases 72–76)

**Goal:** Deliver render-neutral chart data for the four `VizBackend` plot
routes and selected correlation/frontier capabilities, plus optional ImPlot
mapping, without leaking GUI headers into the core.

**Baseline:** `dev` after Iteration 3 and the portfolio layout alignment
(oracle pin `4b8f31b`). Python source: `viz/contracts.py`,
`viz/matplotlib_backend.py`, `viz/html_backend.py`, `optimization/frontier.py`.

**Files:** `include/fincore/viz/chart_data.hpp`,
`src/viz/chart_data.cpp`, extended `include/fincore/viz/implot_backend.hpp`
and `src/viz/implot_backend.cpp`, `examples/implot_dashboard.cpp`,
`tests/unit/chart_data_test.cpp`, CMake lists, design/acceptance records,
README.

## Deliveries by phase

- **72:** `heat_grid` + `build_monthly_heatmap` (year×month compounding,
  twelve labels, explicit missing mask, percent unit, color domain).
- **73:** `series_chart`/`chart_series` + `build_series_chart`,
  `build_returns_chart`, `build_rolling_sharpe_chart` (named series,
  benchmark flag/style, zero line, window metadata).
- **74:** `build_drawdown_chart` with fill-to-zero semantics.
- **75:** `bar_chart`/`build_bar_chart` and
  `histogram_bins`/`build_histogram` (grouped labels, counts and density,
  fixed bin count, constant/NaN/empty cases).
- **76:** `correlation_matrix`/`build_correlation_matrix` (pairwise-finite
  Pearson, labels, missing mask) and
  `frontier_chart_data`/`build_frontier_chart` consuming the Phase 69 result
  without a solver.

## Contract

- Headless builders own arrays/labels; no ImGui/ImPlot headers in the core.
- Alignment is explicit per series (each series keeps its own index).
- Missing heatmap cells are missing, not zero; a count is distinct from a
  density; no implicit normalization is invented.
- Adapter functions map immutable chart data to ImPlot and never recompute
  metrics.
- Report `COMPUTE`, `BUILD` and `VISUAL` separately.

## Tests and gates

Headless oracle tests for monthly pivots, series metadata, bars, histograms,
correlation and frontier shapes; invalid-input cases. Core CTest,
ImPlot-adapter compile/link, demo build, optional visual fixture. No sanitizer
regression is claimed until the deferred re-run completes.
