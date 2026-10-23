# Phase 61 Native GARCH(1,1) Acceptance

## Scope

Adds `garch11_log_likelihood`, `fit_garch11`, and `forecast_garch11` with
parameter/result value types. Five new/changed paths plus CMake
registrations; no dependency change.

## Python parity

Source: `fincore/risk/garch.py`. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase61-python.jYCuR4`), exit `0`, zero stderr; hashes in the
design. Verified: constant-mean fit parameter/loglik parity, closed-form
likelihood at oracle parameters (1e-9), closed-form variance/forecast at the
native optimum (1e-12), error classes, and determinism.

## Recorded source-optimizer finding

SciPy L-BFGS-B stalls on the zero-mean fixture at loglik
`747.2805538792478`; the Python likelihood at the native parameters is
`747.7286069253973`, so the native fit is better and the acceptance asserts
`log_likelihood >= oracle` rather than parameter equality with the stalled
point.

## Test coverage

Five tests: both mean models, determinism, closed-form regression,
insufficient/all-NaN, selector/NUL/horizon.

## Gates (current tree)

- Full headless CTest: **615/615 passed**.
- LLVM 22 ASan+UBSan: **615/615 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

GARCH(1,1) only; no EGARCH/GJR/OLS; owning vectors rather than Series.
