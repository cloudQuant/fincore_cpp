# Phases 68–71 Portfolio Optimization Plan

**Goal:** Deliver the solver integration (68), efficient frontier (69),
portfolio objectives (70) and risk parity (71) from the pinned
`fincore/optimization` routes.

**Baseline:** `dev` after Phase 63 slice 4 (oracle pin `4b8f31b`). Python
source: `optimization/frontier.py`, `objectives.py`, `risk_parity.py`,
`_utils.py`, `exceptions.py`.

**Files:** `include/fincore/error.hpp` (`optimization_error`),
`include/fincore/optimization/{objectives,frontier,risk_parity}.hpp`,
`src/optimization/{portfolio_solver.hpp,portfolio_solver.cpp,objectives.cpp,frontier.cpp,risk_parity.cpp}`,
`tests/unit/optimization_portfolio_test.cpp`, CMake lists, design/acceptance
records and README.

## Native API

```cpp
struct portfolio_optimization_config { objective; risk_free_rate;
    target_return; target_volatility; short_allowed; max_weight;
    min_weight; sector_constraints; sector_map; };
portfolio_optimization_result optimize_portfolio(returns, config);

struct frontier_config { n_points; risk_free_rate; short_allowed;
    max_weight; periods_per_year; };
efficient_frontier_result efficient_frontier(returns, config);

risk_parity_result risk_parity(returns, risk_budget, max_iter);
```

## Contract

- Phase 68: a private projected-gradient solver with simplex/bounds
  projection, target-return, target-volatility and sector penalties,
  deterministic lifecycle and post-solve feasibility validation mapped to
  `optimization_error`. The Phase 60 QP kernel is reused for unconstrained
  min-variance.
- Phase 69: PSD covariance check, minimum-variance and max-Sharpe points,
  bounded greedy max-return endpoint, `n_points` linspace target returns,
  per-point weights/returns/volatilities/Sharpe and feasibility diagnostics.
- Phase 70: `max_sharpe`, `min_variance`, `target_return`, `target_risk`;
  annualized mean/sample covariance, bounds, sector constraints, target
  validation, and result fields (weights/return/volatility/Sharpe/names).
- Phase 71: source equal-risk-contribution objective, default or supplied
  non-negative budget (normalized), bounded weights, risk contributions and
  annualized volatility.

## Tests

Oracle golden weights/returns/volatilities/Sharpe for all four objectives,
sector/min-weight/short variants, 10-point and 5-point short frontiers with
diagnostics, risk parity equal/custom budgets, and validation/failure cases
(unknown objective, missing targets, bad bounds, sector map, frontier point
counts/assets/infeasible bounds, bad budgets, NaN data).

## Gates

Clean detached oracle at `4b8f31b`; objective/feasibility-first tolerance
comparison with declared budgets; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README.
