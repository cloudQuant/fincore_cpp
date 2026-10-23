# Phase 63 Slice 1 Design — GPD Risk Estimates

## Source contract

`gpd_risk_estimates(returns, var_p=0.01)`:

- `len < 3` → `zeros(5)`.
- Losses `= -returns` restricted to `> 0` (NaN excluded implicitly).
- Threshold loop: start `0.2`, halve while `> 1e-9`; at each step fit the GPD
  tail (`loss >= threshold`) with SciPy Nelder-Mead from `[1,1]` and the
  default `xatol=fatol=1e-4`/`maxiter=200*N`; accept if `success`,
  two parameters, `shape > 0` and the closed-form VaR is positive.
- VaR closed form: `threshold + (scale/shape)*((n/exceed)*p)^(-shape) - 1`.
- ES closed form: `var/(1-shape) + (scale - shape*threshold)/(1-shape)`
  unless `1-shape == 0`.
- Unfinished loop → zero-initialized result.

## Native decisions

- Returns an owning `gpd_risk_estimates_result` struct instead of a 5-element
  ndarray; `gpd_risk_estimates_aligned` is a documented wrapper mapping.
- The threshold loop and closed forms are transcribed verbatim; the fit uses
  Phase 60 Nelder-Mead with the source's *default* tolerances (1e-4/1e-4,
  400 iterations) so the acceptance path matches SciPy's stopping regime
  rather than the tighter fit tolerances used by the GARCH phases.
- Invalid GPD domains return `-DBL_MAX` log-likelihood exactly as the source
  (which makes the negative objective huge but finite), preserving the
  optimizer's exploration behaviour.

## Oracle evidence

Root `/tmp/fincore-phase63-python.fTGpZc`, detached at `4b8f31b`, exit `0`,
zero stderr. Script SHA-256
`4fcce4a2992d97ff413070d6c980bb696402d2440b64a4043c1c059caf6ebd5c`; raw
output SHA-256
`cb800dad51d391418f40d8bc280e0bc9d46bf6aa7592816d29980940cea6e26f`.

200-point fixture with five large negative shocks: result
`[0.1, 0.10505964641693998, 7.484505235073074e-07, 0.16571361940540646,
0.2707733936377823]`; the aligned wrapper equals the base result; a
single-value input returns all zeros. Closed-form VaR/ES recomputed in Python
at the fit parameters match the result exactly.

## Tests

Goldens with tolerance (scale/VaR/ES 1e-2, shape 1e-3; threshold exact),
ordering invariants (`shape > 0`, `es > var > threshold`), short-input zeros,
NUL rejection.
