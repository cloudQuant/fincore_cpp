# Phase 60 Solver Spike Plan

**Goal:** Select and prove the numerical optimization strategy for Iteration 3
(risk-model MLE) and Iteration 4 (portfolio optimization), with build-wiring
evidence across the gate configurations.

**Baseline:** `dev` after the Iteration 2 close-out (oracle pin `4b8f31b`).

**Decision scope:** unconstrained MLE (GARCH/EVT), convex QP on a simplex
(frontier/objectives), and the ERC iteration (risk parity). The decision binds
Phases 61–71 until a dated roadmap amendment.

## Candidate options

- **A — vendor a local solver checkout** (OSQP for QP plus an unconstrained
  solver) using the `FINCORE_<LIB>_SOURCE_DIR` pattern.
- **B — hand-ported deterministic kernels**: Nelder-Mead (unconstrained),
  projected-gradient with exact simplex projection (QP), and a Newton/fixed
  point for ERC.

## Evidence required

1. Availability: no local OSQP checkout or Homebrew package exists on this
   machine (`find` over the project root and `/opt/homebrew` returned none),
   and the build is network-free.
2. Build wiring: the chosen kernels compile and link under the clean Release
   and ASan+UBSan configurations with no CMake dependency additions, and the
   four gate configurations are unaffected.
3. Determinism: identical inputs produce identical parameter vectors and
   iteration counts across runs.
4. Convergence quality: quadratic, Rosenbrock, min-variance QP, and
   inverse-variance QP fixtures converge to analytic solutions within declared
   tolerances.

## Deliverables

- `include/fincore/detail/optimization.hpp` (internal detail API).
- `src/optimization/numerics.cpp` with Nelder-Mead, simplex projection,
  quadratic objective, and projected-gradient QP.
- `tests/unit/optimization_numerics_test.cpp` with the four fixtures.
- Design decision document and acceptance record.

## Exit criteria

Decision recorded; kernels pass under both gate configurations; no new
dependency, target, or provider; public headers unchanged.
