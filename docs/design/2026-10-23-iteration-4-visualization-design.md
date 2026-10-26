# Iteration 4 Design — Visualization

## Source contract

`viz/contracts.py:VizBackend` exposes four methods: `plot_returns`,
`plot_drawdown`, `plot_rolling_sharpe`, `plot_monthly_heatmap`.
`matplotlib_backend.py` defines their display behavior:

- Returns: one cumulative-return line, zero horizontal line, title/y label.
- Drawdown: filled red area to zero plus a red line.
- Rolling Sharpe: portfolio line, optional gray dashed benchmark, legend,
  zero line, title with the window.
- Monthly heatmap: a `pd.Series` is resampled by month (`ME`) and compounded
  `(1+x).prod()-1`, scaled to percent, pivoted year×month, reindexed to
  months 1–12 with labels Jan–Dec; a `DataFrame` is treated as already
  monthly values. Color domain is the finite min/max, `(-10,10)` when
  non-finite, expanded by ±1 when constant, and zero-centered only when
  `min < 0 < max`.

Bars, histograms, correlation matrices and frontier charts are selected
extras, not methods of the four-method protocol. `optimization/frontier.py`
provides computed frontier points consumed by a frontier chart.

## Native chart-data model

`include/fincore/viz/chart_data.hpp` is render-neutral (no ImGui/ImPlot):

- `heat_grid`: owning `years`, twelve `columns`, row-major `values`
  (NaN for missing), a `present` mask, `unit="percent"`, and
  `color_min/color_max/color_centered`.
- `series_chart`/`chart_series`: title, y label, zero-line and fill flags,
  window metadata; each series owns its index, values, label, benchmark and
  dashed flags.
- `bar_chart`: group and series labels plus row-major values.
- `histogram_bins`: uniform edges, counts, density and observation count.
- `correlation_matrix`: labels, row-major values (NaN when undefined),
  presence mask and `unit="correlation"`.
- `frontier_chart_data`: computed volatilities/returns/Sharpe plus
  min-variance and max-Sharpe markers and axis labels.

## Compute decisions

- Monthly grouping is by **UTC calendar month** from the native timestamp;
  the index must be ascending (duplicates compound in order).
- The native builder **marks an empty month missing** instead of reproducing
  pandas' empty-resample group value `0.0`. The requirement explicitly says
  a missing month is not a zero return; this is a declared fidelity
  divergence and the `present` mask is authoritative.
- A month containing NaN or a non-finite compounded value is marked missing;
  finite monthly returns are multiplied by 100.
- Series charts do not align across series; each series keeps its own index.
  Benchmark series are flagged and styled (gray in the ImPlot adapter; this
  ImPlot revision has no dashed line property, so color/weight substitutes
  for the matplotlib dash and is documented).
- Histogram: fixed bin count, numpy-compatible equal-width bins over
  `[min,max]`, ±0.5 expansion for constant data, last edge inclusive;
  non-finite observations are dropped and must leave at least one value.
  Density is `count / (n * width)`; counts and density are both stored.
- Correlation: pairwise-finite Pearson over each pair of double columns;
  fewer than two pairs or zero variance yields NaN (including the diagonal
  for a constant column), matching pandas `corr` semantics for these cases.
- Bars require row-major `groups × series` finite values.
- Frontier charts validate equal vector lengths and finite markers; no
  solver is called in the chart layer.

## ImPlot adapter

`include/fincore/viz/implot_backend.hpp` gains `plot_monthly_heatmap`,
`plot_series_chart`, `plot_drawdown_chart`, `plot_bar_chart`,
`plot_histogram`, `plot_correlation_matrix` and `plot_frontier_chart`.
They map immutable chart data to ImPlot primitives (`PlotHeatmap`,
`PlotLine`, `PlotScatter`, `PlotShaded`, `PlotBarGroups`, `PlotBars`) with
month/year ticks, labels and axis titles. No metric is recomputed and no
chart data is mutated. Missing heatmap cells remain NaN; a visual missing
color is not asserted without a rendered check. The adapter remains optional
(`FINCORE_BUILD_IMPLOT`), and the core has no GUI include.

## Oracle evidence

Root `/tmp/fincore-phase72-python.xZUSeB`, detached at `4b8f31b`, Python
3.11.8 / NumPy 1.26.4 / pandas 3.0.3, exit `0`, empty stderr, clean
pre-status. Script SHA-256
`cc93918cd881df9fe67048174401d9047b5fddcd09b7d7a615157682fd7e3dce`; raw
output SHA-256
`24e009a9b395f46ddf9fe47e4b4fc2e865e86e95910d4e75a00a0b8acb51d2c1`.

- 55-day series from 2021-01-15: Jan `0.560685325129362`, Feb
  `-0.7060026454676072`, Mar `0.07200391792694383`, remaining months
  missing; color `[-0.7060026454676072, 0.560685325129362]`, centered.
- Jan/Mar gap fixture: pandas emits Feb `0.0` from the empty resample group;
  **native emits missing**. Jan `0.9800000000000031`, Mar
  `6.0273760000000065`; native color `[0.9800000000000031,
  6.0273760000000065]`, not centered (the source's zero would have set the
  minimum to 0).
- Constant February: `32.12909668982513`, color ±1 around it; negative
  April: `-26.02996266117198`, color ±1; empty: years/values empty, color
  `(-10,10)`, centered.
- Correlation fixture (A/B with one NaN, C, constant D): A-B
  `0.42426406871192857`, A-C `0.22874785549890703`, B-C
  `-0.282842712474619`, D all NaN; diagonals 1 except D.
- Histogram 4 bins: counts `[2,1,3,3]`, edges
  `[-0.03,-0.015,0,0.015,0.03]`, density `[14.8148…, 7.4074…, 22.2222…,
  22.2222…]`; constant 3-bin edges `[-0.49,-0.156666…,0.176666…,0.51]`,
  counts `[0,3,0]`.

## Verification levels

- **COMPUTE:** chart-data tests vs the oracle above.
- **BUILD:** core `fincore` with `chart_data.cpp`, `FINCORE_BUILD_IMPLOT=ON`
  adapter, and `FINCORE_BUILD_IMPLOT_DEMO=ON` demo (GLFW/OpenGL found).
- **VISUAL:** NOT_RUN. No automated screenshot harness is used in this
  session; capturing the host desktop is not an acceptable substitute.
  The demo exercises every adapter call for a later visual pass.

## Tests

`chart_data_test.cpp` (7 tests): heatmap compounding/missing/constant/
negative/empty/two-year; series/returns/rolling-Sharpe/drawdown metadata and
ownership; bar order/validation; histogram counts/density/constant/NaN/empty;
correlation pairwise semantics; frontier chart shape/marker validation.
