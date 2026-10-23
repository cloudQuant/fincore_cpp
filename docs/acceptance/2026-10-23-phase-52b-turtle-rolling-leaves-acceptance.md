# Phase 52b Slice 2 Acceptance — Turtle R³ and Rolling Leaves

## Scope

Adds `r_cubed_turtle`, `roll_annual_volatility`, and `roll_sortino_ratio`.
Six files changed. The aligned rolling wrappers are mapped to the existing
`roll_alpha`/`roll_beta`/`roll_alpha_beta` and are not duplicated.

## Python parity

Sources: `metrics/stats.py` 578–657; `metrics/rolling.py` 55–98. Clean
detached oracle at `4b8f31b` (`/tmp/fincore-phase52b2-python.eDN5xv`):
module path asserted, exit `0`, zero stderr bytes. Script SHA-256
`9455b669dd3eecdeb5676cf2e318726b482759f7dcb252e8c1462f1dd2773dec`; raw
output SHA-256
`b90a64e2cd38a88c96a4702937e35de26753d6312632d6e672e9c42a228ad6fe`.

Goldens: turtle `28.394903817964696`, 400-day `-0.10352738136922`,
singleton NaN; volatility window-3 and NaN/52-annualization arrays; Sortino
window-3 including the infinity and zero cases, and NaN-dropping windows.

## Test coverage

Two stats tests and two rolling tests: goldens, short/empty, selectors/NUL,
endpoint index shapes, NaN/inf classes, annualization override, and window
validation.

## Gates (current tree)

- Full headless CTest: **570/570 passed**.
- LLVM 22 ASan+UBSan: **570/570 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

The native boundary is a timestamped frame, so the source's ndarray
252-row-chunk branch is not reachable and is mapped to UTC-year grouping.
`roll_*_aligned` positional wrappers remain unmapped at the API level; their
computations are covered by the accepted label-aligned rolls.
