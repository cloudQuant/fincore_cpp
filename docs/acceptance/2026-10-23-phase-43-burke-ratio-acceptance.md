# Phase 43 Native Burke Ratio Acceptance

## Scope

Adds `fincore::burke_ratio(returns, risk_free = 0.0, annualization = 252,
column = "returns")`. Four files changed; no target, dependency, provider,
report, plot, or façade change.

## Python parity

Source: `fincore/metrics/ratios.py::burke_ratio` (lines 889–960). Clean
detached oracle at `4b8f31b` (`/tmp/fincore-phase43-python.8hb0Ra`): module
path asserted, exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`828374b9f1373a58dcee161556b8d17c2705fb0f471c34ad999b3e48907499df`; raw
output SHA-256
`cefc85dae73b401959cb07a582014638dfb3e43c021e0833a48364c563f4177e`.

Verified: drawdown-episode denominator, no-drawdown downside/std fallback,
`max(..., 1e-10)` floor, CAGR numerator with NaN-as-zero compounding, risk
gate producing intentional +infinity, and the negative-infinity/NaN classes.

## Test coverage

Six new ratio tests and the umbrella consumer: drawdown and fallback goldens,
risk-free/annualization variants, constants, tiny drawdown, NaN-mixed,
all-NaN, ±infinity samples, raw-count gate, selectors, NUL/structural
precedence, and lifetime ownership.

## Gates (current tree)

- Full headless CTest: **520/520 passed**.
- LLVM 22 ASan+UBSan: **520/520 passed**, no reports.
- ImPlot compile/link re-run: succeeded; demo not executed.

## Limits

No `period` string parameter; annualization is the effective input. Private
helpers stay in `ratios.cpp`.
