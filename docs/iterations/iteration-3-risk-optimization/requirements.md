# Iteration 3 — Risk Models and Optimization: Requirements Specification

**Iteration ID:** iteration-3-risk-optimization
**Phase range:** Phases 60–71
**Source of truth:** fincore Python `dev@4b8f31b`: `fincore/risk/`, `fincore/optimization/`, plus `fincore/metrics/risk.py` (GPD, beta fragility)
**Status:** Draft

> Roadmap amendment 2026-10-23: former Iterations 3 (risk) and 4
> (optimization) are merged into one iteration because they share the
> Phase 60 solver decision and the same tolerance-parity regime.

## 1. Purpose and Scope

Port the risk-model domain (GARCH family, EVT tail risk, VaR/ES backtests,
walk-forward validation, risk-model spec and report compute) and the
optimization domain (efficient frontier, max-Sharpe/min-vol objectives, risk
parity). This is the first iteration that depends on iterative numerical
optimization, which changes the parity regime: **exact hex-float parity is
required for all closed-form intermediates; optimizer-dependent outputs are
accepted under the declared-tolerance policy** (roadmap §2 amendment).

## 2. Entry Conditions

- Iterations 1–2 accepted (metrics kernel, drawdown table, performance
  inference all available for reuse).
- Phase 60 solver decision completed and recorded.

## 3. Functional Requirements

### 3.1 Solver spike (gates the whole iteration)

| Phase | Requirement |
|---|---|
| 60 | Design-only spike: select the numerical optimization strategy. Option A: vendor a local solver checkout (OSQP for QP + a small unconstrained solver) via the established `FINCORE_<LIB>_SOURCE_DIR` CMake pattern. Option B: hand-ported Nelder-Mead + L-BFGS with bounded scope, no new dependency. Deliverables: comparative design doc, build-wiring proof across all four gate configurations (clean, ASan/UBSan, implot-optional, invalid-ImGui expected-failure), parity-feasibility assessment against SciPy on representative fixtures, binding decision. |

### 3.2 Risk-model phases

| Phase | Public API | Python source | Requirement summary |
|---|---|---|---|
| 61 | GARCH(1,1) fit | `fincore/risk/` (GARCH module) | Parameter estimation by MLE. Tolerance-parity contract: (a) per-iteration log-likelihood and gradient evaluations at **bit-exact** parity against the oracle at fixed parameter vectors; (b) final parameters within declared tolerance on frozen fixtures; (c) deterministic seeding/initialization so C++ results reproduce bit-for-bit across runs. |
| 62 | GARCH family variants | `fincore/risk/` | Scope frozen by an oracle scan at design time — enumerate every variant the Python module actually exposes; port exactly those. |
| 63 | GPD/EVT risk estimates | `fincore/metrics/risk.py` + `fincore/risk/` | GPD parameter fitting and quantile/tail estimates; closed-form moments bit-exact, fitting tolerance-banded per the same contract shape as Phase 61. |
| 64 | `beta_fragility_heuristic` | `fincore/metrics/risk.py` | Closed-form; hex-float parity. |
| 65 | VaR/ES backtests | `fincore/risk/` | Backtest statistics (exception counts, traffic-light style classifications per oracle); closed-form parity. |
| 66 | `walk_forward_var` | `fincore/risk/diagnostics.py` | Walk-forward re-estimation loop composing Phase 61+ estimators; deterministic schedule; tolerance-parity on aggregated outputs, bit-exact on per-window closed-form statistics. |
| 67 | `RiskModelSpec` + risk validation report compute | `fincore/risk/specs.py`, `fincore/report/` (risk validation) | Compute-once model: spec value type, digest/fingerprint inputs (`inputs_digest` semantics), report document model. **No rendering** — deferred to Iteration 6. |

### 3.3 Optimization phases

| Phase | Public API | Python source | Requirement summary |
|---|---|---|---|
| 68 | Optimizer integration | — | Objective plumbing per the Phase 60 decision; no public API change beyond what Phase 60 selected; deterministic warm starts. |
| 69 | `efficient_frontier` | `fincore/optimization/frontier.py` | Frontier point set; tolerance-parity on frontier points, bit-exact on closed-form portfolio moments given weights. |
| 70 | `optimize` (max-Sharpe, min-vol) | `fincore/optimization/objectives.py` | Same contract shape as 69. |
| 71 | `risk_parity` | `fincore/optimization/risk_parity.py` | Iterative equal-risk-contribution solve; tolerance-parity on weights, bit-exact on the ERC fixed-point residual identity (sum of contributions differences asserted to machine precision in tests). |

### 3.4 Per-phase standard functional requirements

Standing constraints as Iteration 1 §3.3, plus the tolerance-parity
documentation duty: **every phase's design doc must state its tolerance, the
frozen fixture, and the closed-form/banded split explicitly.**

## 4. Non-Functional Requirements

- All optimizer-dependent results deterministic: same inputs + same seed →
  bit-identical outputs across runs (asserted in tests with double runs).
- Sanitizer gates mandatory per phase (iterative solvers are the classic
  UB/aliasing hazard).
- Vendor checkouts (if Option A) validated by CMake the same way
  DataFrame/ImGui are; no network at build time.

## 5. Exit Criteria

- Phases 60–71 accepted through the standard SDD loop.
- `fincore.risk` and `fincore.optimization` public surfaces at parity
  (enumerated in the final acceptance record).
- Solver decision document superseded only by a dated roadmap amendment.

## 6. Out of Scope

- Rendering of any risk report (Iteration 6).
- PyMC-style Bayesian risk models (non-port).
- Factor risk models (Iteration 5).
