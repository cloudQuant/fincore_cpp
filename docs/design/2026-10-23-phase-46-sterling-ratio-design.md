# Phase 46 Native Sterling Ratio Design

## Source contract

`fincore/metrics/ratios.py::sterling_ratio` (lines 827–888). Numerator is
`annual_return - risk_free` (CAGR, with `np.nanprod`-style NaN-as-zero
compounding, already reproduced by native `annual_return`). Denominator is:

- `|mean(drawdown episodes)|` when any non-zero episode exists;
- otherwise `|mean(negative clean returns)|` when downside returns exist;
- otherwise `max(|population std(clean)|, 1e-10)`.

Gate: `average == 0 or average < 1e-10` → +infinity when annual excess return
is positive, else NaN. No other finite filtering.

## Implementation notes

Shares `population_standard_deviation` and `all_zero_drawdowns` with Phase 43
and reuses `get_all_drawdowns` unchanged. NaN drawdown episodes propagate
through the mean, matching the oracle's all-NaN and negative-infinity
classes (the latter yields signed zero through `(-1)/inf`).

## Oracle evidence

Same clean detached run as Phase 43
(`/tmp/fincore-phase43-python.8hb0Ra`, `4b8f31b`, exit `0`, zero stderr,
`FAILURES=0`; script SHA-256
`828374b9f1373a58dcee161556b8d17c2705fb0f471c34ad999b3e48907499df`, raw
output SHA-256
`cefc85dae73b401959cb07a582014638dfb3e43c021e0833a48364c563f4177e`).

Goldens: ordinary `37.373828320998385`, risk-free 0.001
`37.348828320998386`, annualization 52 `5.190723318961585`, monotone
`17733.449583883645`, flat `0.0`, positive constant
`112740020992.40234`, negative constant `-30.994059065669386`, tiny
drawdown `+inf`, NaN mixed `-1.7656492431737019`, all-NaN NaN, `+inf` sample
`+inf`, `-inf` sample `-0.0`.

## Tests

Five ratio tests plus the umbrella consumer: both denominators, constants,
dates/validity gates, nonfinite classes including signed zero, selectors,
NUL/structural precedence, and lifetime ownership.
