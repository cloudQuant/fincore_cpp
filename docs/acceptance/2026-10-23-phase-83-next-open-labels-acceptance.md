# Phase 83 Completion Slice — Next-Open-to-Open Labels

## Scope

`research_spec.label` now selects the price-derived label basis:

- `close_to_close` (default) keeps the existing prepare-by-horizon path.
- `next_open_to_open` builds labels as `open[t + period + 1] / open[t + 1] -
  1` from a sorted, duplicate-free wide open-price frame, then evaluates
  through the shared per-horizon assembly.
- Any other label fails with `factor_data_error`; the precomputed overload
  rejects `next_open_to_open` and the price route rejects `precomputed`.

## Behaviour

- Labels are `<N>D`. Tails without a future open remain missing and reduce
  per-horizon coverage; the missing tail produces NaN IC cells for that
  horizon rather than failing evaluation.
- The manifest records the requested `label`.
- No separate grid frame is used; the price frame's index is the date axis.
  Exchange-calendar label naming (non-`D` labels) is not implemented.

## Oracle

Pinned Python `4b8f31b1`, 8 UTC dates, opens A=100..107, B=50..57,
C=10..17, study window on the first four dates, periods 1/5:

| cell | oracle |
| --- | --- |
| 1D at 2022-01-03 (A/C) | 0.009901 |
| 1D at 2022-01-06 (A/C) | 0.009615 |
| 5D at 2022-01-03 (A/C) | 0.049505 |
| 5D at 2022-01-05 and 01-06 | NaN |

Oracle output SHA-256
`bf447355a15ce823309ecf9f5fe04b9d841d155a35c7004b978b343b00ff536e`;
script SHA-256
`c26fd9f15b55725aaabca7c8f048ec840e6a838a9e53ea918c4a63321b571d3d`.

The native test asserts two labels, coverage `12/6`, six finite IC values of
`~1`, two NaN tail cells, the manifest label, and fail-closed unknown,
precomputed-on-price, and unsorted-price cases.

## Remaining research gaps

Multi-factor evaluator assembly remains open; grid-driven
non-day label naming is not implemented.

## Gates

Full Release and ASan+UBSan suites: **731/731 passed**. Visual NOT_RUN.
