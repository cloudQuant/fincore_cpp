# Phase 43 Native Burke Ratio Design

## Source contract

`fincore/metrics/ratios.py::burke_ratio` (lines 889–960). The ratio is
`(annualized return - risk_free) / risk`, where `risk` is:

- `sqrt(sum(dd^2))` over `get_all_drawdowns` episode values when any
  non-zero drawdown exists;
- otherwise, `max(population std(clean), 1e-10)` when there are no negative
  clean returns, or `population std(negative clean returns)` when there are.

`np.nanprod`-based compounding ignores NaN while the exponent uses the raw
length, which the accepted native `annual_return` already reproduces. The
`risk == 0 or risk < 1e-10` gate returns positive infinity when annual excess
return is positive and NaN otherwise.

## Implementation notes

- Reuses `get_all_drawdowns` (accepted drawdown kernel) and native
  `annual_return`; no drawdown recomputation.
- Adds private helpers `population_standard_deviation` (ddof=0) and
  `all_zero_drawdowns` in `ratios.cpp`.
- NaN drawdown episodes flow through as NaN risk, reproducing the oracle's
  all-NaN and negative-infinity behaviour through the division.

## Oracle evidence

Root `/tmp/fincore-phase43-python.8hb0Ra`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`828374b9f1373a58dcee161556b8d17c2705fb0f471c34ad999b3e48907499df`; raw
output SHA-256
`cefc85dae73b401959cb07a582014638dfb3e43c021e0833a48364c563f4177e`.

Goldens: ordinary `25.638234713640998`, risk-free 0.001
`25.62108485512675`, annualization 52 `3.5608068202725582`, risk-free 10
`-145.86035042886795`, monotone fallback `17733.449583883645`, flat `0.0`,
positive constant `112740020992.40234`, negative constant
`-30.994059065669386`, tiny drawdown `+inf`, NaN mixed
`-1.7656492431737019`, all-NaN NaN, `+inf` sample `+inf`, `-inf` sample NaN.

## Tests

Six ratio tests plus the umbrella consumer cover the drawdown and fallback
paths, constants, gates, nonfinite classes, selectors, NUL/structural
precedence, and lifetime ownership.
