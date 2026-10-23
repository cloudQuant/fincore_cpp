# Phase 57b Native Disclosure Context Acceptance

## Scope

Adds `disclosure_context` and `render_disclosure`. Five new/changed paths plus
CMake registrations; no dependency change.

## Python parity

Source: `fincore/performance/disclosures.py`. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase57b-python.xCvspC`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`67eb6feee4292c3571ee2b7962de881d5486709fcc3c99af925f484cb76ba3f4`; raw
output SHA-256
`b727e20d057635984feede0467d586400020b8f7fcc69871e194fc1b5039f1fa`.

Verified: the eleven fixed lines in source order, the `unspecified`/`none`
fallbacks, `yes`/`no` annualization, notes joined with `; ` and only when
non-empty, and the exact dataclass defaults.

## Test coverage

Three tests compare default, fully custom, and all-empty contexts against
exact multiline oracle strings.

## Gates (current tree)

- Full headless CTest: **593/593 passed**.
- LLVM 22 ASan+UBSan: **593/593 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Text output only; HTML/XLSX rendering and report assembly remain in
Iteration 6. No GIPS-compliance claim is made.
