# Phase 49 Native Stutzer Index Design

## Source contract

`fincore/metrics/stats.py::stutzer_index` (lines 226–290): drop NaN only from
`returns - target_return`; require two observations; zero mean excess →
`0.0`; minimize `ln(mean(exp(theta*excess)))` over `(-50, -1e-10)` when the
mean is positive and `(1e-10, 50)` otherwise; objective returns `0.0` if any
exponential term is nonfinite; `ip = max(-objective, 0)`; return
`sign(mean)*sqrt(2*ip)`.

## Solver decision (bounded scalar optimization)

Native golden-section search over the source bounds to `|b-a| <= 1e-12`,
200-iteration cap. The objective is unimodal in the exponential tilt family,
so the search is deterministic and self-contained; no dependency and no
coupling to Phase 60's multivariate/constrained decision. Numerical policy:
1e-6 absolute tolerance against the SciPy oracle, whose bounded default is
`xatol=1e-5`. The constant-return fixture witnesses the difference
oracle `0.9999999380979772` vs native `1.0`.

## Oracle evidence

Root `/tmp/fincore-phase49-python.1qYrrC`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`c4b3a78a508f25d8f2c4871d19b068972f66691e255452017547c1f007ec0949`; raw
output SHA-256
`b3009fb29c76efa246e9c72873e91fbba4b1b16a981926edac971e5214ff37e5`.

Goldens: ordinary `0.14630936783628698`, target 0.001
`0.11013897196625373`, negative `-0.14630936783628545`, negative target
`-0.11013897196625273`, zeros `0.0`, NaN-mixed `0.12766869919010967`,
constants ±`0.9999999380979772`, overflow guard `-0.7294427821824202`,
positive-infinity sample `0.8242437395987398`, negative-infinity sample
`-0.7585275606810086`.

## Tests

Five stats tests plus the umbrella consumer: both signs, targets, zero/NaN/
short gates, constants, overflow and infinity, selectors, NUL/missing
errors, lifetime ownership.
