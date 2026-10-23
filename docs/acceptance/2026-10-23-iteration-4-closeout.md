# Iteration 4 Close-out — Visualization (Compute/Build Accepted, Visual NOT_RUN)

## Delivered

- Render-neutral visualization contracts and chart data (`viz/chart_data.hpp`):
  series, bar, histogram bins, correlation matrix, heat grid and frontier
  data.
- ImPlot adapter surface and demo source mapped to those data structures;
  GUI headers stay out of the core library.
- Acceptance record:
  [2026-10-23-iteration-4-visualization-acceptance.md](2026-10-23-iteration-4-visualization-acceptance.md).

## Evidence and gates

- Final Release tree builds the core, report and factor/risk/portfolio
  consumers; full headless CTest **718/718** passed at the final HEAD.
- The final verification build uses `FINCORE_BUILD_IMPLOT=OFF`; the adapter
  and demo are not compiled or linked in that tree, and no rendering was
  exercised.
- Visual verification is **NOT_RUN**: no screenshot or human visual review
  is claimed.
- Sanitizer: last complete ASan+UBSan run was 661/661 at the Iteration 3
  close; Iteration 4 changes were not sanitizer-tested.

## Open items

Actual on-screen rendering, offline image capture, ImPlot adapter installation
with the package, and a sanitizer run covering the visualization changes
remain open. The compute and build halves are accepted; the visual half is
explicitly unverified.
