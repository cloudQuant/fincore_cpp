# Phase 59d Exposures and Daily Bars Acceptance

## Scope

Adds five exposure functions and `daily_txns_with_bar_data` with owning value
types. Six files changed; no dependency change.

## Python parity

Sources: `portfolio/positions.py` 167–560; `portfolio/transactions.py` 63–93.
Clean detached oracle at `4b8f31b` (`/tmp/fincore-phase59d-python.H8TKLD`),
exit `0`, zero stderr; script/raw hashes recorded in the design. A first run
used `percentile=50` (outside pandas' [0,1]) and a bar-iteration mistake; both
were instrument errors, corrected in the oracle, not product defects.

Verified: sector sums, style series, sector/cap bundles, volume percentiles,
volume validation, and daily-bar values/order.

## Test coverage

Four tests (three positions, one transactions) covering exact goldens and
error classes.

## Gates (current tree)

- Full headless CTest: **606/606 passed**.
- LLVM 22 ASan+UBSan: **606/606 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Panels must share one index (stricter than pandas inner alignment). Unmapped
sector symbols are excluded without a warning channel. Adapter helpers
(`make_transaction_frame`, `map_transaction`, `extract_pos`,
`apply_sector_mappings_to_round_trips`) are recorded as mappings to the native
frame/record contracts.
