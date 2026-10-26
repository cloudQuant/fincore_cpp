# Phase 64 Beta Fragility Heuristic Acceptance

## Scope

Adds `beta_fragility_heuristic` to the risk leaf. Three files changed; no
dependency change.

## Python parity

Source: `fincore/metrics/risk.py` 702–768. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase64-python.o5oUiY`), exit `0`, zero stderr;
hashes in the design. All five goldens match exactly.

## Test coverage

One test with ordinary, short, NaN-pair, constant-factor, two-clean goldens
and NUL rejection.

## Gates (current tree)

- Full headless CTest: **628/628 passed**.
- LLVM 22 ASan+UBSan: **628/628 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Empty filtered pairs return NaN instead of the source's indexing error.
