# Phases 68–71 Portfolio Optimization Acceptance

## Scope

Adds the private projected-gradient/penalty solver (Phase 68), the public
`optimize_portfolio` objectives (Phase 70), `efficient_frontier` (Phase 69)
and `risk_parity` (Phase 71), plus `fincore::optimization_error`. Eight new
paths and three changed CMake/test lists; no dependency change.

## Python parity

Source: `fincore/optimization/{objectives,frontier,risk_parity,_utils,exceptions}.py`.
Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase68-python.qV1AHd`), Python 3.11.8 / NumPy 1.26.4 /
pandas 3.0.3 / SciPy 1.17.1, exit `0`, empty stderr, clean pre-status.
Script SHA-256
`3b6a484d626b596eef8358493beb9f35d001c23e7ea228b47531ccd164a48651`; raw output
SHA-256 `4396396a10b6a55b6d0a2d6dbf734665ed2641108eed7e32fece6b1f7579cba4`.

- **Phase 70 objectives** (300-day, 4-asset fixture): min-variance weights
  match the oracle at 1e-5 (volatility `0.05793482049939`, Sharpe
  `2.8190776350768827`); max-Sharpe weights at 1e-4 (return
  `0.23448614828750652`, volatility `0.0708731181623167`, Sharpe
  `3.0263399417009`); target-return 0.15 weights at 5e-3 with a 1e-8
  equality residual; target-risk 0.06 weights at 5e-4 (volatility residual
  3e-10); sector-constrained min variance weights at 5e-3; min-weight and
  short variants at 1e-5.
- **Phase 69 frontier**: 10-point returns/volatilities at 5e-7, Sharpe at
  5e-5, interior weights at 1e-3, endpoint weights exact; min-variance and
  max-Sharpe blocks match. Short 5-point frontier ends at `[-1, 0, 1, 1]`
  with return `0.46572836077640317`/volatility `0.20163113253722942`.
  Per-point sum/upper-bound residuals are zero within 1e-8.
- **Phase 71 risk parity**: equal-budget weights at 1e-6 (contributions
  `0.01448430335…`, volatility `0.05793721339877115`); custom budget
  `[0.1,0.2,0.3,0.4]` weights at 1e-6, contributions at 1e-8.
- **Failures**: unknown objective, missing targets, `max_weight <= 0`,
  `min_weight > max_weight`, sector constraints without a map, a single
  frontier asset, `n_points < 2`, infeasible max-weight sums, malformed
  budgets, `max_iter < 1` and NaN data map to `input_error`; target-risk
  targets outside the feasible volatility interval raise
  `optimization_error`, matching the source's solver failure (SLSQP status 8
  for target 0.10).

## Declared divergences

- SciPy SLSQP is not reproduced. The native solver is a deterministic
  projected gradient with penalties; the contract is objective/feasibility
  parity within the budgets above, not solver-path/weight equality. For a
  positive-definite covariance the optima are unique, so weights are also
  compared.
- Min-variance without sectors runs through the Phase 60 QP kernel; the
  sector-constrained path uses the penalty solver and is documented with a
  5e-3 weight budget.
- Frontier diagnostics expose a typed solver enum and residuals instead of
  SciPy status/message strings; target-risk infeasibility is a pre-check
  rather than a solver-status observation.
- `operations.py` (runtime `OperationSpec` registration) remains owned by
  Iteration 6; this delivery exposes the domain functions only.
- No transaction costs, covariance shrinkage, borrow financing or short
  locate model is added; the source has none in these routes.

## Test coverage

`optimization_portfolio_test.cpp` (7 tests): min-variance and max-Sharpe
goldens; target-return and target-risk including the infeasible target;
sector/min-weight/short variants; 10-point frontier arrays, endpoints and
diagnostics; 5-point short frontier; risk parity equal/custom; validation
matrix for all four phases.

## Gates (current tree)

- Full headless CTest: **655/655 passed**.
- LLVM 22 ASan+UBSan: **655/655 passed**, no sanitizer reports.
- ImPlot compile/link unchanged.

## Limits

These are numerical portfolio optimizers for mapped source parity. They do
not promise a global optimum for nonconvex objectives beyond the tested
fixtures, do not include the runtime operation registry, and do not
constitute investment advice or a production allocation approval.
