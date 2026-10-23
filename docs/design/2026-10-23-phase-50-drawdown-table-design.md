# Phase 50 Native Drawdown Table and Rank APIs Design

## Source contract

`fincore/metrics/drawdown.py` lines 223–438:

- `get_top_drawdowns`: repeatedly locate the maximum underwater episode on a
  cumulative-wealth series (NaN returns treated as zero), record
  `(peak, valley, recovery)`, then splice out the segment between the peak
  and recovery (or truncate at the peak when unrecovered).
- `gen_drawdown_table`: pad to `range(top)` rows; `Net drawdown in %`
  computed from the cumulative values; dates formatted `YYYY-MM-DD`;
  `Duration` is the inclusive business-day count via
  `pd.date_range(peak, recovery, freq="B")`, NaN when unrecovered.
- `get_max_drawdown_period`: `(start, end)` = last peak before the first
  argmin valley; monotone series return `(first, first)`; empty input
  `(None, None)`.

## Native decisions

- `drawdown_table_row` carries timestamps and an optional business-day count
  instead of formatted strings; rendering belongs to Iteration 6. Net
  drawdown percent equals `-underwater[valley] * 100`, which matches the
  cumulative-ratio formula because every peak's underwater value is zero.
- Business-day counting uses UTC day buckets: day 0 (1970-01-01) is Thursday,
  weekday index `((day % 7 + 7 + 3) % 7)` with Monday = 0, counting 0–4.
  A unit defect found during hardening (nanoseconds-per-day was 1000× too
  small) was fixed and is guarded by the exact duration goldens (6 and 4).
- Ranking reuses one private index-based helper for both the window list and
  the table, so ordering and unrecovered handling cannot diverge.

## Oracle evidence

Root `/tmp/fincore-phase50-python.ouAOjp`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes. Script SHA-256
`97b939aeaf2cc314193d98fba8aabaf264ddd634d4e5227bcc04dcaefb98c353`; raw
output SHA-256
`2c4c9992910c6be4e0035e1c2e78ab5ce8970010c0cda1067a17db3015153b75`.

Fixture (business days from 2021-01-04):
`[.02,-.05,.01,.06,.01,-.03,-.04,.05,.02,.03,-.02,.01]`. Top three:
`(Jan 8, Jan 12, Jan 15)`, `(Jan 4, Jan 5, Jan 7)`,
`(Jan 15, Jan 18, unrecovered)`. Table nets `6.880000000000005`,
`5.000000000000004`, `2.0000000000000093`, durations `6`, `4`, NaN.
Period `(Jan 8, Jan 12)`. Monotone table pads three empty rows; period
`(Jan 4, Jan 4)`. Unrecovered fixture net `10.918`.

## Tests

Five drawdown tests: ranked windows, table nets/durations/padding, unrecovered
table, period classes, and selector/ownership validation.
