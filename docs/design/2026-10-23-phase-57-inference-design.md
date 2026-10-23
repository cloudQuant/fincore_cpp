# Phase 57 Native Sharpe Inference Design

## Source contract

`fincore/performance/inference.py`:

- `standard_error_of_mean`: NaN drop, `n < 2` → NaN,
  `np.std(ddof=1)/sqrt(n)`.
- `sharpe_standard_error`: NaN drop then subtract `risk_free`; `n < 2` →
  NaN; `sr = mean/std` when `std > 1e-15` else `0`;
  `sqrt((1 + 0.5*sr^2)/n)`.
- `sharpe_confidence_interval`: NaN drop then risk-free adjustment; `n < 2`
  → `(NaN, NaN)`; SR as above; SE recomputed from the adjusted values; bounds
  `sr ± z*SE` with `z=1.96` default and no finite filter.

## Native decisions

- The value-series utility takes a dataframe plus column instead of a raw
  NumPy array; this matches the native boundary used by every other leaf.
- Private helpers `clean_values`, `sample_std`, and `sharpe_ratio_of` live in
  the translation unit; the CI reuses the same SR/SE arithmetic instead of
  calling the public SE function with a second risk-free subtraction (which
  would double-subtract).
- NUL selectors are rejected before DataFrame access; bounded selectors are
  supported through `string_view`.

## Oracle evidence

Root `/tmp/fincore-phase57-python.B5wIzP`, detached at `4b8f31b`, exit `0`,
zero stderr bytes. Script SHA-256
`8156c5c5e79856431c807b2a7d2496bb2c95c4f53ab1c422a536f79ead8aa4ef`; raw
output SHA-256
`32a4605eccdee19b9eae20852ca8637653093322685deaf8b470be856b8eef1a`.

Goldens: SE mean `0.00896908269804914`, NaN-mixed
`0.014529663145135579`; Sharpe SE `0.31779627930188475`, risk-free
`0.3171110093724174`, constant `0.5`; CI
`(-0.4818505590150136, 0.7639108558483746)`, risk-free/`z=2.58`
`(-0.7123737928683265, 0.9239190154933472)`; singleton bounds NaN.

## Tests

Four performance tests covering the goldens, degenerate samples,
selectors/NUL, and ownership.
