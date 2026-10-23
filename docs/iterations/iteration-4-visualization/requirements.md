# Iteration 4 — Visualization Completion: Requirements Specification

**Iteration ID:** iteration-4-visualization
**Phase range:** Phases 72–76
**Source of truth:** fincore Python `dev@4b8f31b`: `fincore/viz/` (backend protocol + matplotlib backend)
**Status:** Draft

> Roadmap amendment 2026-10-23: former Iteration 5 (viz) renumbered to 4
> after the risk/optimization merge.

## 1. Purpose and Scope

Complete the ImPlot adapter so the C++ library exposes the full non-factor
`VizBackend` protocol surface: monthly heatmap, multi-series returns with
benchmark overlay, drawdown underwater fill, bar/histogram/distribution
plots, correlation-matrix heatmap, and efficient-frontier plotting. The
adapter remains optional and headless-gated: **no GUI runtime enters the core
library**; compile/link-only gates in CI, with the GLFW demo target as the
manual-run vehicle.

## 2. Entry Conditions

- Existing `fincore_implot` target and the three adapter functions
  (`plot_cumulative_returns`, `plot_drawdown`, `plot_rolling_metric`) green
  under the ImPlot compile/link-only gate.
- Local ImGui/ImPlot checkouts validated by the existing CMake wiring.
- Iteration 1 complete if the monthly-heatmap pivot is needed at compute time
  (Phase 72 carries its own pivot computation, so only the metrics kernel is
  required).

## 3. Functional Requirements

| Phase | Adapter function | Python reference (`fincore/viz/matplotlib_backend.py` unless noted) | Requirement summary |
|---|---|---|---|
| 72 | `plot_monthly_heatmap` | `plot_monthly_heatmap` | Year×month return grid with diverging colormap (RdYlGn-equivalent), zero-centered normalization, colorbar, year/month tick labels. **Includes the compute-side year×month pivot** (from a monthly-resampled returns series) as a public value-type entry, parity-tested against the pandas pivot oracle. |
| 73 | multi-series returns + benchmark | `plot_returns` (plotly multi-line variant as reference) | N series + dashed benchmark overlay + legend + zero line. Series passed as named column sets on one aligned frame, or as a small vector of series views — frozen at design time. |
| 74 | drawdown underwater fill | `plot_drawdown` | Filled area below zero in the established red fill; axis semantics match the existing line version. |
| 75 | bar + histogram/distribution | factor plotting needs (`render_matplotlib.py` bar/hist families as reference) | `plot_bars` (grouped, value-labeled) and `plot_histogram` (fixed-bin). These are the tear-sheet primitives Iteration 5 consumes. |
| 76 | `plot_correlation_matrix` + `plot_efficient_frontier` | plotly backend | Correlation heatmap (diverging colormap + colorbar, reuse Phase 72 machinery); frontier scatter + capital-allocation line consuming Iteration 3's frontier value types. |

### 3.1 Cross-cutting functional requirements

- Every adapter function is a **render-call declaration**: it receives
  ImPlot/ImGui context state as the existing adapter functions do; it computes
  nothing beyond axis ranges/tick layouts. All quantitative computation stays
  in the core library; the adapter only maps value types to ImPlot calls.
- Fidelity notes: where ImPlot lacks a matplotlib primitive (exact RdYlGn
  colormap, seaborn violin), the phase records the substitution and its
  visual delta in the acceptance record. No silently dropped features.
- Headless guarantee: the core `fincore` target must build with
  `FINCORE_BUILD_IMPLOT=OFF` unchanged.

## 4. Non-Functional Requirements

- Gates per phase: the standard SDD loop adapted to viz — the oracle run
  covers the compute side (pivot values, axis ranges, tick layouts, bin
  edges) with hex-float goldens; the render side is accepted by
  compile/link-only evidence plus one manual GLFW demo run recorded in the
  acceptance doc (screenshot path under the evidence root).
- The invalid-ImGui expected-failure build stays green whenever ImPlot wiring
  changes.
- No new third-party dependency; ImPlot/ImGui stay optional.

## 5. Exit Criteria

- Phases 72–76 accepted; the C++ viz adapter exposes the full non-factor
  `VizBackend` protocol set (audit table in the final acceptance record).
- Core library builds headless with ImPlot OFF; `fincore_implot` and demo
  build with ImPlot ON; invalid-ImGui expected-failure build fails as
  expected.
- Iteration-head clean build + full CTest + ASan/UBSan evidence; commits
  pushed.

## 6. Out of Scope

- Factor tear sheets (Iteration 5 composes these primitives).
- plotly/bokeh backends (non-port); HTML report rendering (Iteration 6).
- Any stateful GUI: no windows management, no event loop, no ImGui widgets
  beyond what ImPlot plotting requires.
