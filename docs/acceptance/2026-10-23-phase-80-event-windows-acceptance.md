# Phase 80 Completion Slice Acceptance — Event Windows and Quantile Event Returns

## Scope

Ports `common_start_returns` and `average_cumulative_return_by_quantile` with
clean Python-oracle evidence, closing the last two factor capability gaps in
the program ledger.

## Common start returns

`event_window_result` is a typed long table (`offset`, `event_date`, `asset`,
`value`) rather than a wide pandas frame:

- Window rows span `[event_position - before, event_position + after]` over
  the returns index, truncated at the edges; offsets are reported relative to
  the event date.
- **Source semantics preserved:** `cumulative=true` leaves the supplied
  returns unchanged; `cumulative=false` first compounds each asset into a
  wealth series (missing returns treated as zero) and windows that curve.
- `mean_by_date=true` averages the event assets per offset and leaves the
  asset label empty; otherwise one cell per event asset is emitted.
- `demean_by` supplies an event-date universe; the cross-sectional mean of
  that universe's window values is subtracted from every event asset.
  Missing factor dates in the returns index are skipped; a demean date with
  no assets fails with `factor_data_error`.
- Non-finite cells are omitted from the long representation (the pandas
  frame keeps them as NaN); this is a declared shape difference.

Oracle fixture (5 dates x 3 assets, SHA-256
`b3822c7beb0a96148212b0dc4562127d2cbc185a138d93f6129443245788fe75`):
raw windows `A/B/C`, date-mean `0.015/0.04` at offset -1, demeaned
`0.015/0.025/-0.015/0` across offsets -1..2, and wealth windows
`1.03/1.0712/1.081912/1.10355024`.

## Average cumulative return by quantile

`average_cumulative_return_by_quantile(factor_data, returns, before, after,
demeaned)` builds one event series per quantile (the quantile value itself is
the event factor, matching the source), calls the window kernel with
`cumulative=true` and `mean_by_date=true`, optionally demeans against the
all-asset quantile universe, then reports, per offset, the mean and sample
standard deviation across event dates.

Oracle values: Q1 (one event date) means `0.015/0.015/0.025/0.02` with no
standard deviation; Q2 means `0.035/0.025/0.015/0.025` with first deviation
`0.007071067811865477`; demeaned Q1 `-0.005/-0.008333333333333335/0.005/0`
and demeaned Q2 `0.005/0.008333333333333333/-0.005/0`. The test asserts these
values and the one-date standard-deviation `NaN`.

## Declared gaps

Group-aware variants (`by_group`, `group_adjust`) are not exposed; missing
cells are omitted rather than carried as NaN; and `demean_by` uses the source's
quantile universe (it ignores the supplied values for demeaning, exactly as
the pinned implementation does).

## Tests and gates

`FactorPerformance.CommonStartEventWindowsMatchCleanPythonOracle` covers raw,
date-mean, demeaned, wealth and quantile-event outputs. Full headless CTest:
**716/716 passed**. Sanitizer not re-run; visual NOT_RUN.
