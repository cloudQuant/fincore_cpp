# Phase 61 Native GARCH(1,1) Design

## Source contract

`fincore/risk/garch.py`: NaN cleaning, `T >= 11`, initialization
`(mean?, 0.1*var, 0.1, 0.85)`, L-BFGS-B bounds `(omega 1e-6.., alpha
1e-6..1, beta 1e-6..1)`, variance recursion from the population variance,
Gaussian likelihood with one-observation burn-in, stationarity-based
`converged`, and the analytical forecast recursion.

## Native implementation

- Bounds are enforced by transforms (`omega = 1e-6 + exp(theta)`, alpha/beta
  = `1e-6 + (1-2e-6)*sigmoid(theta)`), then Phase 60 Nelder-Mead optimizes the
  unconstrained likelihood `theta`. This keeps the spike decision intact and
  avoids penalty discontinuities.
- The public `garch11_log_likelihood` exposes the closed-form kernel for
  regression; it shares the recursion and normal-likelihood code with the fit.
- Mean handling matches the source: zero mean fixes `mu = 0`; constant mean
  optimizes `mu` from the sample mean.

## Parity policy (tolerance class per roadmap §2)

- **Closed-form layer, bit-exact intent:** log-likelihood at oracle parameters
  and conditional-variance/forecast values at the native optimum were
  recomputed in Python and matched to 1e-12.
- **Fit layer, declared tolerance:** constant-mean parameters match SciPy
  within 1e-4/1e-7 and log-likelihood within 1e-4.
- **Source-optimizer stall finding:** for the zero-mean fixture, SciPy's
  L-BFGS-B terminates at a suboptimal point (`loglik 747.2805538792478`).
  Re-evaluating the Python likelihood at the native parameters gives
  `747.7286069253973`, confirming the native Nelder-Mead reaches a better
  optimum. The acceptance therefore requires
  `log_likelihood >= oracle_loglik` and a closed-form reproduction at oracle
  parameters, instead of blind parameter equality with a stalled optimizer.
  This is recorded, not hidden.
- **Determinism layer:** two identical fits return identical parameters and
  log-likelihood (asserted).

## Fixture

300-point deterministic GARCH-generated series: shocks
`sin(1.7 i) + 0.3 cos(0.9 i)`, `sigma2_0 = 5e-5 + 0.9*var(z)`, recursion
`5e-5 + 0.1*eps^2 + 0.8*sigma2`. An initial C++ fixture omitted the
`0.8*var(z)` term; the likelihood mismatch exposed it and the generator was
corrected to match the oracle byte-for-byte.

## Oracle evidence

Root `/tmp/fincore-phase61-python.jYCuR4`; recovery script SHA-256
`4327759a7b542a272ea5319044a69b947e922a9ce456b1f9f62acd1142ea7342`, raw
output SHA-256
`d0ad7f35e5815218300176a662dd872843598342103fae6d959f17bb8be1be19`;
closed-form check script SHA-256
`0ab2f5817e56e6accea6212174ffdfdc870b41040f6b904d1860ce75d22c3a3e`.
Zero-mean oracle stopped at `omega 1.969069393424767e-05`, `alpha
0.28572917912639345`, `beta 0.6745868900271427`, loglik
`747.2805538792478`; constant mean `mu -0.00010120350935733376`, `omega
2.7373511565588752e-05`, `alpha 0.2561245263089078`, `beta
0.6580996343039742`, loglik `747.7378806410663`. Constant-mean conditional
variances and forecasts match; zero-mean closed-form values at the native
optimum are `var_last 0.00020259002605075416`, forecasts
`[0.00017830257310753238, 0.00020143745105132634]` for steps 0/2.

## Limits

- Only GARCH(1,1) is ported here; EGARCH/GJR remain for Phase 62.
- `method="OLS"` is not implemented in the source path (it ignores the flag);
  the native API does not expose it.
- Residual/forecast arrays are returned as owning vectors, not Series.
