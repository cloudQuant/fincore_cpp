# Phase 59 Native Transaction Liquidity Design

## Source contract

`fincore/portfolio/transactions.py`:

- `days_to_liquidate_positions`: `dv = volume*price`; rolling mean over
  `mean_volume_window` shifted by one row; zero means become NaN;
  `get_percent_alloc` with `cash` dropped; per-row
  `alloc*capital_base / (max_bar_consumption*mean_dv)`; rows `0..w-1` are
  dropped.
- `get_max_days_to_liquidate_by_ticker`: optional `dtlp` filter to dates
  strictly after `max_date - last_n_days`; unstack to (symbol, date), sort by
  days descending, `groupby(symbol).first` — i.e. the row with the largest
  days value, reporting its allocation percent.
- `get_low_liquidity_transactions`: daily absolute amount per (symbol, day)
  joined with that day's close/volume; `amount/volume*100`; sort descending;
  `groupby(symbol).first` — the maximum percent; optional date filter.

## Native decisions

- Market data is wide (date × symbol) and must share one index with
  positions; mismatches throw `input_error` instead of relying on pandas
  label alignment.
- The long transaction ledger is represented as owning `trade_record`
  values; the close price is irrelevant to the percent formula and the volume
  is looked up from the volume frame, so the record needs only date, symbol,
  and signed amount (absolute value is taken).
- Allocation reuses the accepted `get_percent_alloc` formula but excludes the
  cash column before normalization? No — the source normalizes including cash
  and only then drops the cash column; the native helper does exactly that
  (row total includes cash, output excludes it).
- Rolling means skip invalid windows (any NaN → NaN) and treat a zero mean as
  NaN, matching pandas defaults.
- Ties select the earliest date in ascending order (pandas sort instability is
  not reproducible); goldens avoid ties.

## Oracle evidence

Root `/tmp/fincore-phase59-python.xbOTIp`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes. Script SHA-256
`6bc5e62924473be499be017a0824e9ef81ac85fbfe023ee4af498987a6385310`; raw
output SHA-256
`0b6c65d6405f138e02e7e4a69dd8ebb24e8bd290ae635a87443306a7f8de1630`.

Goldens: seven-day, two-symbol, window-2 fixture. Days matrix rows
2021-01-03..07 for AAA `[33.733133433283356, 51.86721991701245,
55.47177006951307, 38.833874233724444, 46.44428052345437]` and BBB
`[24.380333197887037, 19.88808432547754, 21.117094287825992,
23.089355806972986, 19.185020336121557]`. Ticker maximums AAA
`55.47177006951307` / `13.008130081300814`, BBB `24.380333197887037` /
`5.217391304347826`; last-2-days BBB becomes `23.089355806972986` /
`6.557377049180328`. Low-liquidity AAA `2021-01-02` / `3.3333333333333335`,
BBB `2021-01-03` / `7.2727272727272725`; last-2-days both on `2021-01-05`
(`1.5384615384615385` and `2.307692307692308`).

## Tests

Three transaction tests: oracle-exact days/maximums, low-liquidity paths, and
validation (bar/window/index mismatch/NUL).
