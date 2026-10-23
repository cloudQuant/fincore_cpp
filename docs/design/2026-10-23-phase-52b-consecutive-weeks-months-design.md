# Phase 52b Slice 1 Design — Weekly/Monthly Consecutive Runs

## Source contract

`fincore/metrics/consecutive.py`: empty input → NaN; resample with pandas
`W` (Sunday-ending, right-closed) or `ME`; per bucket apply
`cum_returns_final` (`np.nanprod(1 + r) - 1`); then `_max_consecutive_run`
over `> 0` / `< 0` masks. Buckets with a zero compound return break runs.

## Native decisions

- Bucket keys: weekly = UTC day advanced to the week-ending Sunday; monthly =
  `year * 12 + (month - 1)`. Groups are ordered by key, matching pandas'
  sorted resample output even for duplicate/nonmonotonic input labels.
- Bucket return reuses the NaN-as-zero compounding rule already accepted for
  `annual_return`/`compound_wealth`.
- No resampling parameters are exposed; the source's calendar is fixed.

## Oracle evidence

Root `/tmp/fincore-phase52b-python.k4bDem`, detached at `4b8f31b`, module
path asserted, exit `0`, zero stderr bytes. Script SHA-256
`ad4e9678ddb9d68480d1af190644f18213cd46f4502f8669691271903f869b7d`; raw
output SHA-256
`84a01b24661e51faeb12fa27c098c418fdc513e445cc48e438626b14a743261b`.

Goldens: five-month daily fixture (2020-11-01…2021-03-31, constant sign per
month) → up months 2, down months 1, up weeks 8, down weeks 5; six-week
Monday-to-Sunday fixture (ISO weeks 1,2,4 positive) → up weeks 2, down weeks
2; empty input → NaN.

## Tests

Two consecutive tests: the monthly/weekly goldens and the
empty/selector/NUL/ownership matrix.

## Open ledger

`r_cubed_turtle`, `roll_annual_volatility`, `roll_sortino_ratio`, and aligned
rolling alpha/beta variants remain; Phase 52b stays open.
