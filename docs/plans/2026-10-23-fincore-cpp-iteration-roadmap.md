# Fincore C++ Refactoring Iteration Roadmap (Post-Phase-38)

**Date:** 2026-10-23
**Status:** Proposed roadmap — not yet accepted as SDD plan of record
**Scope:** Multi-iteration plan taking fincore-cpp from Phase 38 to parity with
fincore Python `dev` head `4b8f31b` (v0.5.2.dev0).

> **Amendment 2026-10-23 — Reorganization into six delivery iterations.**
> This roadmap originally numbered eight iterations (0–7). Delivery is now
> organized as **six iterations**, each with its own folder under
> [`docs/iterations/`](../iterations/README.md) containing a requirements
> specification, a design document, and an acceptance document:
> former Iteration 0 (close-out + re-pin) is folded into Iteration 1 as
> preparatory phases (38 close-out, 39 re-pin) plus entry conditions; former
> Iterations 3 (risk) and 4 (optimization) are merged into Iteration 3 (shared
> Phase 60 solver decision, shared tolerance-parity regime); former
> Iterations 5/6/7 renumber to 4/5/6. Phase numbers are unchanged. The
> sections below keep the original iteration numbering for traceability; the
> authoritative delivery structure is `docs/iterations/README.md`.

## 1. Current State Snapshot

- Implementation lives in the linked worktree
  `.worktrees/fincore-cpp-foundation` (branch
  `codex/fincore-cpp-foundation-impl`, 273 commits, **206 unpushed**).
- Phases 2A–37 accepted; 478 parity tests green at the Phase 37 acceptance
  head. Phase 38 (adjusted Sharpe ratio) has design + plan committed and
  implementation present as uncommitted working-tree changes pending its
  clean-oracle acceptance run.
- Parity baseline is pinned to Python fincore `2354bb5`; the Python `dev`
  branch has advanced to `4b8f31b` with factor-analysis work (quantile
  turnover, long universes, `evaluate_factor_research`, bilingual offline
  ECharts factor report) and performance work (weight/snapshot reuse).
- Dependencies: local cloudQuant/DataFrame (required, linked), local
  ImGui/ImPlot (optional adapter, compile/link-only so far), GoogleTest via
  FetchContent. No numerical solver dependency exists yet.

## 2. Methodology (unchanged, restated as binding)

Every phase below is executed as the established SDD loop:

1. `docs/plans/<date>-phase-N-*.md` — narrow plan with exact-signature
   constraints and the exact-index commit gate.
2. `docs/design/<date>-phase-N-*-design.md` — design plus a clean detached
   Python oracle run (Anaconda base interpreter, pinned fincore checkout)
   capturing hex-float goldens and edge-case result classes.
3. Implementation in `include/fincore/` + `src/`, tests in the two registered
   per-module translation units.
4. Acceptance gates: clean-tree AppleClang build + full CTest, Homebrew LLVM
   22 ASan+UBSan run, ImPlot compile/link-only evidence (when viz code
   changes), invalid-ImGui expected-failure build (when ImPlot wiring changes),
   then `docs/acceptance/<date>-phase-N-*-acceptance.md` and README ledger
   entry.

Constraints carried into every phase: no aliases, no facade includes, no
automatic-input conveniences, embedded-NUL selector rejection before DataFrame
access, structural-validation precedence, scalar ownership after input
destruction, evidence outside the repo under `/tmp/fincore-phaseN-*` roots.

**Parity policy amendment (new, applies from Phase 60 onward):** exact
hex-float parity is required for closed-form statistics. For
optimizer-dependent algorithms (GARCH MLE, efficient frontier, risk parity),
bit-exact parity with SciPy is not achievable; those phases instead lock
(a) a tolerance-bounded parity band against the Python oracle on fixed
fixtures, (b) exact parity of all closed-form intermediates (e.g. per-iteration
gradients, log-likelihood evaluations), and (c) deterministic seeding so the
C++ result is itself reproducible bit-for-bit across runs. Each such design
doc must state its tolerance, the fixture, and the convergence-criterion
parity contract explicitly.

## 3. Iteration 0 — Close-Out and Baseline Re-Pin (immediate)

Not new metrics; hygiene that must land before any new phase.

| Item | Description | Exit criteria |
|---|---|---|
| 0.1 | Finish Phase 38 acceptance: clean detached Python oracle run, CTest + ASan/UBSan gates, commit working-tree `adjusted_sharpe_ratio` code, acceptance record + README ledger entry | Phase 38 accepted; clean tree |
| 0.2 | Push the 206 unpushed impl commits to origin as backup (user-authorized push) | `codex/fincore-cpp-foundation-impl` pushed; no force |
| 0.3 | Reconcile doc checkout vs impl worktree topology: decide whether the main checkout's docs-only branch merges the impl branch or remains a docs mirror; record the decision in the roadmap and README | Written decision; no silent divergence |
| 0.4 | **Phase 39 — Oracle re-pin:** rebuild parity goldens against fincore `dev@4b8f31b` in a fresh detached checkout; re-run the full C++ suite; classify any golden drift (expected: none for metrics already ported — fincore changes since `2354bb5` touched factor_analysis/report only, but this must be measured, not assumed) | All 478+ tests green against the new pin, or every drift item classified and resolved; design doc records old/new pin SHAs |

## 4. Iteration 1 — Metrics Completion (Phases 40–52)

Continue the one-narrow-ratio-per-phase cadence. Order is by rising complexity
and dependency; numbers are provisional.

| Phase | API (Python source) | Notes |
|---|---|---|
| 40 | `mar_ratio` (`metrics/ratios.py`) | |
| 41 | `conditional_sharpe` (`metrics/ratios.py`) | needs VaR interplay — reuse native `value_at_risk` |
| 42 | `deflated_sharpe_ratio` (`metrics/ratios.py`) | needs normal CDF/inverse — add private stats helper, no public dependency |
| 43 | `burke_ratio` (`metrics/ratios.py`) | |
| 44 | `treynor_ratio` (`metrics/ratios.py`) | reuse native beta |
| 45 | `modigliani_ratio` (M², `metrics/ratios.py`) | |
| 46 | `sterling_ratio` (`metrics/ratios.py`) | |
| 47 | capture-return slices (`metrics/ratios.py`) | up/down/up-down capture *return* distinct from existing capture ratios |
| 48 | `hurst_exponent` (`metrics/stats.py`) | rescaled-range; specify window policy from oracle |
| 49 | `stutzer_index` (`metrics/stats.py`) | |
| 50 | drawdown table machinery (`metrics/drawdown.py`) | top-N drawdown table, underwater series, recovery days/weeks/months — one phase for the table assembly, not each column |
| 51 | `extract_interesting_date_ranges` (`metrics/timing.py`) | crisis-period definitions ported as static data |
| 52 | `perf_stats` bootstrap variants (`metrics/perf_stats.py`) | reuse existing simulation/bootstrap primitives |

**Iteration exit:** `fincore.metrics` public surface at full parity (bayesian
submodule excepted, see §10). Yearly×month heatmap pivot (used by viz) lands
with Iteration 5's heatmap phase.

## 5. Iteration 2 — Performance (GIPS) and Portfolio Completion (Phases 53–59)

All closed-form or tolerance-free numerics; straightforward parity.

| Phase | API (Python source) | Notes |
|---|---|---|
| 53 | `performance.returns.twr` | time-weighted return with sub-period geometric chaining |
| 54 | `performance.returns.mwr` | money-weighted (IRR-based) |
| 55 | `performance.returns.xirr` | needs a root finder — private bisection/Brent port; tolerance-parity policy from §2 applies if Python uses SciPy's solver |
| 56 | `performance.cashflows.cashflow_adjusted_returns` | |
| 57 | `performance.inference.sharpe_confidence_interval` | |
| 58 | `portfolio.capacity` module | capacity sweep, `get_max_commission/_slippage`-style entry points |
| 59 | `portfolio.liquidity` module | |

## 6. Iteration 3 — Risk Models and the Solver Decision (Phases 60–67)

**Phase 60 is a spike/design phase, not a metric phase.** It selects the
numerical optimization strategy for everything downstream (GARCH MLE here,
portfolio QP in Iteration 4):

- Option A: vendor a local optimizer checkout (OSQP for QP + a small
  unconstrained solver) via the established `FINCORE_<LIB>_SOURCE_DIR` CMake
  pattern, mirroring DataFrame/ImGui.
- Option B: hand-port Nelder-Mead + L-BFGS (bounded scope, no new dependency).

Selection criteria: parity feasibility against SciPy within §2 tolerance
policy, build-system friction across the four gate configurations, and
maintenance cost. The decision is recorded in the Phase 60 design doc and is
binding for Iterations 3–4.

| Phase | API (Python source) | Notes |
|---|---|---|
| 60 | Solver spike + decision | design doc only + build-wiring proof |
| 61 | GARCH(1,1) fit (`risk/`) | tolerance-parity policy applies |
| 62 | GARCH family variants present in `risk/` | scope frozen by oracle scan at design time |
| 63 | GPD/EVT risk estimates (`metrics/risk.py` + `risk/`) | |
| 64 | `beta_fragility_heuristic` (`metrics/risk.py`) | |
| 65 | VaR/ES backtests (`risk/`) | |
| 66 | Walk-forward validation (`risk/diagnostics.walk_forward_var`) | |
| 67 | `RiskModelSpec` + risk validation report compute model | rendering deferred to Iteration 7 |

## 7. Iteration 4 — Optimization (Phases 68–71)

Uses the Phase 60 solver decision.

| Phase | API (Python source) | Notes |
|---|---|---|
| 68 | Optimizer integration + objective plumbing | no public API change beyond what 60 chose |
| 69 | `optimization.frontier.efficient_frontier` | tolerance-parity |
| 70 | `optimization.objectives.optimize` (max-Sharpe, min-vol) | |
| 71 | `optimization.risk_parity.risk_parity` | |

## 8. Iteration 5 — Visualization Completion (Phases 72–76)

Completes the ImPlot adapter toward the Python `VizBackend` protocol surface.
All phases stay compile/link-only gated (headless CI); the GLFW demo target is
the manual-run vehicle. No GUI runtime enters the core library.

| Phase | Function | ImPlot mapping |
|---|---|---|
| 72 | `plot_monthly_heatmap` | ImPlot heatmap + diverging colormap + colorbar; needs the year×month pivot (compute side lands here) |
| 73 | multi-series returns + benchmark overlay | `PlotLine` ×N + dashed benchmark + legend |
| 74 | drawdown underwater fill | `PlotShaded` (or filled line) |
| 75 | bar + histogram/distribution plots | `PlotBars`, `PlotHistogram` — groundwork for factor tear sheets |
| 76 | `plot_correlation_matrix` + `plot_efficient_frontier` | heatmap reuse + scatter/line frontier |

**Iteration exit:** the C++ viz adapter exposes the full non-factor
`VizBackend` protocol set.

## 9. Iteration 6 — Factor Analysis (Phases 77–87) — largest block

**Key architectural risk:** fincore's factor code is MultiIndex
`(date, asset)`-heavy (110 groupby/xs/unstack call sites). `hmdf::StdDataFrame`
is single-index. Phase 77 resolves this once:

**Phase 77 — Panel adapter design (mandatory design phase before any factor
work).** Establish the wide-table convention (UTC-ns date index, one double
column per asset) and a thin, explicitly-owned `factor_panel` layer over
`StdDataFrame<std::int64_t>` providing: cross-section iteration (date →
asset-value row view), per-asset column iteration, quantile bucketing, and
forward-return computation. No factor metric lands in this phase; the design
doc locks the data-shape contract every later phase builds on, and the
acceptance is a compile+test-gated adapter with parity tests on
bucketing/forward-return primitives against pandas `qcut`/`shift` oracles.

| Phase | API (Python source) | Notes |
|---|---|---|
| 78 | `factor_analysis.data.prepare_factor_data` | quantiles + forward returns on the Phase 77 adapter |
| 79 | `factor_analysis.analysis.analyze_factor` | compute-once assembly model |
| 80 | `factor_analysis.performance` — mean returns by quantile, IC by group | |
| 81 | turnover kernels incl. **quantile turnover + long universes** (`9027733`) | latest fincore feature; mirror the vectorized kernel structure |
| 82 | `factor_analysis.portfolio` construction | weights, capacity-aware positions |
| 83 | `research.evaluate_factor_research` / `ResearchEvaluationSpec` (`5399498`) | plain-frame entry; CSV + chart outputs land as artifacts |
| 84 | tear-sheet compute models (`factor_analysis/models.py`) | frozen renderer-ready models, no rendering |
| 85 | ImPlot tear-sheet rendering | uses Iteration 5 primitives; multi-panel grid |
| 86 | bilingual (zh/en) factor HTML report with offline ECharts | ECharts is a static JS asset — C++ emits HTML/JS strings, no implot; port the asset verbatim |
| 87 | factor report perf parity (weight/snapshot reuse patterns) | mirror `280c28c`, `dcc1c89`, `e04bd08`, `fc512cd` semantics so large-fixture behavior matches |

**Iteration 6 is where oracle drift bites hardest:** fincore `dev` is actively
changing factor_analysis. Before starting Phase 77, re-check the fincore `dev`
head; if it has moved past `4b8f31b`, insert an oracle re-pin phase (same
procedure as Phase 39) rather than porting against a moving target.

## 10. Iteration 7 — Report and Runtime (Phases 88–92), plus explicit non-ports

| Phase | API (Python source) | Notes |
|---|---|---|
| 88 | `report` compute-once `ReportDocument` model | the "compute once, render many" spine |
| 89 | HTML renderer (`report/renderers/html.py`) | escaped tables, artifact bundle |
| 90 | XLSX renderer | minimal openpyxl-compatible writer; decide vendored lib vs. minimal OOXML emitter at design time |
| 91 | `report.portfolio.compute.build_portfolio_report` + risk validation report rendering | closes the loop from Phase 67 |
| 92 | `runtime` core: `OperationCatalog`, `OperationSpec`, `plan/run/batch`, `AnalysisSession` | mirror the compute-once architecture; NumPyBackend has no C++ analogue — the native path *is* the backend |

**Explicit non-ports (decided now, revisitable only by roadmap amendment):**

- `metrics/bayesian.py` (PyMC) — no C++ equivalent; the existing Bayesian
  return cone (Phase 28) is the extent of Bayesian support.
- `data/providers.py` network SDKs (yfinance/tushare/akshare/AlphaVantage) —
  heavy SDK/network surface with low C++ value. The `DataSnapshot`
  content-addressing *model* may be ported if a later phase needs it.
- `viz` plotly/bokeh backends — ImPlot is the designated interactive target;
  the HTML backend is covered by Iteration 7's HTML renderer.
- PDF renderer (Playwright/Chromium) — external browser dependency; HTML
  output is the portable equivalent.
- `extensions/` entry-point discovery — a Python packaging mechanism.

## 11. Risk Register

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| 206 unpushed commits lost | low, catastrophic | all impl work | Iteration 0.2 push, before anything else |
| Oracle drift (fincore `dev` moves during a long iteration) | high in Iteration 6 | parity churn | re-pin checkpoint before Phase 77 and between factor sub-phases; §2 policy |
| MultiIndex semantics lost in translation | medium | wrong factor results | Phase 77 adapter design gate with its own oracle tests before any metric |
| Solver parity mismatch (GARCH/optimization) | high | acceptance impossible under hex-float policy | §2 tolerance-parity amendment; Phase 60 decides solver before any dependent work |
| StdDataFrame capability gaps (no resample/rolling primitives) | medium | per-phase reimplementation | rolling kernels already follow the NumPy sliding-window pattern from `_rolling.py`; document each kernel in its phase design |
| Worktree/doc-checkout divergence | medium | lost acceptance records | Iteration 0.3 decision, recorded in README |
| ImPlot heatmap/colorbar gaps vs matplotlib | low | viz fidelity | degrade to nearest ImPlot primitive; fidelity notes in each viz phase acceptance |

## 12. Definition of Done (per iteration)

- Every phase in the iteration has plan → design (with clean-oracle run) →
  implementation → test hardening → acceptance record → README ledger entry,
  in commit order.
- Full CTest green and clean-tree AppleClang build at the iteration head;
  ASan+UBSan run recorded for the iteration head (not merely per phase).
- Any deviation from this roadmap (phase reordering, scope change, non-port
  reversal) is recorded as an amendment section in this document, dated, with
  the reason — never silently.

## 13. Sequencing Summary

```
Iter 0  close-out + re-pin            (Phase 38 accept, 39)
Iter 1  metrics completion            (40–52)
Iter 2  performance + portfolio       (53–59)
Iter 3  risk models + solver spike    (60–67)   ← solver decision gates Iter 4
Iter 4  optimization                  (68–71)
Iter 5  viz completion                (72–76)   ← can run in parallel with 3/4
Iter 6  factor analysis               (77–87)   ← largest; re-pin checkpoint first
Iter 7  report + runtime              (88–92)
```

Iteration 5 has no dependency on Iterations 3–4 and may interleave with them
if staffing allows; Iterations 1–2 are strictly sequential prerequisites for
nothing except their own content and can be reordered among themselves by
priority. Iteration 6 depends on Iterations 1 (drawdown table), 5 (viz
primitives), and nothing from 3–4.
