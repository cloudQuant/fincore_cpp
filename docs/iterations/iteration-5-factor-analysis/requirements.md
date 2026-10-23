# Iteration 5 — Factor Analysis: Requirements Specification

**Iteration ID:** iteration-5-factor-analysis
**Phase range:** Phases 77–87
**Source of truth:** fincore Python `dev@4b8f31b`: `fincore/factor_analysis/` (incl. `research.py`), `fincore/report/factor/`
**Status:** Draft

> Roadmap amendment 2026-10-23: former Iteration 6 (factor analysis)
> renumbered to 5 after the risk/optimization merge.

## 1. Purpose and Scope

Port the factor-analysis domain — the largest single block of the port and
the area where the Python oracle is **actively changing**. Scope: factor data
preparation (quantile bucketing, forward returns), the compute-once analysis
model, factor performance (mean returns by quantile, IC, turnover including
the newest quantile-turnover/long-universe features), factor portfolio
construction, the plain-frame research-evaluation entry, tear-sheet compute
models, ImPlot tear-sheet rendering, and the bilingual offline-ECharts HTML
factor report.

## 2. Entry Conditions

- Iteration 1 accepted (drawdown table, stats kernel).
- Iteration 4 accepted (bar/histogram/heatmap/line chart primitives) for
  Phase 85.
- **Oracle drift checkpoint:** re-check fincore `dev` head immediately before
  Phase 77. If past `4b8f31b`, insert a re-pin phase (Phase 39 procedure)
  first. Factor analysis is where drift bites hardest — never port against a
  moving target.

## 3. Functional Requirements

### 3.1 Panel adapter (mandatory design gate)

| Phase | Requirement |
|---|---|
| 77 | Establish the wide-table convention (UTC-ns date index, one double column per asset) and a thin, explicitly-owned `factor_panel` layer over `StdDataFrame<std::int64_t>` providing: cross-section iteration (date → asset-value row view), per-asset column iteration, quantile bucketing, and forward-return computation. **No factor metric lands in this phase.** The design doc locks the data-shape contract every later phase builds on. Acceptance = compile+test-gated adapter with parity tests on bucketing/forward-return primitives against pandas `qcut`/`shift` oracles. |

### 3.2 Core factor phases

| Phase | Public API | Python source | Requirement summary |
|---|---|---|---|
| 78 | `prepare_factor_data` | `factor_analysis/data.py` | Factor quantization into buckets + forward returns on the Phase 77 adapter; bucket-edge and label semantics parity (incl. duplicate-drop and NaN handling). |
| 79 | `analyze_factor` | `factor_analysis/analysis.py` | Compute-once assembly of the full factor model — frozen value type aggregating all downstream-computable sections. |
| 80 | factor performance | `factor_analysis/performance.py` | Mean returns by quantile, IC by group; exact aggregation semantics (equal/cap weighting per oracle). |
| 81 | turnover kernels | `factor_analysis/performance.py` | Turnover incl. **quantile turnover + long universes** (commit `9027733`); mirror the vectorized kernel structure of the Python side. |
| 82 | factor portfolio construction | `factor_analysis/portfolio.py` | Weights and capacity-aware positions. |
| 83 | `evaluate_factor_research` / `ResearchEvaluationSpec` | `factor_analysis/research.py` (`5399498`) | Plain-frame research evaluation entry: validates UTC-indexed wide frames, evaluates periods, stability, correlations; emits CSV + chart artifacts (artifact paths, not rendering, in the compute model). |
| 84 | tear-sheet compute models | `factor_analysis/models.py` | Frozen renderer-ready models (`FactorTearSheetArtifacts` equivalent) — compute only, no rendering. |

### 3.3 Rendering phases

| Phase | Public API | Python source | Requirement summary |
|---|---|---|---|
| 85 | ImPlot tear sheets | `factor_analysis/tears.py`, `render_matplotlib.py` | Summary/returns/information/turnover/event tear sheets as multi-panel ImPlot compositions of Iteration 4 primitives. Grid layout parameters from the oracle's `GridFigure`; violin → histogram/bar substitution with fidelity notes. |
| 86 | bilingual factor HTML report | `report/factor/html.py` (`7679c71`) | Self-contained zh/en HTML embedding offline ECharts (`echarts.min.js` ported verbatim as an asset): line charts, bar charts, histogram bins, KPI cards, coverage tables — built from the frozen report model with no recomputation. |
| 87 | factor report perf parity | commits `280c28c`, `dcc1c89`, `e04bd08`, `fc512cd` | Mirror the weight/snapshot-reuse semantics so large-fixture behavior (result fingerprints streamed, model snapshots reused, weights reused within one analysis) matches; a performance-parity phase, not a new feature. |

### 3.4 Cross-cutting requirements

- **MultiIndex prohibition:** no phase may invent its own panel
  representation; all factor data flows through the Phase 77 `factor_panel`
  layer. Any discovered gap in the adapter is fixed **in the adapter** with
  its own parity test, not worked around in a consumer.
- Rendering phases (85–86) consume frozen value types only; the compute/render
  split from Iteration 4's `chart_data.hpp` pattern extends here.
- PIT (point-in-time) contracts (`pit.py`) and calendar semantics
  (`calendar.py`, CustomBusinessDay): enumerate at Phase 78 design time and
  port the minimal subset the ported functions actually consume — record the
  subset decision.

## 4. Non-Functional Requirements

- Hex-float parity for all closed-form aggregation paths; tolerance policy
  not expected to be needed (no iterative solvers in this domain) — any
  exception must be declared in that phase's design.
- Determinism: identical inputs → bit-identical models and report bytes.
- Standard SDD gates; evidence conventions unchanged.

## 5. Exit Criteria

- Phases 77–87 accepted.
- `factor_analysis` public surface at parity for the ported subset, with the
  PIT/calendar subset decision recorded; anything excluded is listed with a
  reason in the final acceptance record.
- The bilingual HTML report for the design-doc fixture is byte-stable across
  two runs (determinism test) and structurally validated against the Python
  oracle's report (same sections, same values within parity policy).

## 6. Out of Scope

- `factor_analysis/inference.py` statistical extras beyond what ported
  functions consume (enumerate at 78; exclude with reasons).
- Plotly/bokeh factor rendering (non-port; ECharts + ImPlot are the targets).
- Any new data provider for factor inputs.
