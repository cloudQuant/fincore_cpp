# Phase 63 Slice 1 Acceptance — GPD Risk Estimates

## Scope

Adds `gpd_risk_estimates` and its result struct. Three files changed; no
dependency change.

## Python parity

Source: `fincore/metrics/risk.py` 501–700. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase63-python.fTGpZc`), exit `0`, zero stderr; hashes in the
design. Verified: threshold loop, GPD Nelder-Mead fit at the source's default
tolerances, closed-form VaR/ES, aligned-wrapper equality, short-input zeros,
and ordering invariants.

## Test coverage

One risk test with the heavy-tail fixture, error and invariant checks, plus
NUL rejection.

## Gates (current tree)

- Full headless CTest: **620/620 passed**.
- LLVM 22 ASan+UBSan: **620/620 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits and remaining scope

`risk/evt.py` (hill estimator, GPD/GEV fits, EVT VaR/CVaR, extreme risk
bundle) remains the second slice of Phase 63. The legacy result is a struct,
not an ndarray; `gpd_risk_estimates_aligned` maps to the same function.
