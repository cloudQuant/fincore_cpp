# Phase 66 Walk-Forward VaR Acceptance

## Scope

Adds `risk_model_spec`, `walk_forward_var`, and supporting enums/value types;
extracts SHA-256 into `fincore::detail`. Eight new/changed paths plus CMake
registrations; no dependency change.

## Python parity

Source: `fincore/risk/specs.py` and `diagnostics.py`. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase66-python.fm7dfz`), exit `0`, zero stderr;
hashes in the design. Normal and historical path values, refit counts/dates,
and backtest fields match at 1e-12; unsupported and insufficient statuses
match with the source reasons.

## Test coverage

Three tests: both distributions, structured statuses, validation, digest
shape.

## Gates (current tree)

- Full headless CTest: **634/634 passed**.
- LLVM 22 ASan+UBSan: **634/634 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Digest is the declared native canonical form (not pandas-CSV-equal). The
GitHub-style `fit_parameters` dictionary becomes an owning refit vector.
