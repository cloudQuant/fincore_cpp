# Phase 57 Native Sharpe Inference Acceptance

## Scope

Adds `standard_error_of_mean`, `sharpe_standard_error`, and
`sharpe_confidence_interval`. Five new/changed paths plus CMake
registrations; no dependency change.

## Python parity

Source: `fincore/performance/inference.py`. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase57-python.B5wIzP`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`8156c5c5e79856431c807b2a7d2496bb2c95c4f53ab1c422a536f79ead8aa4ef`; raw
output SHA-256
`32a4605eccdee19b9eae20852ca8637653093322685deaf8b470be856b8eef1a`.

Verified: NaN-only cleaning, ddof=1 standard deviation, the `1e-15`
deviation fallback, Lo (2002) Sharpe SE, risk-free adjustment order, and CI
bounds with arbitrary `z`.

## Test coverage

Four tests: SE/SR-SE/CI goldens, degenerate samples, selectors/NUL, and
ownership.

## Gates (current tree)

- Full headless CTest: **590/590 passed**.
- LLVM 22 ASan+UBSan: **590/590 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

The value-series utility takes a dataframe column rather than a raw array.
Normal-quantile inputs are caller-supplied; no special-function budget is
introduced here.
