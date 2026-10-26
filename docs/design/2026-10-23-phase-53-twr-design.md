# Phase 53 Native Time-Weighted Return Design

## Source contract

```python
def twr(returns):
    r = np.asarray(returns, dtype=float)
    r = r[~np.isnan(r)]
    return float(np.prod(1.0 + r) - 1.0)
```

The function is exactly geometric compounding after NaN removal: no
cashflow-event subdivision, no annualization, no finite filter. `np.prod` of
an empty array is `1.0`, hence empty/all-NaN input returns `0.0`.

## Native decisions

- New `performance` leaf directory, mirroring the Python package layout;
  `returns_column` is reused from the metrics boundary header.
- The product is evaluated in DataFrame row order with the same
  NaN-as-skip rule; no sorting, alignment, deduplication, or retention.
- No `period`/annualization parameter exists in the source and none is
  invented.
- CMake source list and the test translation-unit list gain one entry each;
  no new target or dependency.

## Oracle evidence

Root `/tmp/fincore-phase53-python.KgXspf`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes. Script SHA-256
`0f99ea0594eb21e649e77f038ba8d12d013c5717cbb7cdc304d8e3c3473dce54`; raw
output SHA-256
`c0cdc53917ac3860cd8d269aabc08debeb41bc458a0b688e9e4a69af864dc0ae`.

Goldens: ordinary `0.008050700000000077`, single `0.010000000000000009`,
empty `0.0`, all-NaN `0.0`, NaN-mixed `0.019494000000000122`, total loss
`-1.0`, below-minus-one `-1.6`, `+inf` sample `inf`, `-inf` sample `-inf`.

## Tests

Four tests: goldens, NaN/nonfinite classes, selectors/NUL, ownership.
