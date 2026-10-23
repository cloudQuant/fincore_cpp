# Iteration 3 — Risk Models and Optimization: Design Document

**Iteration ID:** iteration-3-risk-optimization
**Status:** Draft
**Depends on:** Iteration 1 (drawdown table, inference helpers), Iteration 2
(cashflow model not directly, but performance inference patterns).

## 1. Architecture Overview

New leaf families:

```
include/fincore/risk/{garch,gpd,backtest,walk_forward,specs}.hpp
src/risk/*.cpp + tests/unit/risk_*_test.cpp
include/fincore/optimization/{frontier,objectives,risk_parity,solver}.hpp
src/optimization/*.cpp + tests/unit/optimization_*_test.cpp
```

An internal numerical core is introduced (see D1) — the first shared
implementation detail across leaves, kept private under
`src/optimization/detail/` (or vendored via CMake per the Phase 60 decision).

## 2. Key Design Decisions

### D1 — Phase 60 solver decision (open until the spike)

**Option A — vendor:** OSQP (QP, for frontier/objective problems) plus a
small unconstrained solver. Pros: industrial quality, sparse QP support;
Cons: vendoring friction across four gate configurations, license/version
drift, larger attack surface for the sanitizer gates.

**Option B — hand-ported:** Nelder-Mead + projected-gradient/L-BFGS with
bounded scope. Pros: zero new dependencies, full control of determinism,
every line under our parity regime; Cons: convergence quality risk on hard
frontier instances, maintenance of numerical code.

Selection criteria (binding order): (1) parity feasibility against SciPy
within the declared-tolerance policy on representative fixtures; (2)
build-system friction across the four gate configurations; (3) maintenance
cost. The spike produces a comparative doc with measured evidence, not a
vote.

### D2 — Parity contract shape for iterative solvers (fixed)
Every optimizer-dependent phase defines three layers, and its tests assert
each:

1. **Bit-exact closed-form layer:** log-likelihood, gradient, portfolio
   moments, ERC residuals evaluated at oracle-supplied fixed points — hex-float
   goldens.
2. **Tolerance-banded output layer:** final parameters/weights within a
   tolerance declared in the design doc, measured on a frozen fixture against
   a clean detached oracle run.
3. **Determinism layer:** two C++ runs with identical inputs and seed produce
   bit-identical outputs.

### D3 — GARCH likelihood as the shared numerical kernel
The GARCH(1,1) conditional-variance recursion is implemented once in
`src/risk/detail/garch_recursion.{cpp,h}` (private). Variants (Phase 62)
compose it; the walk-forward loop (Phase 66) re-invokes the same estimator
with a deterministic window schedule. No variant re-derives the recursion.

### D4 — RiskModelSpec is a frozen value type with digest semantics
Mirrors the Python `inputs_digest` fingerprint: the spec value type carries
its inputs and a digest computed with the same content-addressing recipe the
Python side uses (`to_csv`-equivalent canonical serialization → SHA-256).
The serialization recipe is locked by the Phase 67 design doc so digests are
cross-language comparable where the Python oracle's tests assert them.

### D5 — Optimization results are value types, not matrices
Frontier = `std::vector<frontier_point>` (weights vector + moments);
objectives return a result struct (weights + converged flag + iterations).
`hmdf` stays the input boundary; outputs are plain C++ value types consumed
by viz (Iteration 4) and reports (Iteration 6).

## 3. Component Plan

| Phase | Files | New public surface |
|---|---|---|
| 60 | docs + `cmake/` proof only | none (decision doc) |
| 61 | risk/garch.hpp/.cpp + TUs + detail recursion | `garch11_fit` (+ result type) |
| 62 | risk/garch.hpp variants | per oracle enumeration |
| 63 | risk/gpd.hpp/.cpp + TUs | GPD fit/estimate entries |
| 64 | metrics/risk.hpp/.cpp | `beta_fragility_heuristic` |
| 65 | risk/backtest.hpp/.cpp + TUs | VaR/ES backtest entries |
| 66 | risk/walk_forward.hpp/.cpp + TUs | `walk_forward_var` |
| 67 | risk/specs.hpp/.cpp + TUs | `RiskModelSpec` + report compute model |
| 68 | optimization/solver wiring | (per Phase 60) |
| 69 | optimization/frontier.hpp/.cpp + TUs | `efficient_frontier` |
| 70 | optimization/objectives.hpp/.cpp + TUs | `optimize` |
| 71 | optimization/risk_parity.hpp/.cpp + TUs | `risk_parity` |

## 4. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Solver parity mismatch makes acceptance impossible | D2 contract shape agreed up front; Phase 60 measures feasibility before any metric phase starts. |
| Convergence quality of hand-ported solver (Option B) | Spike includes a hard-frontier benchmark; failure there forces Option A. |
| GARCH recursion divergence across variants | D3 single kernel; variant tests share recursion goldens. |
| Digest recipe drift from Python | D4 recipe locked by design doc; cross-language digest test against oracle fixture. |
| Sanitizer failures in numerical kernels | Mandatory per-phase ASan/UBSan; iterative code gets dedicated aliasing tests. |

## 5. Sequencing

60 strictly first. 61 → 62 → 66 (recursion chain). 63, 64, 65 independent
after 61 exists (63 needs fitting machinery; 64/65 closed-form). 67 after
61–66. 68 after 60; 69 → 70 → 71 in order.
