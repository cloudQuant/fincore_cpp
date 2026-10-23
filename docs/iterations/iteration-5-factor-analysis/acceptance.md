# Iteration 5 — Factor Analysis: Acceptance Document

**Iteration ID:** iteration-5-factor-analysis
**Status:** Template — completed and signed off per phase and at iteration close

## 1. Standard Per-Phase Gate (Phases 77–87)

Identical to Iteration 1 §1 (full SDD loop, clean-oracle design run with
hex-float goldens, clean-tree build, full CTest, ASan+UBSan, exact-index
gate, acceptance record + README ledger). Rendering phases (85–86) add the
Iteration 4 viz gates where adapter code changes.

## 2. Iteration-Specific Acceptance Items

### Phase 77 (panel adapter — hard gate)
- [ ] No factor metric in the phase diff (audit).
- [ ] `qcut`-parity tests: bucket edges, labels, duplicate-edge handling, NaN
      exclusion — goldens from a clean detached pandas oracle.
- [ ] `shift`/forward-return parity tests, including period > 1 and
      trailing-NaN classes.
- [ ] Cross-section iteration order and per-asset view order locked by tests
      (value order preserved; no sort/dedup).

### Phase 78 (prepare_factor_data)
- [ ] PIT/calendar subset decision recorded with the exclusion list.
- [ ] Prepared frame parity: bucket assignment per (date, asset) cell matches
      oracle on the frozen fixture.

### Phase 79 (analyze_factor)
- [ ] Frozen model fields enumerate the oracle model 1:1 (audit table in the
      acceptance record).
- [ ] Compute-once asserted: no downstream section recomputes a model field
      (structural test or include-graph audit).

### Phase 80–81 (performance, turnover)
- [ ] Per-stage intermediates (membership matrices, set-difference counts)
      parity-asserted, not just final scalars.
- [ ] Long-universe and quantile-turnover fixtures from the Python tests
      (`9027733`) ported with matching values.

### Phase 82 (factor portfolio)
- [ ] Weight/capacity-aware position semantics parity; degenerate-universe
      classes locked.

### Phase 83 (evaluate_factor_research)
- [ ] Validation parity: non-UTC index, wrong-typed columns, missing names →
      the established error classes in the oracle's precedence order.
- [ ] Artifact result type carries CSV paths + chart models; no I/O side
      effects beyond documented artifact writing.

### Phase 84 (tear-sheet models)
- [ ] Model types frozen; no rendering code in the diff.

### Phase 85 (ImPlot tear sheets)
- [ ] Panels compose Iteration 4 primitives (reuse audit).
- [ ] Fidelity notes for every matplotlib/seaborn primitive without an ImPlot
      equivalent (violin, QQ) recorded in the acceptance.
- [ ] Viz gates apply (headless core, compile/link-only, invalid-ImGui).

### Phase 86 (bilingual ECharts HTML report)
- [ ] `echarts.min.js` SHA-256 pin test committed; asset byte-identical to
      the Python repo's.
- [ ] Byte-stability determinism test: two runs → identical report bytes.
- [ ] Structural validation vs the Python oracle report: same sections,
      same chart families, values within the parity policy; zh/en label
      dictionaries complete against the oracle's.

### Phase 87 (perf parity)
- [ ] Reuse-parity measurements recorded: weight reuse within one analysis,
      model-snapshot reuse across sections, fingerprint streaming — behavior
      demonstrated on a large fixture (identical results, reuse asserted by
      instrumentation or structural argument).
- [ ] No numerical-output change vs Phase 86 head (full suite green).

## 3. Iteration-Level Definition of Done

- [ ] All phases 77–87 accepted per §1 and §2.
- [ ] Public API parity audit for `factor_analysis/` ported subset +
      exclusion list with reasons, committed in the final acceptance record.
- [ ] Oracle drift log: every fincore `dev` advance detected during the
      iteration has a recorded response (re-pin phase inserted or
      no-impact classification).
- [ ] Iteration-head clean build + full CTest + ASan/UBSan evidence; commits
      pushed.

## 4. Sign-Off Record

| Item | Date | Commit | Evidence root | Accepted by |
|---|---|---|---|---|
| Phase 77 | | | | |
| Phase 78 | | | | |
| Phase 79 | | | | |
| Phase 80 | | | | |
| Phase 81 | | | | |
| Phase 82 | | | | |
| Phase 83 | | | | |
| Phase 84 | | | | |
| Phase 85 | | | | |
| Phase 86 | | | | |
| Phase 87 | | | | |
| Iteration head | | | | |
