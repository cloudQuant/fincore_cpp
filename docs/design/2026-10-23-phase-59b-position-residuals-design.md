# Phase 59b Design — Position Residual Slice

## Source contract

- `get_top_long_short_abs`: max/min/abs-max per column after dropping cash;
  filter positives/negatives; `nlargest`/`nsmallest` with `top`.
- `get_long_short_notional`: per-row positive sum and absolute negative sum,
  cash excluded, NaN masked to zero.
- `stack_positions`: row-major `(dt, ticker)` flattening of non-cash columns
  with `pos_in_dollars` inert; the oracle's pandas version retains NaN cells.

## Native decisions

- Column order follows `numeric_columns` (hmdf insertion order equals the
  oracle's column order in the fixture); ties in ranking are resolved by that
  order, matching `nlargest`'s first-occurrence behaviour in these fixtures.
- The three functions share the existing `numeric_columns` and
  `checked_cash_column_name` helpers; no new normalization is introduced.
- Outputs are owning value types: two ranked vectors and a stack of
  `position_record` values. No pandas-like Series/DataFrame adapters.
- NUL selector rejection precedes column access.

## Oracle evidence

Root `/tmp/fincore-phase59b-python.xCZ0Kq`, detached at `4b8f31b`, exit `0`,
zero stderr bytes. Script SHA-256
`68d55552ddd77b26c3a01c56a8cf5ccbc74df7f395f33c9bda52d707528e5b35`; raw
output SHA-256
`db08cc9a6032071cd28d1a52f93a7b23f6112a499d9f5ed28868104d7c6c1286`.

Four-date fixture with AAA/BBB/CCC plus cash and one NaN cell. Goldens:
top long `AAA 100, CCC 90`; top short `BBB -200, AAA -50`; top abs
`BBB 200, AAA 100`; notionals longs `150, 80, 120, 40`, shorts
`200, 50, 20, 10`; stack of 12 records in row-major order including
`2021-01-04 AAA NaN`.

## Tests

Three positions tests: top lists, notional vectors, stack length/order/NaN,
plus NUL rejection.
