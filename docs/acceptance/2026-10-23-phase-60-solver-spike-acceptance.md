# Phase 60 Solver Spike Acceptance

## Scope

Design decision plus internal numerical kernels (Nelder-Mead, simplex
projection, projected-gradient QP, quadratic objective). New internal header,
one source, one test TU, two CMake list entries. No public header, dependency,
target, or provider change.

## Decision

Option B — hand-ported deterministic kernels. Evidence: no local OSQP
checkout/package exists and the build is network-free; internal kernels add
minimal build wiring; determinism and stopping rules are explicit.

## Evidence (current tree)

- Clean Release CTest: **610/610 passed**.
- LLVM 22 ASan+UBSan: **610/610 passed**, no reports.
- Determinism and analytic convergence fixtures: quadratic, Rosenbrock,
  equal-covariance min-variance, inverse-variance diagonal QP, simplex
  projection with bounds.

## Limits

No Python oracle is used for the kernels (they are numerical primitives, not
financial metrics); downstream phases carry the tolerance-parity contracts.
Public APIs remain untouched; the decision binds Phases 61–71 until a dated
roadmap amendment.
