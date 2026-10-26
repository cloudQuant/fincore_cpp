# Phase 64 Design — Beta Fragility Heuristic

## Source contract

`metrics/risk.py` 702–768: min length 3 on the aligned inputs; drop NaN
pairs; stable sort by factor; start/mid/end rows with
`mid = round(n/2)` (NumPy banker's rounding); weights from factor gaps
(0.5/0.5 if the range is zero); result
`w_start*r_start + w_end*r_end - r_mid`.

## Native decisions

- Label alignment via `align_on_common_index` (the established binary-metric
  boundary), then NaN-pair filtering and `std::stable_sort`.
- `std::nearbyint` reproduces NumPy's round-half-to-even for `n/2` under the
  default rounding mode.
- Empty filtered pairs return NaN as a defined native class (the source would
  raise on indexing); recorded rather than left implicit.
- NUL selectors are rejected before alignment.

## Oracle evidence

Root `/tmp/fincore-phase64-python.o5oUiY`, detached at `4b8f31b`, exit `0`,
zero stderr. Script SHA-256
`400a1258b70fe08f41d04a43aadd87228326359ff692a6f52c7e0bb7dacc43b2`; raw
output SHA-256
`e3a4656262472f29fff265c144f80f732c83112e7f50cb46ef89348dbd0becda`.

Goldens: ordinary `0.012499999999999999`, short NaN, NaN-pair `-0.02`,
constant factor `-0.005`, two-clean pair `-0.02`.

## Tests

One risk test with the five goldens and NUL rejection.
