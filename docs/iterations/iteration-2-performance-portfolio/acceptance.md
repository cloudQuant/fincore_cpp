# Iteration 2 — Performance (GIPS) and Portfolio Completion: Acceptance Document

**Iteration ID:** iteration-2-performance-portfolio
**Status:** Template — completed and signed off per phase and at iteration close

## 1. Standard Per-Phase Gate (Phases 53–59)

Identical to Iteration 1 §1 (plan → design with clean-oracle run → clean-tree
AppleClang build → full CTest → ASan+UBSan → exact-index gate per commit →
acceptance record + README ledger). Evidence under `/tmp/fincore-phase<N>-*`.

## 2. Iteration-Specific Acceptance Items

### Phase 53–54 (twr / mwr)
- [ ] Chaining identity asserted in tests: TWR equals the geometric product
      of sub-period returns on a multi-cashflow fixture.
- [ ] Degenerate-cashflow classes (no flows, single flow, all-outflow) match
      oracle result classes exactly.

### Phase 55 (xirr)
- [ ] Parity contract declared in the design doc (bit-exact or
      tolerance-banded) and satisfied on the frozen fixture; the fixture,
      tolerance, and convergence criterion recorded.
- [ ] No-bracket and multiple-sign-change inputs produce the oracle's exact
      error/result classes (error types mapped to `fincore::input_error` or
      NaN per oracle behavior, decided in design and asserted in tests).

### Phase 56 (cashflows)
- [ ] Sign-convention fixture (deposits, withdrawals, valuation-date edges)
      matches oracle hex-float goldens.

### Phase 57 (inference)
- [ ] CI bounds at hex-float parity; reuse of the Iteration 1 quantile-helper
      pattern asserted (no duplicated special-function code).

### Phase 58–59 (capacity / liquidity)
- [ ] Multi-column input validation parity: wrong-type/missing columns,
      index mismatch, NUL rejection — same precedence classes as existing
      portfolio leaves.
- [ ] Reuse map from the design doc honored; any private-helper divergence
      carries a recorded reason in the phase acceptance.

## 3. Iteration-Level Definition of Done

- [ ] All phases 53–59 accepted per §1 and §2.
- [ ] Public API parity audit tables for `fincore/performance/` and
      `fincore/portfolio/` committed in the final acceptance record
      (Python function ↔ C++ function ↔ non-port reason).
- [ ] Iteration-head clean build + full CTest + ASan/UBSan evidence recorded.
- [ ] README ledger updated per phase; all iteration commits pushed.

## 4. Sign-Off Record

| Item | Date | Commit | Evidence root | Accepted by |
|---|---|---|---|---|
| Phase 53 | | | | |
| Phase 54 | | | | |
| Phase 55 | | | | |
| Phase 56 | | | | |
| Phase 57 | | | | |
| Phase 57b | | | | |
| Phase 58 | | | | |
| Phase 59 | | | | |
| Iteration head | | | | |
