# Iteration 4 — Visualization: Requirements

**Phases:** 72–76. **Status:** Revised plan; NOT_RUN.
**Oracle:** Python `4b8f31b1b59707480d5db9603704d83bbf7e6946`.
The [shared delivery contract](../delivery-contract.md) applies.

## 1. Scope and entry

Extend the optional ImPlot adapter and headless chart-data builders.
Inventory the actual `fincore/viz/contracts.py:VizBackend`: `plot_returns`,
`plot_drawdown`, `plot_rolling_sharpe`, `plot_monthly_heatmap`. Bars,
histograms, correlation and frontier plots are additional selected backend/
factor capabilities, not extra methods of that four-method protocol.

Start after 39 and the specific accepted compute primitives. Existing
adapter APIs and caller-owned ImGui/ImPlot contexts remain valid. Phases
72–75 do not wait for all risk work; 76's frontier slice requires Phase 69.
Local GUI dependencies are needed only for adapter/demo checks.

## 2. Functional requirements

| Phase | Native delivery | Requirements |
|---|---|---|
| 72 | Monthly pivot and heatmap | Headless owning year×month values, explicit missing mask, labels, unit and color-domain policy; return compounding/resampling/calendar behavior against the pinned backend. Missing month is not zero return. |
| 73 | Multi-series and benchmark plots | Named series, benchmark styles/legend/zero line and declared alignment. Map `plot_returns` and `plot_rolling_sharpe` (including benchmark/window metadata) to existing/new native calls with evidence. |
| 74 | Drawdown fill | Consume the same validated drawdown series as the line plot; unit/sign and fill-to-zero semantics preserved. |
| 75 | Bars and distributions | Grouped labels, counts/density distinction, fixed/automatic-bin policy as scoped from oracle, NaN/empty/constant cases; headless bin computation. Record violin/QQ substitutions rather than implying feature identity. |
| 76 | Correlation/frontier | Owned labeled correlation matrix using shared heatmap representation; frontier chart consumes Phase 69's computed result. No solver in the adapter. |

Compute quantitative values, bins and missingness in the core. Adapter calls
map immutable chart data to ImPlot; they may perform UI layout/context work,
but do not re-estimate metrics or silently change the statistical input.

## 3. Ownership and verification levels

Chart objects own arrays/labels or retain immutable shared ownership. Any
view is internal and its lifetime bounded to a render call. Models used by
reports or async callers remain valid after source frames are destroyed.

Report three distinct results: `COMPUTE` (data/shape/numerics), `BUILD`
(adapter/demo compilation and linking), `VISUAL` (actual rendered output).
No display means VISUAL `NOT_RUN`, not a pass. A phase can record bounded
compute/build acceptance while visual acceptance is incomplete; the iteration
cannot claim visually verified functionality until the scoped visual checks
are complete. Do not assert GUI execution is impossible in all CI systems.

## 4. Exit

Protocol mapping and selected extras accounted for; headless core has no GUI
link dependency; positive adapter build and separate optional demo build
recorded; visual fixture evidence for selected plots retained. Render-neutral
chart types are available to Iterations 5–6 without ImGui headers. Each
fidelity difference is documented alongside the affected capability.
