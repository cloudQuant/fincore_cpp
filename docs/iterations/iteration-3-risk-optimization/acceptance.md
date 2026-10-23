# Iteration 3 — Risk Models and Optimization: Acceptance Document

**Iteration ID:** iteration-3-risk-optimization
**Status:** Template — completed and signed off per phase and at iteration close

## 1. Standard Per-Phase Gate (Phases 60–71)

Identical to Iteration 1 §1, with one addition: **every phase's design doc
must contain the D2 three-layer parity contract** (bit-exact layer, tolerance
layer with declared tolerance and frozen fixture, determinism layer). A phase
whose design lacks any of the three layers cannot be accepted.

## 2. Iteration-Specific Acceptance Items

### Phase 60 (solver spike)
- [ ] Comparative design doc committed with measured evidence: parity
      feasibility runs on representative fixtures for each option, build
      wiring proof across all four gate configurations.
- [ ] Binding decision recorded; no dependent phase started before it.

### Phase 61–62 (GARCH)
- [ ] Log-likelihood and gradient at fixed parameter vectors: hex-float goldens
      from a clean detached oracle run.
- [ ] Final parameters within declared tolerance on the frozen fixture.
- [ ] Double-run determinism test (bit-identical outputs) committed.
- [ ] Single shared recursion kernel asserted (no variant duplicates it).

### Phase 63 (GPD/EVT)
- [ ] Same three-layer contract as Phase 61, plus closed-form moment parity.

### Phase 64 (beta fragility)
- [ ] Hex-float parity (closed-form; no tolerance layer needed).

### Phase 65 (backtests)
- [ ] Exception counts and classifications exact-match against oracle on the
      frozen fixture; degenerate-sample classes locked.

### Phase 66 (walk-forward)
- [ ] Window schedule deterministic and documented; per-window closed-form
      statistics bit-exact; aggregated outputs within declared tolerance.

### Phase 67 (RiskModelSpec)
- [ ] Digest cross-language test: C++ digest equals the Python oracle's
      `inputs_digest` on the design-doc fixture.
- [ ] No rendering code in the phase (audit the diff).

### Phase 68–71 (optimization)
- [ ] Portfolio moments at oracle-supplied weights: hex-float goldens.
- [ ] Frontier/objective/risk-parity outputs within declared tolerance on
      frozen fixtures.
- [ ] ERC residual identity (Phase 71) asserted to machine precision.
- [ ] Double-run determinism tests committed.

## 3. Iteration-Level Definition of Done

- [ ] All phases 60–71 accepted per §1 and §2.
- [ ] Public API parity audit tables for `fincore/risk/` and
      `fincore/optimization/` committed in the final acceptance record.
- [ ] Iteration-head clean build + full CTest + ASan/UBSan evidence recorded
      (sanitizer run at iteration head is non-negotiable for this iteration).
- [ ] README ledger updated per phase; all iteration commits pushed.
- [ ] Phase 60 decision doc unamended or amended only by dated roadmap entry.

## 4. Sign-Off Record

| Item | Date | Commit | Evidence root | Accepted by |
|---|---|---|---|---|
| Phase 60 | | | | |
| Phase 61 | | | | |
| Phase 62 | | | | |
| Phase 63 | | | | |
| Phase 64 | | | | |
| Phase 65 | | | | |
| Phase 66 | | | | |
| Phase 67 | | | | |
| Phase 68 | | | | |
| Phase 69 | | | | |
| Phase 70 | | | | |
| Phase 71 | | | | |
| Iteration head | | | | |
