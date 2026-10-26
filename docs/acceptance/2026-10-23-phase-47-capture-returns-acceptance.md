# Phase 47 Native Capture-Return Acceptance

## Scope

Adds `fincore::up_capture_return` and `fincore::down_capture_return`
(annualization + two selectors). Four files changed; the shared private
implementation lives in `ratios.cpp`.

## Python parity

Source: `fincore/metrics/ratios.py` lines 1405–1516. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase47-python.eEXGHW`): module path asserted,
exit `0`, zero stderr bytes, `FAILURES=0`, post-run checkout clean. Script
SHA-256
`57e2e63d766f072589fa2846c4a88c97a2899894c01024963cfd11dd61692d15`; raw
output SHA-256
`d79dc852ca66e2e2fcb9b7083b74699773578fdb7b4e364fed75aadda455b018`.

Verified: inner alignment, sign-window selection, empty-window NaN, no
minimum selection size (single up-market observation `11.274002099240244`),
NaN-as-zero CAGR compounding, and NaN-pair windows.

## Test coverage

Five ratio tests plus the umbrella consumer: goldens, empty/single windows,
NaN pairs, selectors, NUL/missing errors, and lifetime ownership. Focused
suite: 101/101.

## Gates (current tree)

- Full headless CTest: **542/542 passed**.
- LLVM 22 ASan+UBSan: **542/542 passed**, no reports.
- ImPlot compile/link unchanged for these scalars.

## Limits

No `up_down_capture_return` exists at this pin and none is invented. No
alignment-policy parameters; inner alignment only.
