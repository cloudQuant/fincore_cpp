# Phase 54 Native MWR Acceptance

## Scope

Adds `fincore::mwr(cashflows, period = nullopt, amount_column)` reusing the
Phase 55 IRR solver. Three files changed; no dependency or façade change.

## Python parity

Source: `fincore/performance/returns.py::mwr` (244–268). Clean detached oracle
at `4b8f31b` (`/tmp/fincore-phase54-python.a86SSQ`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`077d376f7cd226cd9144ceb212264be213fee1ad11513a6867f7f79b0a8c0108`; raw
output SHA-256
`9f709faafefb6843797e3decc3e48c7d33ca1f77ffa6db1afeb29b1a6d449b6e`.

Goldens: two flows `0.10000000000000002`, half-period
`0.21000000000000005`, three flows `0.05691785736085275`, zero-dropped
`0.048808848170151624`, loss `-0.5000000000000002`; non-conventional and
all-negative NaN; invalid input classes raise `input_error`.

## Test coverage

Three tests: conventional goldens, rejection/error classes, selector/NUL and
ownership.

## Gates (current tree)

- Full headless CTest: **582/582 passed**.
- LLVM 22 ASan+UBSan: **582/582 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Positional cashflow contract (frame index ignored), matching the Python array
API. No multi-root selection; non-conventional streams return NaN.
