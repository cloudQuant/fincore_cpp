# Iteration 3 — Risk and Optimization: Requirements

**Phases:** 60–71. **Status:** Revised plan; NOT_RUN.
**Oracle:** Python `4b8f31b1b59707480d5db9603704d83bbf7e6946`.
The [shared delivery contract](../delivery-contract.md) applies.

## 1. Entry and scope

Start the solver spike after Phase 39 and numerical fixture/budget design.
Phase 60 is work inside this iteration, not an already-completed entry
condition. Independent closed-form routes need not await all performance
work. Solver-dependent phases require the successful Phase 60 decision;
Phase 67 report projection additionally needs the early Phase 88a schema.

## 2. Capability inventory and owners

| Phase | Pinned source | Required scope |
|---|---|---|
| 60 | Solver capability spike | Characterize unconstrained/bounded fitting, nonlinear constrained objectives and portfolio equality/inequality constraints. Deliver comparative measured proof, failure cases, dependency/build/license decision and numerical contracts. |
| 61 | `risk/garch.py` | GARCH(1,1), `GARCHResult`, fitting, recursion, forecasts and failure/result semantics. |
| 62 | `risk/garch.py`, `risk/models.py` | `EGARCH`, `GJRGARCH`, `forecast_volatility`, `conditional_var`, `conditional_es`; `RiskEstimate`, `forecast_var`, `forecast_es`. Freeze actual dispatch/model variants from the ledger. |
| 63 | `risk/evt.py`, `metrics/risk.py` | `hill_estimator`, `gpd_fit`, `gev_fit`, `evt_var`, `evt_cvar`, `extreme_risk` plus metric GPD routes. Fit, support/shape constraints and tail estimates are separate tests. |
| 64 | `metrics/risk.py` | `beta_fragility_heuristic` and any canonical/aligned mapping in the ledger; closed-form contract. |
| 65 | `risk/backtesting.py`, `risk/calibration.py` | `kupiec_lr`, `christoffersen_lr`, `RiskBacktestResult`, `backtest_var`, `backtest_es`; `expected_exception_count`, `basel_traffic_light`, `es_calibration_score`. |
| 66 | `risk/diagnostics.py` | `WalkForwardVaRResult`, `walk_forward_var`; estimator dispatch, training/forecast schedule and aggregation. |
| 67 | `risk/specs.py`, `risk/report.py` | `RiskModelSpec`, `RiskValidationReport`, `build_risk_validation_report`; shared-schema projection contract with `report/risk.py`. Rendering belongs to 91. |
| 68 | Solver integration | Private objective/constraint adapters and lifecycle/error/status plumbing from 60; no gratuitous public solver framework. |
| 69 | `optimization/frontier.py` | `efficient_frontier` and its exported result/config mappings; moments, feasible points and target-return constraints. |
| 70 | `optimization/objectives.py` | `optimize` and every supported objective/option at the pin, including max-Sharpe/min-vol; no silent objective truncation. |
| 71 | `optimization/risk_parity.py` | `risk_parity`; risk contributions, supported budgets/constraints, degeneracy and convergence semantics. |

The ledger covers public submodules even when the package-root `__all__` is
empty. New discoveries are assigned before implementation; a GARCH-only or
GPD-only slice does not close the entire risk domain.

## 3. Numerical requirements

Do not promise exact SciPy gradient, likelihood or floating reduction parity
merely because an expression is closed-form. Preserve existing exact tests;
otherwise declare justified per-kernel and output budgets. Test likelihoods,
variance paths, moments and gradients (when used) at fixed parameters before
comparing fitted outputs. Independent finite-difference checks apply to
implemented analytic gradients.

Solver success requires objective/residual and feasibility checks, including
sum of weights, bounds, target return and risk budget as applicable. Near-
singular covariance, infeasible targets, constants, short/nonfinite inputs,
max iterations and convergence failures have explicit result/error semantics.
Nonunique optima compare objective/feasibility first; weight equality is
required only where uniqueness is established. A seed is not a convergence
proof, and ERC residual is a bounded solve criterion, not a zero-by-definition
identity at machine precision.

## 4. Exit

All mapped risk/optimization capabilities and metric cross-owners accepted;
solver evidence includes failures and supported builds; full candidate
CTest/sanitizer checks and workloads pass. Rendering remains in 91. No
financial validity or production risk approval follows from source parity.
