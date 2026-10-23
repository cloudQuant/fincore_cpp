# Phase 59c Design — Round-Trip Statistics

## Source contract

`fincore/portfolio/round_trips.py::gen_round_trip_stats` builds five tables
from `_presentation.py`:

- PNL stats on `pnl`: sum, positive sum, negative sum, profit factor
  (`positive / |negative|` or NaN), mean, positive mean, negative mean,
  positive/|negative| mean ratio or NaN, max, min.
- SUMMARY: count, percent profitable, winning/losing/even counts.
- DURATION: Timedelta mean/median/max/min plus group-aware trips-per-day and
  trips-per-month using `close_dt.max - open_dt.min` and 21 trading days per
  month.
- RETURNS on `returns`/`rt_returns`: mean/median for all/winning/losing and
  largest win/loss.
- SYMBOLS: per-symbol return statistics (labels use `max`/`min`).

Aggregation loops `groupby("long")` (False then True), then prepends
`All trades`; empty input returns five empty frames.

## Native decisions

- Native frame schema is the accepted `extract_round_trips` output: `pnl`,
  `open_dt`, `close_dt`, `long`, `rt_returns`, `symbol`, plus `duration`
  (timestamp). Duration statistics are computed from `close_dt - open_dt`
  and reported in **days as double** — the oracle converts Timedelta values
  to days, so the declared unit is explicit.
- Empty input is detected from the frame index before column access, because
  the native frame cannot hold zero-length typed columns; this reproduces the
  source's early empty return.
- Group order is `All trades`, `short`, `long`; absent groups are skipped.
- `symbols` uses ascending symbol order (pandas groupby default); the
  return-stat label mapping is duplicated in the table labels instead of a
  generic callable dictionary because C++ has no runtime stats dictionary.
- Mappings: `agg_all_long_short` is this aggregation engine (not a separate
  public leaf); `groupby_consecutive` maps to
  `groupby_consecutive_transactions`; `add_closing_transactions` is covered by
  `extract_round_trips` closing trips; `apply_sector_mappings_to_round_trips`
  remains a sector-mapping helper handled in the exposures slice.

## Oracle evidence

Root `/tmp/fincore-phase59c-python.oSidR4`, detached at `4b8f31b`, exit `0`,
zero stderr bytes. Script SHA-256
`f2b5bd2aae5805211f20cc443c6cc96454af630233f1c35d3cbb02b212e2db64`; raw
output SHA-256
`8636f6d2e6acabfd3437f82a1f317cd1ef1cd74a69ccb7b0f5c5dd6f2154c26d`.

Six-trip fixture (three long, three short, one zero PnL): PNL All
`135/185/-50/3.7/22.5/61.6667/-25/2.4667/100/-40`; SUMMARY All
`6/0.5/3/2/1`; DURATION All
`2.3333333333333335/2.5/4/0/0.6666666666666666/14` days; RETURNS All
`0.02/0.06/-0.030000000000000002/0.01/0.06/-0.03/0.1/-0.05`; SYMBOLS
AAA/BBB/CCC with means `0.025/0.005/0.03`.

## Tests

One round-trip test covering every family, group ordering, NaN loss classes,
symbol labels, and the empty-input guard. An initial implementation routed the
returns family to the PnL vector; the test caught it and the group collector
now carries both series.
