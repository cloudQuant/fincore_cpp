# Phase 48 Native Hurst Exponent Acceptance

## Scope

Adds `fincore::hurst_exponent(returns, column)`. Four files changed; no
dependency or façade change.

## Python parity

Source: `fincore/metrics/stats.py` lines 126–222. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase48-python.MTUMFS`): module path asserted,
exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`d907d8d204697c7e075663d51924d431ea339675cbc7ce201c585d125f1de9f8`; raw
output SHA-256
`cfa09dbf6afcf71dedaf5359bf1bee81d10c4ade058421934dcd2219fc13e3e1`.

Verified: raw/clean length gates, R/S lag enumeration including NumPy
`geomspace` endpoint handling (22 unique lags for n=120), per-block ratio
averaging, fallback path, log-log OLS slope, and [0,1] clamping. Floating
behaviour of repeated non-representable vs exactly representable constants is
locked by two fixtures.

## Test coverage

Four stats tests plus the umbrella consumer: short/constant/zero gates, sin
and cos long-series goldens (1e-6 tolerance for transcendental ulp
differences), spike and eight-value clamp cases, NaN mixtures, selectors,
NUL/missing errors, and lifetime ownership.

## Gates (current tree)

- Full headless CTest: **546/546 passed**.
- LLVM 22 ASan+UBSan: **546/546 passed**, no reports.
- ImPlot compile/link unchanged for this scalar.

## Limits

No window/lag parameters are exposed; the source's fixed policy is
reproduced. The regression uses closed-form OLS, not `np.polyfit`'s SVD path.
