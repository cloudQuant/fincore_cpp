# Iteration 4 — Visualization Completion: Design Document

**Iteration ID:** iteration-4-visualization
**Status:** Draft
**Depends on:** metrics kernel (Iteration 1) for the heatmap pivot; Iteration
3's frontier value types for Phase 76 (Phase 76 can land after Iteration 3's
Phase 69 only; the rest of this iteration has no Iteration 3 dependency and
may interleave).

## 1. Architecture Overview

Extend the existing optional adapter only:

```
include/fincore/viz/implot_backend.hpp   (declarations; new functions appended)
src/viz/implot_backend.cpp               (ImPlot calls; no computation)
examples/implot_dashboard.cpp            (manual demo; grows one panel per phase)
include/fincore/viz/chart_data.hpp       (NEW: render-neutral value types)
```

The key structural addition is `chart_data.hpp`: pure value types
(`heat_grid`, `series_set`, `bar_groups`, `histogram_bins`, `frontier_chart`)
that carry everything a renderer needs. Computation functions producing them
live in the **core** library (headless, testable, oracle-parity-checked);
`implot_backend.cpp` only consumes them. This split is what keeps the
headless gate honest and is reused verbatim by Iteration 5 (tear sheets) and
Iteration 6 (HTML renderer consumes the same value types).

## 2. Key Design Decisions

### D1 — Compute/render split via chart value types
Rationale: ImPlot cannot run in CI; putting quantitative decisions (bin
edges, tick layouts, colormaps, axis ranges) in value types makes them
oracle-testable and renderer-agnostic (ImPlot today, HTML in Iteration 6).
The matplotlib oracle's layout computations (e.g. `np.histogram` bin edges,
colormap normalization) become compute-side functions with hex-float
goldens.

### D2 — Colormap as a sampled ramp value
`heat_grid` carries its min/max and a named diverging scheme; the renderer
maps values through the scheme. The Phase 72 design records the sampled
RdYlGn-equivalent control points (transcribed from matplotlib's colormap
data) so color mapping is deterministic and testable without a display.

### D3 — Series sets over one aligned frame
`series_set` references named double columns of one aligned DataFrame plus
optional benchmark column with line-style flags. This mirrors how the
Python backends receive aligned Series and avoids inventing a multi-frame
container.

### D4 — Demo target is the only place ImGui widget code appears
Panels, layout, and demo data wiring live in `examples/implot_dashboard.cpp`.
The adapter and core library contain no widget code. The demo grows one
panel per phase and each acceptance records a manual run screenshot.

## 3. Component Plan

| Phase | Core files (compute) | Adapter additions |
|---|---|---|
| 72 | `metrics/` or `viz/chart_data.hpp` + `src/viz/chart_data.cpp`: monthly pivot + `heat_grid` | `plot_monthly_heatmap(const heat_grid&)` |
| 73 | `series_set` assembly helper (alignment reuse) | `plot_returns_multi(const series_set&)` |
| 74 | (existing drawdown series; fill needs no new compute) | `plot_drawdown_filled` |
| 75 | `bar_groups` / `histogram_bins` builders (bin edges oracle-locked) | `plot_bars`, `plot_histogram` |
| 76 | reuse Iteration 3 frontier types + correlation matrix → `heat_grid` | `plot_correlation_matrix`, `plot_efficient_frontier` |

## 4. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| ImPlot lacks a needed primitive (colormap bar, exact fill semantics) | Fidelity-note duty: each acceptance records substitution + visual delta; nearest-primitive mapping decided in the phase design, not ad hoc in code. |
| Hidden computation creeping into the adapter | Acceptance audits the adapter diff for numeric literals beyond layout constants. |
| Heatmap pivot semantics diverge from pandas pivot | Phase 72 compute side is oracle-tested (pivot values, month ordering, missing-month handling) before any render code. |
| Demo rot (GLFW dep unavailable on some machine) | Demo stays optional; adapter acceptance does not depend on the demo run, only records it when performed. |

## 5. Sequencing

72 first (introduces `chart_data.hpp`). 73, 74, 75 independent after 72.
76 last (correlation part can precede Iteration 3; frontier part requires
Phase 69). Iteration 4 phases 72–75 may interleave with Iteration 3.
