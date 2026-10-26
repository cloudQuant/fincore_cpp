# Phases 68–71 Design — Portfolio Optimization, Frontier and Risk Parity

## Source contract

`optimization/objectives.py: optimize`:

- Requires a non-empty DataFrame with at least 2 observations and finite
  values; `max_weight > 0`; `min_weight <= max_weight`; `sector_map`
  required with sector constraints.
- Annualizes `returns.mean()` and `returns.cov()` (ddof=1) by 252; bounds are
  `[min_weight or (short ? -max_weight : 0), max_weight]`; weights sum to 1.
- Objectives: `max_sharpe` (SLSQP, negative Sharpe, `ftol=1e-12`,
  `maxiter=1000`), `min_variance` (portfolio volatility), `target_return`
  (min variance with an equality return target), `target_risk` (max return
  with an equality volatility target). Sector constraints add lower/upper
  allocation inequalities. Result fields: weights, return, volatility,
  Sharpe, asset names, objective.

`optimization/frontier.py: efficient_frontier`:

- Requires ≥2 observations, ≥2 assets, `n_points ≥ 2`, finite positive
  `max_weight` and `periods_per_year`, finite risk-free rate, finite values.
- Annualizes by `periods_per_year`; checks the covariance is finite,
  symmetric and PSD (eigenvalues ≥ -1e-10); rejects `n*max_weight < 1`.
- Minimum-variance and max-Sharpe SLSQP points; a bounded greedy max-return
  endpoint; `n_points` linspace target returns from the min-variance return
  to the max-return endpoint; endpoint points reuse the min-variance and
  greedy weights; interior points solve min variance with a return equality.
- Each point validates sum/bounds/return residuals (1e-8, return tolerance
  scaled by `max(1,|target|)`); returns arrays, weights matrix, min-variance
  and max-Sharpe blocks, asset names, annualization and diagnostics.

`optimization/risk_parity.py: risk_parity`:

- Requires ≥2 observations, `max_iter ≥ 1`, finite values; covariance × 252;
  default budget `1/N`; supplied budget must be 1-D, finite, non-negative
  with a positive sum and is normalized; bounds `[1e-6,1]`, sum 1.
- Objective `sum((rc/rc_total - budget)^2)` with `rc = w*(Σw)/vol`;
  SLSQP with `ftol=1e-14`, `maxiter=max_iter`; weights normalized; result
  weights, risk contributions, annualized volatility, asset names.

## Native decisions

- The private `detail::minimize_projected` is the Phase 68 solver: simplex +
  bound projection through the Phase 60 projection kernel, Armijo projected
  gradient with bounded normalized direction, and an outer penalty schedule
  for target-return, target-volatility and sector constraints. It is not a
  public solver framework; objectives/constraints are adapters built by the
  three owning modules. Post-solve `validate_weights` checks sum/bounds,
  target return (1e-8 scaled), sectors (1e-8) and target volatility (1e-6),
  raising `fincore::optimization_error`.
- `optimize_portfolio` uses the Phase 60 `minimize_quadratic_on_simplex` for
  min-variance without sector constraints (exact convex QP path) and the
  projected solver otherwise. Target-risk first computes the minimum-variance
  and max-return volatilities; a target outside that interval is rejected as
  infeasible, matching the source's solver failure on target 0.10.
- `efficient_frontier` uses a Jacobi eigenvalue check for PSD, the QP kernel
  for the min-variance point, the projected solver for max-Sharpe and
  interior return-target points, and the source's stable descending-mean
  greedy endpoint. Diagnostics are typed (`frontier_solver` enum +
  sum/lower/upper/return residuals); solver message strings are not
  reproduced.
- `risk_parity` uses the source's squared risk-contribution-share objective
  with a central-difference gradient (n is small) and the projected solver
  with bounds `[1e-6,1]`; weights are renormalized, contributions and
  volatility come from the source formulas.
- DataFrame extraction uses every double column in frame order and its name;
  non-double columns are ignored. Means/covariance are computed directly so
  the annualization factor is explicit. Short sales are bounded only by
  `-max_weight`; no borrow cost/financing model is invented.
- Results own all arrays; labels preserve the DataFrame column order.

## Oracle evidence

Root `/tmp/fincore-phase68-python.qV1AHd`, detached at `4b8f31b`, module
resolved under the detached root, Python 3.11.8 / NumPy 1.26.4 / pandas
3.0.3 / SciPy 1.17.1, exit `0`, empty stderr, clean pre-status. Script
SHA-256 `3b6a484d626b596eef8358493beb9f35d001c23e7ea228b47531ccd164a48651`;
raw output SHA-256
`4396396a10b6a55b6d0a2d6dbf734665ed2641108eed7e32fece6b1f7579cba4`.

Fixture: 300 days, four assets
`0.0004*a + 0.01*sin((t+1)(0.7+0.1a)) + 0.003*cos((t+1)(0.13+0.07a))`;
annualized means `[0.04137267374109348, 0.10518067018144017,
0.19487182851041235, 0.3122292060070843]`.

- min_variance: weights `[0.2449111175220411, 0.25453194116959454,
  0.25452347467870895, 0.24603346662965547]`, return
  `0.16332275676201832`, volatility `0.05793482049938796`, Sharpe
  `2.8190776350768827`.
- max_sharpe (rf 0.02, max 0.7): `[0.0342053696311322, 0.1541496893519629,
  0.3115427697687592, 0.5001021712481456]`, `0.23448614828750652`,
  `0.0708731181623167`, `3.0263399417009`.
- target_return 0.15: `[0.28435786369316385, 0.2733254168638967,
  0.2438482724258347, 0.19846844701710478]`, volatility
  `0.058436766019624525`.
- target_risk 0.06: `[0.16436060916751138, 0.2161572599144954,
  0.2763210875385537, 0.3431610433794395]`, return `0.1905276990129139`,
  volatility `0.060000000000001885`; target 0.10 fails with SLSQP status 8.
- sector (x/y 0.2–0.6) and min_weight 0.1 do not change the min-variance
  point; short min-variance is unchanged.
- Frontier (10 points, rf 0.02, max 0.7): returns
  `[0.16332275676201832 … 0.2770219927580827]`, volatilities
  `[0.05793482049938796 … 0.08898211825400286]`, weights ending at
  `[0, 0, 0.3, 0.7]`. Short 5-point frontier ends at `[-1, 0, 1, 1]`
  (return `0.46572836077640317`, volatility `0.20163113253722942`).
- Risk parity equal: weights `[0.24739839216870257, 0.2523350851557695,
  0.2523098511408693, 0.24795667153465864]`, contributions
  `0.01448430335…`, volatility `0.05793721339877115`; custom budget
  `[0.1,0.2,0.3,0.4]` weights `[0.16086432195279138, 0.23220151467183564,
  0.2841967290791839, 0.3227374342961891]`.

## Tolerance budgets

- min_variance (no sectors): weights 1e-5, return/volatility 5e-7, Sharpe
  5e-6; min_weight/short variants the same.
- max-sharpe: weights 1e-4, return/volatility/Sharpe 1e-6.
- target_return: weights 5e-3, return 1e-6, volatility 1e-5, equality
  residual 1e-8.
- target_risk: weights 5e-4, return 1e-6, volatility 1e-7; infeasible
  targets raise `optimization_error`.
- sector-constrained min variance: weights 5e-3, volatility 1e-5 (penalty
  path; the optimum is not unique to solver precision).
- frontier: returns/volatilities 5e-7, Sharpe 5e-5, interior weights 1e-3,
  endpoints exact; diagnostics residuals ≤1e-8.
- risk parity: weights 1e-6, contributions 1e-8, volatility 1e-8.

These are optimizer tolerance-parity contracts, not SLSQP-path equality.
Uniqueness holds for a positive-definite covariance, so objective and
feasibility are compared first; the weights remain within the budgets above.

## Tests

`optimization_portfolio_test.cpp` (7 tests): min-variance/max-Sharpe oracle
goldens; target-return/target-risk including infeasible target; sector,
min-weight and short variants; 10-point frontier with per-point return,
volatility, Sharpe, endpoint weights and diagnostics; 5-point short frontier;
risk parity equal/custom; and the validation/failure matrix (unknown
objective, missing targets, bad weights, sector map, point counts, one asset,
infeasible bounds, budget errors, max_iter, NaN data).
