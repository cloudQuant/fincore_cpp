# Phase 62 GARCH Family Design

## Source contracts

- **EGARCH** (`garch.py` 295–460): `T >= 10`; positive finite variance;
  parameters `(omega, alpha, gamma, beta)` from `(0.01, 0.1, -0.1, 0.95)`;
  bounds `alpha >= 1e-6`, `gamma in [-1, 1]`, `beta in [1e-6, 1]`; recursion
  in log variance with conditional standardization `z_{t-1} = y_{t-1} /
  sqrt(s2_{t-1})`; burn-in one; penalty 1e100 outside the finite domain.
- **GJR-GARCH** (462–604): `T >= 10`; init `(0.01, 0.05, 0.05, 0.9)`;
  bounds `omega >= 1e-6`, `alpha in [1e-6,1]`, `gamma in [0,1]`,
  `beta in [1e-6,1]`; indicator recursion for negative shocks; stationarity
  `alpha + 0.5 gamma + beta < 1`.
- **forecast_volatility / conditional_var / conditional_es** (608–760):
  string-selected model, `z_alpha = norm.ppf(alpha)`, total variance summed
  across horizons, VaR `z_alpha*sqrt(total)`, ES
  `-sqrt(total)*pdf(z_alpha)/alpha`.

## Native decisions

- Separate public result types per family (the parameters differ), with a
  `garch_model` enum replacing the source's string selector (unknown strings
  are impossible at the boundary; each enum branch is tested).
- Bounds via transforms: EGARCH `omega` free, `alpha = 1e-6 + exp`,
  `gamma = tanh`, `beta` sigmoid; GJR all four transformed in their
  respective bounds.
- **Deterministic multi-start** for both families: EGARCH starts from
  `(0.01,0.1,-0.1,0.95)`, `(0.01,0.3,0.2,0.95)`, `(0.01,0.3,-0.2,0.9)`,
  `(-0.2,0.2,0.1,0.8)`; GJR from three fixed starts. The best objective wins.
  This was required: a single-start EGARCH run converged to a worse local
  optimum (loglik 745.4, alpha at the floor) versus the oracle's 749.63.
- Normal quantile is a private Acklam approximation with one Newton
  refinement (CDF via `erfc`), matching `norm.ppf(0.05)` to machine precision;
  `alpha` outside `(0,1)` yields NaN (the source's `norm.ppf` returns NaN).
- Dispatchers compute forecasts only; the result types do not own fitted
  models beyond the forecast/convergence summary.

## Oracle evidence

Root `/tmp/fincore-phase62-python.KsCo7s`, detached at `4b8f31b`, exit `0`,
zero stderr. Script SHA-256
`58213a8396853f02f43be5c3b4272699be65256ea30d050557a9b7ec58560d1e`; raw
output SHA-256
`fcf46959a92cdadd0c934587347b403d6d3457d5acdc5da8bb41c8f8fae74d14`.

Goldens: EGARCH `omega -0.4898979840671724`, `alpha 0.2513723688487509`,
`gamma 0.29181294103945654`, `beta 0.9699282283629626`, loglik
`749.6345510786522`, var-last `0.0001585249892216032`, forecasts
`[0.00012303941916578446, …, 0.0001186138132055392]`. GJR `omega
2.7366455496523534e-05`, `alpha 0.25611200984967286`, `gamma 0.0` (bound),
`beta 0.6581371813024437`, loglik `747.7286055791172`. EGARCH conditional VaR
`-0.031313875234808844`, ES `-0.03926886285878336`; GJR VaR
`-0.03927214673733541`; the source's GARCH VaR uses its stalled fit
(`-0.039232733118264355`), so the native GARCH VaR test asserts closed-form
composition instead.

## Tests

Five family tests plus the earlier GARCH tests: tolerance fits, forecasts,
conditional estimates, closed-form GARCH VaR composition, invalid alpha,
short input, NUL, and horizon validation.
