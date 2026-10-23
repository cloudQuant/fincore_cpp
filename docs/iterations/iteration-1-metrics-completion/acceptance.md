# Iteration 1 — Metrics Completion: Acceptance Document

**Iteration ID:** iteration-1-metrics-completion
**Status:** Template — completed and signed off per phase and at iteration close

## 1. Standard Per-Phase Gate (applies to Phases 38–52)

Each phase is accepted only when **all** of the following hold, with evidence
retained under a unique `/tmp/fincore-phase<N>-*` root:

- [ ] Plan doc committed (`docs/plans/<date>-phase-N-*.md`) with exact-signature
      constraints and the exact-index commit gate.
- [ ] Design doc committed, containing a clean detached Python oracle run
      (Anaconda base interpreter, pinned fincore checkout) with hex-float
      goldens and edge-case result classes.
- [ ] Clean-tree AppleClang build; full CTest run green, no regressions in
      the previously accepted phases.
- [ ] Homebrew LLVM 22 ASan+UBSan run green on the phase's new tests and the
      full suite.
- [ ] Exact-index gate used before every commit; staging-scope evidence
      directory retained.
- [ ] Acceptance record committed (`docs/acceptance/...`) and README ledger
      section updated.

## 2. Iteration-Specific Acceptance Items

### Phase 38 close-out
- [ ] Clean-oracle run re-executed (not reusing the design-doc run) covering:
      raw-count gates 0–4, dampening, clamp boundaries at n=19/20/49/50,
      zero-std NaN/+inf/−inf classes, order-sensitivity fixtures.
- [ ] Working-tree `adjusted_sharpe_ratio` code committed via exact-index
      gate; tree clean at phase head.

### Phase 39 oracle re-pin
- [ ] Fresh detached checkout of fincore `dev@4b8f31b`; old pin `2354bb5`
      and new pin SHA recorded in the design doc.
- [ ] Full C++ suite re-run against regenerated goldens; drift log committed
      with every item classified (`no-drift` / `re-baselined` / `blocked-on`).
- [ ] Zero unresolved drift items at phase close.

### Phase 50 (drawdown table)
- [ ] Table row count, ordering, and every field matches the oracle table on
      the design-doc fixture at exact parity (hex-float where numeric,
      exact-match where ordinal/date).
- [ ] Underwater series matches the existing native drawdown series
      (consistency identity asserted in tests).

### Phase 52 (bootstrap perf_stats)
- [ ] Deterministic seeding contract: two runs with the same seed produce
      bit-identical results; seed handling matches oracle semantics.
- [ ] Bootstrap variant reuses Phases 20–22 primitives (asserted by include
      graph, not duplicated code).

## 3. Iteration-Level Definition of Done

- [ ] All phases 38–52 accepted per §1 and §2.
- [ ] Public API parity audit: enumerate `fincore/metrics/*.py` public
      functions (minus `bayesian.py`) and the C++ headers; every Python
      function either has a C++ counterpart or appears in the roadmap's
      non-port list. Audit table committed in the final acceptance record.
- [ ] Clean-tree build + full CTest + one ASan/UBSan run at the iteration
      head commit (recorded as the iteration-head evidence, distinct from
      per-phase runs).
- [ ] README ledger contains one section per phase with commit chains and
      fresh-build evidence.
- [ ] All iteration commits pushed to origin.

## 4. Sign-Off Record

| Item | Date | Commit | Evidence root | Accepted by |
|---|---|---|---|---|
| Phase 38 | | | | |
| Phase 39 | | | | |
| Phase 40 | | | | |
| Phase 41 | | | | |
| Phase 42 | | | | |
| Phase 43 | | | | |
| Phase 44 | | | | |
| Phase 45 | | | | |
| Phase 46 | | | | |
| Phase 47 | | | | |
| Phase 48 | | | | |
| Phase 49 | | | | |
| Phase 50 | | | | |
| Phase 51 | | | | |
| Phase 52 | | | | |
| Iteration head | | | | |
