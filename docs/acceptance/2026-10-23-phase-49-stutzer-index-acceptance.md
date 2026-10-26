# Phase 49 Native Stutzer Index Acceptance

## Scope

Adds `fincore::stutzer_index(returns, target_return = 0.0, column)`. Four
files changed; the bounded solver is private to `stats.cpp`.

## Python parity

Source: `fincore/metrics/stats.py` lines 226–290. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase49-python.1qYrrC`): module path asserted,
exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`c4b3a78a508f25d8f2c4871d19b068972f66691e255452017547c1f007ec0949`; raw
output SHA-256
`b3009fb29c76efa246e9c72873e91fbba4b1b16a981926edac971e5214ff37e5`.

Verified: NaN-only excess filtering, zero-mean short-circuit, both bound
families, nonfinite objective guard, and the signed square-root index.

## Numerical policy

Native golden-section converges the bounded scalar problem to `1e-12`; the
SciPy oracle uses `xatol=1e-5`. Declared acceptance tolerance: **1e-6
absolute**. The constant-return fixture (oracle `0.9999999380979772`, native
`1.0`) documents the expected difference.

## Test coverage

Five stats tests plus the umbrella consumer: both signs/targets, zero and
NaN/short gates, constants, overflow and ±infinity, selectors, NUL/missing
errors, lifetime ownership.

## Gates (current tree)

- Full headless CTest: **551/551 passed**.
- LLVM 22 ASan+UBSan: **551/551 passed**, no reports.
- ImPlot compile/link unchanged for this scalar.

## Limits

The solver bounds and tolerance are the source's; no public optimization
parameters are exposed.
