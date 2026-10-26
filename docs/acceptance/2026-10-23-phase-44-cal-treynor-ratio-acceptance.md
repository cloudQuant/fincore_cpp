# Phase 44 Native Cal-Treynor Ratio Acceptance

## Scope

Adds `fincore::cal_treynor_ratio(returns, benchmark, risk_free = 0.0,
annualization = 252, returns_column, benchmark_column)`. No alias is
declared: Python's `treynor_ratio` forwards to this canonical route and is
recorded here as a mapping only. Four files changed.

## Python parity

Source: `fincore/metrics/ratios.py` lines 606–700. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase44-python.Od17iK`): module path asserted,
exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`617b033470e162960af92281edb63f8361b99867b2389b9e38d7eadc7fe0af29`; raw
output SHA-256
`b7a46f3edef5afbe712dd3f118e56f67b05a8d82db7d82e5d2aff25fe139b442`.

Verified: inner alignment, positive-beta goldens, beta zero/negative/NaN
gates, short input, NaN pairs, and the alias identity (`treynor - cal_treynor
= 0.0` on the oracle fixture).

## Test coverage

Five ratio tests plus the umbrella consumer: goldens, beta gates, NaN pairs,
selectors, NUL/structural precedence, and lifetime ownership. Focused suite
at the test commit: 90/90.

## Gates (batch head, shared with Phase 45)

- Full headless CTest: **537/537 passed**.
- LLVM 22 ASan+UBSan: recorded in the Phase 45 acceptance at the same head.
- ImPlot compile/link: unchanged; adapter not rebuilt for this scalar.

## Limits

Single-scalar native output only; no 2D/DataFrame forms. No `period` string
parameter. Phase 45 shares the oracle root and fixture set.
