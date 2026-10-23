# Iteration 4 — Visualization Completion: Acceptance Document

**Iteration ID:** iteration-4-visualization
**Status:** Template — completed and signed off per phase and at iteration close

## 1. Standard Per-Phase Gate (Phases 72–76)

The SDD loop adapted to viz, per iteration requirements §4:

- [ ] Plan + design docs committed; design contains the compute-side oracle
      run (pivot values, bin edges, axis ranges, tick layouts, colormap
      control points — whichever apply) with hex-float goldens.
- [ ] Clean-tree AppleClang build with `FINCORE_BUILD_IMPLOT=OFF` (core
      headless) **and** with `FINCORE_BUILD_IMPLOT=ON` (adapter + demo).
- [ ] ImPlot compile/link-only gate green; invalid-ImGui expected-failure
      build fails exactly as expected when wiring changed.
- [ ] Full CTest green; ASan/UBSan on the core compute additions.
- [ ] Exact-index gate per commit; acceptance record + README ledger entry.
- [ ] Manual demo run recorded when performed: screenshot path under the
      evidence root (not blocking).

## 2. Iteration-Specific Acceptance Items

### Phase 72 (monthly heatmap)
- [ ] Monthly pivot compute values bit-exact against the pandas pivot oracle
      (values, month ordering, missing-month handling).
- [ ] Colormap control points committed as data; zero-centered normalization
      matches the oracle's `TwoSlopeNorm`/`Normalize` semantics.
- [ ] Fidelity note for the colorbar if ImPlot's differs from matplotlib's.

### Phase 73 (multi-series + benchmark)
- [ ] Alignment reuse asserted: series built from one aligned frame; index
      mismatch raises the established structural error.
- [ ] Zero line and benchmark-dash styling parameters recorded in the design
      doc (not inline magic numbers in the adapter).

### Phase 74 (drawdown fill)
- [ ] Fill renders from the same drawdown series as the existing line plot
      (consistency identity: same input value type).

### Phase 75 (bars + histogram)
- [ ] Bin edges match `np.histogram` oracle goldens (bin edges, not just
      counts).
- [ ] No violin attempt: seaborn violin substitution decision recorded
      (histogram or per-bin bars) with fidelity note.

### Phase 76 (correlation matrix + frontier)
- [ ] Correlation matrix reuses the Phase 72 `heat_grid` machinery (asserted
      by code structure, not duplication).
- [ ] Frontier chart consumes Iteration 3's frontier value types directly —
      no adapter-side recomputation of moments.

## 3. Iteration-Level Definition of Done

- [ ] All phases 72–76 accepted per §1 and §2.
- [ ] VizBackend protocol audit table committed: every non-factor protocol
      method ↔ C++ adapter function ↔ chart value type ↔ fidelity notes.
- [ ] Headless core build unchanged (`FINCORE_BUILD_IMPLOT=OFF` produces the
      same library surface as before the iteration).
- [ ] Iteration-head clean build + full CTest + ASan/UBSan evidence; commits
      pushed.

## 4. Sign-Off Record

| Item | Date | Commit | Evidence root | Accepted by |
|---|---|---|---|---|
| Phase 72 | | | | |
| Phase 73 | | | | |
| Phase 74 | | | | |
| Phase 75 | | | | |
| Phase 76 | | | | |
| Iteration head | | | | |
