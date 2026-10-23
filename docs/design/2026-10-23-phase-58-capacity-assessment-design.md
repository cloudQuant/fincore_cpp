# Phase 58 Native Capacity Assessment Design

## Source contract

`fincore/portfolio/capacity.py`:

- `CapacityConfig` frozen dataclass: `max_bar_consumption=0.2`,
  `capital_base=1e6`, `mean_volume_window=5`, `last_n_days=None`, with the
  four validation rules.
- `CapacityAssessment` holds `liquidation_days`, `ticker_maximums`, and
  `low_liquidity_transactions`.
- `assess_liquidity` composes the three transaction kernels; it does not add
  calculations of its own.

## Native decisions

- The config is a plain struct with the same defaults; validation is
  duplicated at the entry point because C++ structs have no `__post_init__`.
- `liquidation_days` is returned as the full date×symbol frame (the source
  does not apply `last_n_days` to it), while the ticker maximums and
  low-liquidity records apply their own filters — preserving the source's
  asymmetry.
- The transaction span is passed through unchanged; no copying into a frame.
- Directory `include/fincore/portfolio` mirrors the Python package for
  capacity; the underlying kernels stay in `metrics/transactions.hpp` to
  reuse the accepted transaction leaf.

## Oracle evidence

Root `/tmp/fincore-phase58-python.J7tNIo`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes. Script SHA-256
`20f8e038125827251506d64c329cf919bf51a44bf4ace0ead02c92710c92e742`; raw
output SHA-256
`9b07c67dc76e507034b3429e057d1f989ae0fcbbe36a8926f1b62091565008e9`.

With `mean_volume_window=2`, `last_n_days=2`: liquidation grid
2021-01-03..07 (AAA `[33.733133433283356, 51.86721991701245,
55.47177006951307, 38.833874233724444, 46.44428052345437]`); ticker
maximums AAA `55.47177006951307` / `13.008130081300814`, BBB
`23.089355806972986` / `6.557377049180328`; low-liquidity both on
2021-01-05 with `1.5384615384615385` and `2.307692307692308`. Config errors
match the four source messages.

## Boundary correction

Oracle cross-checking exposed a Phase 59 boundary defect: the ticker-maximum
filter must include the cutoff date (`loc[cutoff:]`), unlike the
low-liquidity strict `>` filter. The kernel and its test were corrected in
this phase and the correction is recorded in the Phase 59 acceptance record
as an addendum.

## Tests

Composition goldens plus the four config-validation error classes.
