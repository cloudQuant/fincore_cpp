# Phase 59b Position Residual Slice Acceptance

## Scope

Adds `get_top_long_short_abs`, `get_long_short_notional`, and
`stack_positions` with three value types. Three files changed; no dependency
change.

## Python parity

Source: `fincore/portfolio/positions.py` 66–96, 207–229, 509–536. Clean
detached oracle at `4b8f31b` (`/tmp/fincore-phase59b-python.xCZ0Kq`): module
path asserted, exit `0`, zero stderr bytes. Script SHA-256
`68d55552ddd77b26c3a01c56a8cf5ccbc74df7f395f33c9bda52d707528e5b35`; raw
output SHA-256
`db08cc9a6032071cd28d1a52f93a7b23f6112a499d9f5ed28868104d7c6c1286`.

Verified: NaN-skipping column extrema, ranked lists, per-row notionals, and
row-major stacking with a retained NaN cell (matching the oracle's pandas
version).

## Test coverage

Three positions tests with exact goldens plus NUL rejection. A first stack
test used a wrong index for the second-row assertion and was corrected.

## Gates (current tree)

- Full headless CTest: **601/601 passed**.
- LLVM 22 ASan+UBSan: **601/601 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

`pos_in_dollars` is accepted but inert, matching the source body. Outputs are
owning value types rather than pandas Series/DataFrame.
