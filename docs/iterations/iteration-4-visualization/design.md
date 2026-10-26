# Iteration 4 — Visualization: Design

**Status:** Revised plan. [Shared contract](../delivery-contract.md) applies.

## 1. Architecture

Keep `include/fincore/viz/implot_backend.hpp` and
`src/viz/implot_backend.cpp` as the optional adapter; extend the existing
`examples/implot_dashboard.cpp` demonstration. Add render-neutral data types
under `include/fincore/viz/chart_data.hpp` with headless builders in the core.
No ImGui/ImPlot declarations may leak into chart/report models.

Model `heat_grid`, `series_set`, `bar_groups`, `histogram_bins` and
`frontier_chart` with explicit sizes, owning values, labels, units and missing
masks. Freeze checked shape invariants, storage order and finite-value
handling. Use immutable shared buffers only when ownership and thread/lifetime
behavior are tested. An unowned reference to a caller DataFrame is not the
persistent chart-data contract.

## 2. Compute/render boundary

Monthly compounding/calendar pivot, correlation and histogram bins belong to
headless functions. Colormap/domain decisions belong to the declared chart
contract; the adapter maps them to display primitives. Distinguish a missing
cell from a finite zero and a count from a density. Do not preselect a
normalization (such as always zero-centered) when it differs from the pinned
source; document intentional visual changes.

Alignment is explicit per series builder. Reuse the native alignment helper
only if its duplicate/order semantics match the required plot. Build chart
values once; redraws cannot mutate inputs or rerun financial kernels.

The Phase 88a schema references these neutral concepts without depending on
ImPlot. It can freeze its metadata/shape contract before all chart builders
exist; later phases implement against that contract rather than introducing
another chart hierarchy.

## 3. Build and visual checks

`FINCORE_BUILD_IMPLOT=ON` builds the adapter. The demo additionally needs
`FINCORE_BUILD_IMPLOT_DEMO=ON` and its GLFW/OpenGL dependencies. The registered
`ImPlotAdapterCompile` test builds the adapter; it does not launch a GUI.
Record these commands/results separately. Apply invalid-ImGui expected
failure only when relevant wiring changes.

Use deterministic demo fixtures for each chart, actual rendered screenshots
and a short visual checklist (labels/units, missing values, legend, scales,
layout and substitution). Record window size, renderer/dependency versions
and locale. Headless numerical tests remain primary for data correctness;
screenshots do not replace them. Missing display/dependencies leave the
appropriate result `BLOCKED/NOT_RUN` and preserve useful build evidence.

## 4. Sequence

72 establishes core chart ownership. 73/74/75 can then proceed independently
with disjoint source ownership or serialized adapter edits. Correlation in
76 follows 72; frontier follows 69. The Iteration 5 rendering dependency is
on the primitives it uses, not on unrelated frontier visualization.
