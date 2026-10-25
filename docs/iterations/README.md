# Fincore C++ — Iteration Plan Index

## Comments and bilingual documentation plan — 2026-10-25

[Iteration 10: Google-style comments and bilingual online documentation](iteration-10-comments-bilingual-docs/README.md)
plans P126–P134 for source comments, English/Chinese product and API documentation,
and publication on Read the Docs and GitHub Pages. This is a **plan-only** delivery;
implementation and publication remain **NOT_RUN**. Suspected defects are recorded
for review and separate fixes; this planning round changes no code or configuration.

## Performance delivery closeout — 2026-10-25

[Iteration 9: at least 30x, aiming for 100x end-to-end performance](iteration-9-performance-30x-100x/README.md)
implemented bounded rounds through P125. Its [requirements](iteration-9-performance-30x-100x/requirements.md),
[design](iteration-9-performance-30x-100x/design.md) and
[acceptance protocol](iteration-9-performance-30x-100x/acceptance.md) define the
same-CPU, complete-output comparison and evidence-driven plan revisions.
The user requested closeout at the current performance level. The 500-file
development batch reached 41.20x; 1,000/2,000-file diagnostic pairs reached
34.46x/35.63x. The original two-batch formal gate remains incomplete, including
the noisy full-corpus attempt. Further tuning and remaining performance gates
are deferred; see the [closeout record](iteration-9-performance-30x-100x/closeout.md).

Iteration 8's current execution state is in its
[2026-10-24 acceptance record](../acceptance/2026-10-24-iteration-8-native-parity-acceptance.md).
The remainder of this index is the historical R2 planning snapshot; its missing
Iteration 7/shared-contract references are not prerequisites of Iteration 9.

## Historical R2 index

**Revision:** 2026-10-23 R2, source-backed plan review.
**Status:** Current selected native scope accepted on 2026-10-24, with
explicit partial models, deferrals, historical variances and performance tradeoffs.
Iteration 7 (Phases 94–99) is an additional plan, reviewed on 2026-10-24;
its implementation and acceptance remain **NOT_RUN**. See its
[source-backed review](iteration-7-latest-fincore-alignment/review-2026-10-24.md).

Current execution evidence is tracked in the
[2026-10-23 candidate acceptance](../acceptance/2026-10-23-revised-plan-candidate-acceptance.md),
with [route-level scope decisions](../acceptance/2026-10-23-revised-plan-scope-decisions.md)
and the [completion work record](../plans/2026-10-23-revised-plan-completion.md).
The planning snapshot below is historical; it does not describe the current
dirty candidate or its newly completed tests. Current evidence is macOS
Release 850/850, ASan+UBSan 849/849, x86_64 Ubuntu QEMU
849/849, fresh clean-build relocated consumers, and user-accepted bilingual
offline HTML. The ledger is reconciled with exact DEFERRED/NON_PORT and partial
rows. Benchmark semantics pass; its raw performance FAIL and accepted
tradeoffs remain in the candidate record. No full Python-package parity is claimed.

The original R2 port retains **six delivery iterations** and the existing
Phase 38–92 identifiers. Iteration 7 extends that historical program with
latest-pin factor alignment and performance work. Execution follows
dependencies rather than phase-number order.
R2 adds Phase 52b (residual metric coverage), splits Phase 88 into an early
88a shared-report contract and 88b completion, and adds Phase 93 (native
package/consumer validation). No existing phase or acceptance record is erased.

Start with the [review findings](review-2026-10-23.md), the
[shared delivery contract](delivery-contract.md), and the
[capability map](capability-map.md). These are part of every iteration.

| Iteration | Requirements | Design | Acceptance | Delivery |
|---|---|---|---|---|
| 1 | [requirements](iteration-1-metrics-completion/requirements.md) | [design](iteration-1-metrics-completion/design.md) | [acceptance](iteration-1-metrics-completion/acceptance.md) | 38 close-out, 39 baseline/inventory, 40–52 + 52b metrics |
| 2 | [requirements](iteration-2-performance-portfolio/requirements.md) | [design](iteration-2-performance-portfolio/design.md) | [acceptance](iteration-2-performance-portfolio/acceptance.md) | 53–59, including 57b disclosures |
| 3 | [requirements](iteration-3-risk-optimization/requirements.md) | [design](iteration-3-risk-optimization/design.md) | [acceptance](iteration-3-risk-optimization/acceptance.md) | 60–71 risk and optimization |
| 4 | [requirements](iteration-4-visualization/requirements.md) | [design](iteration-4-visualization/design.md) | [acceptance](iteration-4-visualization/acceptance.md) | 72–76 chart data and ImPlot |
| 5 | [requirements](iteration-5-factor-analysis/requirements.md) | [design](iteration-5-factor-analysis/design.md) | [acceptance](iteration-5-factor-analysis/acceptance.md) | 77–87 panel, kernels, models, research and reports |
| 6 | [requirements](iteration-6-report-runtime/requirements.md) | [design](iteration-6-report-runtime/design.md) | [acceptance](iteration-6-report-runtime/acceptance.md) | 88a early contract; 88b–92 integration; 93 distribution |
| 7 | [requirements](iteration-7-latest-fincore-alignment/requirements.md) | [design](iteration-7-latest-fincore-alignment/design.md) | [acceptance](iteration-7-latest-fincore-alignment/acceptance.md) | 94–99: freeze/re-pin, research corrections, compute reuse, export/hash/positions, measured closeout; NOT_RUN |
| 8 | [requirements](iteration-8-full-python-parity/requirements.md) | [design](iteration-8-full-python-parity/design.md) | [acceptance](iteration-8-full-python-parity/acceptance.md) | 100–110: native parity; see dated execution record above |
| 9 | [requirements](iteration-9-performance-30x-100x/requirements.md) | [design](iteration-9-performance-30x-100x/design.md) | [acceptance](iteration-9-performance-30x-100x/acceptance.md) | 111–125: delivery closed at user request; formal performance acceptance partial |

## Verified planning snapshot

At review start the C++ checkout was `dev` at
`6900250cfe3541e8a40d81963509e777e28f57b7`, clean, with one registered
worktree at `/Users/yunjinqi/Documents/new_projects/fincore_cpp`.
Phase 38 implementation is committed in `e2038e0`, but its acceptance record
and README acceptance entry are absent. The Phase 37 record's 478/478 is
historical evidence, not a test run on the current tree. This review ran no
C++ build or Python numerical oracle.

The target Python object resolves to
`4b8f31b1b59707480d5db9603704d83bbf7e6946`; source inspection used that fixed
object. Phase 39 must establish executable oracle evidence and the complete
capability ledger. Remote state was not queried.

## Dependency order

- 38 close-out → 39 oracle/inventory/benchmark baseline.
- 40–52/52b, 53–59, 60–71 and 72–75 can proceed in bounded independent
  slices after 39 and their named prerequisites. Whole-iteration completion
  is not an artificial prerequisite for unrelated kernels.
- 49 has a bounded scalar-solver gate; 55 supplies the solver used by 54.
  59's transaction liquidity primitives precede 58's capacity composition.
- 60 precedes iterative risk/optimization. 69 precedes the frontier portion
  of 76; the remaining chart work does not wait for all risk work.
- 77 → 78 → 80/81; 82 additionally consumes its required 80 kernels;
  completed 80/81/82 → 79 → 84 → 85/86. Phase 83 follows 79 and its required
  model contracts. Rendering consumes the same frozen results.
- 88a (owned by Iteration 6) starts after 39 and must finish before 67's
  report composition and 84/86. It freezes only the shared schema, not the
  entire runtime or a second report implementation.
- 88b → 89/90; 91 uses the required portfolio/risk models; 92 completes
  catalog/session integration; 87 and 93 close performance/distribution
  evidence at the candidate source identity.

The [top-level roadmap](../plans/2026-10-23-fincore-cpp-iteration-roadmap.md)
preserves the six-iteration R2 structure and adds the dated Iteration 7 extension.
Per-phase designs refine
it; changes in scope, ownership, baseline or acceptance require a dated
amendment. Blank checklists remain `NOT_RUN`.
