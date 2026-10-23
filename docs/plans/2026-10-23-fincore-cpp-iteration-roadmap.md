# Fincore C++ Refactoring Iteration Roadmap (Post-Phase-38)

**Date:** 2026-10-23. **Revision:** R2, source-backed review.
**Status:** Revised delivery plan; no new implementation acceptance.
**Target:** Native C++ capabilities mapped to Python source
`4b8f31b1b59707480d5db9603704d83bbf7e6946` (v0.5.2.dev0).

## 1. Authority and current state

The [iteration index](../iterations/README.md),
[shared delivery contract](../iterations/delivery-contract.md), and
[capability map](../iterations/capability-map.md) define the program-level
scope and acceptance. Each iteration has requirements, design and acceptance
files. Phase designs refine those contracts without silently narrowing them.
The [review](../iterations/review-2026-10-23.md) records the source-backed
reasons for this revision.

Verified at review start: C++ `dev@6900250cfe3541e8a40d81963509e777e28f57b7`,
clean, one registered checkout at
`/Users/yunjinqi/Documents/new_projects/fincore_cpp`. Phase 38 code is already
committed in `e2038e0`, but the acceptance record/README entry is missing.
The recorded Phase 37 478/478 run is historical, not current-tree evidence.
The prior linked-worktree description and 206-unpushed-commit prerequisite
are retired as stale. No remote synchronization state was verified here.

Current dependencies are local DataFrame, optional local ImGui/ImPlot, and
GoogleTest FetchContent for tests. Native installation/export rules, a
repository CI matrix and benchmark harness are absent in the reviewed tree.
This review changes documentation only; it does not claim those features.

## 2. Delivery and parity policy

Follow requirement → phase design → scoped implementation → validation →
acceptance → README trace. Preserve historical exact contracts, especially
Phase 38. Phase 39 freezes source and environment manifests, a complete
capability ledger and benchmark protocol. Moving Python `dev` is monitored
as a change request; it never silently replaces the oracle pin.

Use the shared contract's structural/exact/numerical/solver/stochastic
comparison classes. Special functions, regression, reductions and scalar
optimizers can require justified tolerances before Phase 60. Same seed is
not a cross-language RNG equivalence claim. Iterative solutions require
residual/feasibility/failure checks, not merely close weights. A phase must
freeze numeric budgets and fixtures before implementation.

Keep scratch logs in unique `/tmp/fincore-phase<N>-*` directories and archive
required manifests/scripts/results durably before acceptance. Run the full
registered suite and sanitizer gates on the recorded candidate; reuse an
identical eligible run instead of repeating it for a different label.
Push, merge, tag and publication are distinct authorized actions, not
requirements for local numerical acceptance.

## 3. Six delivery iterations

| Iteration | Phases | Scope and exit |
|---|---|---|
| [1: metrics](../iterations/iteration-1-metrics-completion/requirements.md) | 38 close-out, 39, 40–52, 52b | Accept existing Phase 38; re-pin with before/after drift; correct ratio/statistic contracts; close residual metric ledger. Domain coverage is measured, not inferred from phase count. |
| [2: performance/portfolio](../iterations/iteration-2-performance-portfolio/requirements.md) | 53–59 + 57b | TWR/MWR/XIRR, cashflow timing, all inference entries, disclosure output, transaction liquidity and capacity assessment. No invented liquidity module. |
| [3: risk/optimization](../iterations/iteration-3-risk-optimization/requirements.md) | 60–71 | Capability-driven solver spike; GARCH/EVT/backtests/walk-forward/specs; constrained frontier/objectives/risk parity with failure and feasibility evidence. |
| [4: visualization](../iterations/iteration-4-visualization/requirements.md) | 72–76 | Render-neutral owned chart data and optional ImPlot; compute, compile/link, and visual runtime are separate gates. Frontier consumes Phase 69. |
| [5: factor analysis](../iterations/iteration-5-factor-analysis/requirements.md) | 77–87 | Panel/PIT/calendar contract; preparation and kernels before full model assembly; research/tears; offline report and measured reuse/scaling. |
| [6: report/runtime/delivery](../iterations/iteration-6-report-runtime/requirements.md) | 88a early; 88b–93 | Shared report contract before its consumers, HTML/XLSX, report builders, runtime catalog/session integration, native package/consumer/platform validation. |

Phase IDs are stable references, not a prescribed numeric execution order.
Phase 57b is now explicitly part of every Iteration 2 scope/sign-off table.
Phase 52b adds capability reconciliation rather than silently deleting
unlisted metrics. Phase 93 is new and was unused at this review.

## 4. Dependency checkpoints and parallel work

| Checkpoint | Prerequisites | Unblocks / acceptance outcome |
|---|---|---|
| 38 close-out | Existing Phase 38 plan/design and committed implementation | Full missing boundary matrix and fresh acceptance; code presence is insufficient |
| 39 baseline | 38 accepted under its original contract | Frozen oracle/environment, API ledger, drift resolution, benchmark baseline and platform decision |
| Scalar solver | 39; Phase 49 or 55 design | Bounded minimization for Stutzer and bracketed root finding for XIRR/MWR have distinct contracts; no dependency on future Phase 60 |
| 88a shared schema | 39 + report model inventory | Typed section/table/chart metadata, special values, ownership and serialization contract for 67/84/86; remains owned by Iteration 6 |
| 60 solver spike | 39 + fixed numerical comparison protocol | Measured support for actual constrained and unconstrained problem classes; implementation follows only a successful decision |
| 77 panel spike | 39 + fixed factor/PIT inventory | Sparse/unbalanced panel and calendar semantics, missingness, quantiles, ownership and memory budget |
| 82 factor portfolio/costs | 78 + the required Phase 80 performance kernels | Portfolio composition and separate gross/net/cost accounting before full analysis assembly |
| 79 analysis assembly | 78 + required 80/81/82 kernels | Compute once, immutable full model; consumers do not compute backward dependencies |
| 76 frontier chart | 69 + chart primitives | No adapter-side portfolio optimization |
| 86 factor HTML | 84 + 88a | Shared ReportDocument and serializers; no parallel private document framework |
| 93 native delivery | Scope reconciled across all iterations; 87 and runtime/report integration | Install/relocation/downstream consumer, supported matrix, benchmark and release-readiness evidence |

After 39, independent metric, portfolio, risk and chart slices may interleave.
Assign disjoint file ownership where several phases share `ratios.cpp` or
CMake lists. Converge shared interfaces before parallel implementation; avoid
merge churn from multiple agents editing the same leaf.

## 5. Scope boundaries

The native target does not promise Python import/ABI compatibility. Existing
capabilities and aliases are mapped rather than copied mechanically. The
carried-forward non-ports are PyMC-dependent Bayesian functionality (the
accepted Bayesian cone remains), network provider SDKs, Plotly/Bokeh-specific
backends, browser-based PDF output, and Python entry-point discovery.
These exclusions describe the selected scope; HTML is not claimed to be
feature-equivalent to PDF. See the exact disposition table in
[Iteration 6 requirements](../iterations/iteration-6-report-runtime/requirements.md).

Factor inference, calendar/PIT behavior, legacy route equivalents and newly
found public functions cannot be excluded merely because the old roadmap
omitted them. Phase 39 records the full ledger; owner phases either cover
those capabilities or record a dated explicit deferral. Any deferral narrows
the final parity claim. Do not register failing placeholder operations.

## 6. Principal risks and controls

| Risk | Control |
|---|---|
| Incorrect formula/API names copied from the old draft | Fixed-object source anchors and golden characterization before phase implementation |
| Infinite re-pin churn | Immutable manifest; upstream movement recorded without automatic migration |
| Impossible numerical acceptance | Algorithm-specific error budgets, fixed-input kernel tests, residuals and declared reproducibility boundary |
| Missing surface hidden by a "complete" label | Capability ledger with aliases, consumer scenarios, gaps and dispositions |
| Panel densification / dangling views / look-ahead | Phase 77 ownership, sparse-layout benchmark, membership masks and PIT fixtures |
| Solver cannot express constraints | Phase 60 capability matrix, infeasible/nonconvergent fixtures, convergence status contract |
| Report/model integration arrives too late | 88a shared schema checkpoint before 67/84/86; kernels before 79 |
| Compile-only visualization mistaken for UI acceptance | Independent visual runtime status with fixtures and screenshots |
| Temporary evidence disappears | Durable artifacts and checksums, not only `/tmp` paths |
| C++ rewrite lacks usable package or measured speedup | Phase 39 baseline and Phase 93 installed downstream consumer/platform gates |

## 7. Completion and revision record

An iteration closes only when all its phase rows have scoped evidence at
identified candidates, coverage has no unexplained gaps, and the relevant
full-suite/sanitizer/integration checks pass. Program closure additionally
requires the master capability reconciliation, native distribution evidence
and explicit remaining exclusions. Documentation completion is not product
completion, live financial validation, platform certification or publication.

- Initial proposal: eight iterations 0–7, later grouped into six.
- 2026-10-23 R2: synchronize six-iteration numbering; replace stale topology;
  correct Python APIs/formulas and dependencies; introduce shared delivery
  contracts, coverage reconciliation, early report schema, measured
  performance and Phase 93 distribution gate. Preserve historical phase IDs
  and acceptance records; no code acceptance or Git delivery performed.

## Amendment 2026-10-23 — Layout alignment to Python package paths (owner-approved)

After Iteration 3 completes, run a dedicated mechanical refactor phase that
moves the three historical leaves currently under `metrics/` to the
`portfolio/` tree, matching the Python package layout:

- `include/fincore/metrics/positions.hpp` + `src/metrics/positions.cpp`
  → `include/fincore/portfolio/positions.hpp` + `src/portfolio/positions.cpp`
- `include/fincore/metrics/transactions.hpp` + `src/metrics/transactions.cpp`
  → `include/fincore/portfolio/transactions.hpp` + `src/portfolio/transactions.cpp`
- `include/fincore/metrics/round_trips.hpp` + `src/metrics/round_trips.cpp`
  → `include/fincore/portfolio/round_trips.hpp` + `src/portfolio/round_trips.cpp`

Rules: no compatibility alias headers (the project forbids aliases); update
every include, CMake source entry, and affected test; the public function
signatures are unchanged; the refactor must pass the full CTest suite and
ASan+UBSan at its head and be recorded as a dedicated acceptance record.
Tests stay flat under `tests/unit/` (deliberate difference from Python's
mirrored test tree). The internal `detail/optimization` kernels and the
`viz/implot_backend` adapter remain non-Python-parallel by design.
