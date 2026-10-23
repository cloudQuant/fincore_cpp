# Iteration 6 — Report and Runtime: Acceptance Document

**Iteration ID:** iteration-6-report-runtime
**Status:** Template — completed and signed off per phase and at iteration close

## 1. Standard Per-Phase Gate (Phases 88–92)

Identical to Iteration 1 §1 (full SDD loop, clean-oracle design run,
clean-tree build, full CTest, ASan+UBSan, exact-index gate, acceptance
record + README ledger).

## 2. Iteration-Specific Acceptance Items

### Phase 88 (ReportDocument)
- [ ] Document model fields enumerate the oracle's document model 1:1 (audit
      table committed).
- [ ] Compute-once discipline: renderer diffs contain no computation (audit
      the diff for numeric literals beyond formatting constants).

### Phase 89 (HTML renderer)
- [ ] Byte-determinism test: two runs → identical bytes.
- [ ] Structural parity vs the Python oracle on the frozen fixture: same
      sections, same tables, same cell values (within parity policy);
      escaping rules match (angle brackets, quotes, non-ASCII).

### Phase 90 (XLSX renderer)
- [ ] Emitter decision recorded (minimal OOXML default, or amended vendoring
      plan with reason).
- [ ] Output opens in a real spreadsheet application; sheet/table/value
      structural comparison vs openpyxl output committed as evidence.
- [ ] Byte-determinism test.

### Phase 91 (portfolio + risk validation reports)
- [ ] `build_portfolio_report`-equivalent output covers the oracle's section
      list (returns, positions, rolling window inputs) on the frozen
      fixture.
- [ ] Risk validation report closes Phase 67's deferred rendering item;
      consumes Phase 67's model without recomputation (asserted).

### Phase 92 (runtime core)
- [ ] Catalog registration audit: every ported public operation registered;
      no stubs for unported operations (absent, not present-and-failing).
- [ ] `plan` validation: unknown operation, cyclic request, shape mismatch →
      established error classes in the oracle's precedence order.
- [ ] `run`/`batch` semantics: deterministic ordering; per-operation error
      propagation matches oracle behavior; `AnalysisSession` ownership
      (frames by value, snapshot immutability) asserted by tests.
- [ ] `ArtifactBundle` collects document/chart bytes from a full end-to-end
      example (the portfolio report path) — one integration test exercising
      catalog → plan → run → bundle → render.

## 3. Iteration-Level Definition of Done

- [ ] All phases 88–92 accepted per §1 and §2.
- [ ] Public API parity audit tables for `report/` and `runtime/` ported
      subsets committed in the final acceptance record.
- [ ] Non-port list (requirements §6) re-verified against the current
      fincore `dev` head; any reversal recorded as a dated amendment.
- [ ] Iteration-head clean build + full CTest + ASan/UBSan evidence; commits
      pushed.
- [ ] **Program-level close-out:** final parity audit across all six
      iterations (metrics, performance/portfolio, risk/optimization, viz,
      factor analysis, report/runtime) committed as the port's master
      acceptance record, referencing each iteration's sign-off.

## 4. Sign-Off Record

| Item | Date | Commit | Evidence root | Accepted by |
|---|---|---|---|---|
| Phase 88 | | | | |
| Phase 89 | | | | |
| Phase 90 | | | | |
| Phase 91 | | | | |
| Phase 92 | | | | |
| Iteration head | | | | |
| Program close-out | | | | |
