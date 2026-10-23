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
  consumers; full headless CTest **721/721** passed at the final HEAD.
- The Release verification tree leaves `FINCORE_BUILD_IMPLOT=OFF`. A separate
  Ninja Release build with `FINCORE_BUILD_IMPLOT=ON` compiles and links
  `fincore_implot` at this HEAD (rc 0,
  `/tmp/fincore-implot-build/libfincore_implot.a`); the GLFW demo is not
  built, and no rendering was exercised.
- Visual verification is **NOT_RUN**: no screenshot or human visual review
  is claimed.
- Sanitizer: full ASan+UBSan suite **722/722 passed** at the final HEAD
  (Homebrew LLVM 22.1.8, Ninja Debug).

## Open items

Actual on-screen rendering, offline image capture and ImPlot adapter
installation with the package remain open. The compute and build halves are accepted; the visual half is
explicitly unverified.
