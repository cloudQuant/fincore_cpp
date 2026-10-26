# Phase 53 Native Time-Weighted Return Acceptance

## Scope

Adds the new `performance` leaf with `fincore::twr`. Five changed paths plus
the CMake source/test registrations; no target or dependency change.

## Python parity

Source: `fincore/performance/returns.py::twr` (32–39). Clean detached oracle
at `4b8f31b` (`/tmp/fincore-phase53-python.KgXspf`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`0f99ea0594eb21e649e77f038ba8d12d013c5717cbb7cdc304d8e3c3473dce54`; raw
output SHA-256
`c0cdc53917ac3860cd8d269aabc08debeb41bc458a0b688e9e4a69af864dc0ae`.

Goldens: ordinary `0.008050700000000077`, single `0.010000000000000009`,
empty/all-NaN `0.0`, NaN-mixed `0.019494000000000122`, total loss `-1.0`,
below-minus-one `-1.6`, ±infinity propagation.

## Test coverage

Four tests: goldens, NaN/nonfinite classes, selectors/NUL, ownership.

## Gates (current tree)

- Full headless CTest: **576/576 passed**.
- LLVM 22 ASan+UBSan: **576/576 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

No cashflow-event subdivision (Phase 56) and no annualization. Iteration 2
continues with the IRR root contract (Phase 55) before MWR (Phase 54), as
required by the revised plan.
