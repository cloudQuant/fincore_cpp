# Phase 46 Native Sterling Ratio Acceptance

## Scope

Adds `fincore::sterling_ratio(returns, risk_free = 0.0,
annualization = 252, column = "returns")`. Four files changed; no target,
dependency, provider, report, plot, or façade change.

## Python parity

Source: `fincore/metrics/ratios.py::sterling_ratio` (lines 827–888). Clean
detached oracle at `4b8f31b` shared with Phase 43
(`/tmp/fincore-phase43-python.8hb0Ra`): module path asserted, exit `0`, zero
stderr bytes, `FAILURES=0`. Script SHA-256
`828374b9f1373a58dcee161556b8d17c2705fb0f471c34ad999b3e48907499df`; raw
output SHA-256
`cefc85dae73b401959cb07a582014638dfb3e43c021e0833a48364c563f4177e`.

Verified: episode-mean denominator, downside-mean and population-deviation
fallbacks, the `1e-10` gate with intentional +infinity, NaN-as-zero CAGR
numerator, all-NaN NaN, and the signed-zero result for a negative-infinite
sample.

## Test coverage

Five new ratio tests and the umbrella consumer: both denominator paths,
constants, tiny drawdown, NaN-mixed, nonfinite classes, raw-count and
risk-free gates, selectors, NUL precedence, and lifetime ownership.

## Gates (current tree)

- Full headless CTest: **525/525 passed**.
- LLVM 22 ASan+UBSan: **525/525 passed**, no reports.
- ImPlot compile/link re-run: succeeded; demo not executed.

## Limits

Phases 44 and 45 remain open; this phase is accepted out of numeric order
because it shares the drawdown/fallback kernel with Phase 43. No `period`
string API is added.
