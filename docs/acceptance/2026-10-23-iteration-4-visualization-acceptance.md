# Iteration 4 Acceptance — Visualization

## Scope

Adds the render-neutral chart-data core (`chart_data.hpp/.cpp`) and extends
the optional ImPlot adapter and demo for monthly heatmaps, series/benchmark
charts, drawdown fills, bars, histograms, correlation matrices and the
computed efficient frontier. Two core paths, two adapter/demo paths and one
test TU changed/added; no new dependency.

## COMPUTE parity

Oracle: `/tmp/fincore-phase72-python.xZUSeB`, detached at `4b8f31b`, Python
3.11.8 / NumPy 1.26.4 / pandas 3.0.3, exit `0`, empty stderr, clean
pre-status. Script SHA-256
`cc93918cd881df9fe67048174401d9047b5fddcd09b7d7a615157682fd7e3dce`; raw
output SHA-256
`24e009a9b395f46ddf9fe47e4b4fc2e865e86e95910d4e75a00a0b8acb51d2c1`.

- Monthly heatmap: 55-day compounding matches Jan `0.560685325129362`, Feb
  `-0.7060026454676072`, Mar `0.07200391792694383`; constant `32.12909668982513`
  with ±1 color expansion; negative `-26.02996266117198`; empty grid has
  color `(-10,10)` and centered flag.
- Correlation matches pandas pairwise semantics
  (`0.42426406871192857`, `0.22874785549890703`, `-0.282842712474619`),
  with NaN for the constant column including its diagonal.
- Histogram counts/edges/density match NumPy's fixed-bin semantics; the
  constant 3-bin case reproduces `[0,3,0]` and `[-0.49, -0.156666…,
  0.176666…, 0.51]`.
- Series/returns/rolling-Sharpe/drawdown charts carry owned index/values,
  labels, benchmark flags, zero-line/fill flags and window metadata.
- Frontier charts consume computed vectors/markers; no solver runs in the
  chart layer.

## Declared fidelity differences

- **Missing month:** pandas resample emits `0.0` for an empty month group;
  native marks that month missing (`present=false`, NaN). This follows the
  iteration requirement that a missing month is not a zero return. The
  gap-month fixture therefore has native color `[0.98, 6.027…]` rather than
  the source's zero-inclusive `[0, 6.027…]`.
- **Heatmap input:** the single-return-series compute route is ported; the
  matplotlib path that treats an arbitrary DataFrame as already-monthly
  values is not exposed.
- **Timezone:** monthly grouping is UTC calendar; pandas groups by the
  index's local timezone.
- **Benchmark style:** the pinned ImPlot revision exposes line color/weight
  but not a dashed-line property; the adapter uses gray/lighter style for
  benchmarks instead of a dash pattern.
- **Bars/histograms:** native selected extras (NumPy-compatible bins), not
  methods in the pinned `VizBackend` protocol.
- **Heatmap cells:** the headless mask is authoritative; the adapter passes
  NaN cells to ImPlot without claiming a specific missing-cell color.

## BUILD evidence

- Core release build succeeded.
- `FINCORE_BUILD_IMPLOT=ON` adapter build succeeded (`fincore_implot`).
- `FINCORE_BUILD_IMPLOT_DEMO=ON` demo configured (GLFW/OpenGL found) and
  `fincore_implot_demo` linked successfully.
- Core CTest: **668/668 passed**.
- ImPlot-tree CTest (includes `ImPlotAdapterCompile`): **669/669 passed**.

## VISUAL

**NOT_RUN.** No automated screenshot harness was used in this session;
capturing the host desktop is not accepted as visual evidence. The demo now
calls every adapter path so a later visual pass can record labels/scales/
missing-cell behavior in a controlled fixture.

## Sanitizer

The sanitizer re-run was interrupted by the owner during this delivery, so
this record makes **no sanitizer claim**. The last completed sanitizer run
before the visualization changes was 661/661; the Iteration 4 sanitizer gate
remains open until re-run.

## Test coverage

`chart_data_test.cpp` (7 tests): monthly compounding/missing/constant/
negative/empty/two-year; series metadata and ownership; bars; histograms;
correlation; frontier shape/error cases.

## Limits

This is a compute/build delivery, not a visually verified renderer. No
colorbar, interactive zoom, export, or visual regression fixture is claimed.
