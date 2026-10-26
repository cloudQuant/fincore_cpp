# Phase 59c — Portfolio Ledger Slice: Round-Trip Statistics

**Goal:** Add `gen_round_trip_stats` over the native round-trip frame,
completing the Python round-trip presentation surface. The Python helpers
`agg_all_long_short` (the aggregation engine), `groupby_consecutive`,
`add_closing_transactions`, and `apply_sector_mappings_to_round_trips` are
construction/preprocessing steps; native `groupby_consecutive_transactions`,
`extract_round_trips`, and the frame contract cover them and are recorded as
mappings rather than duplicated APIs.

**Baseline:** `dev` after Phase 59b (oracle pin `4b8f31b`). Python sources:
`portfolio/round_trips.py` 387–470 and `portfolio/_presentation.py`.

**Files:** `include/fincore/metrics/round_trips.hpp`,
`src/metrics/round_trips.cpp`, `tests/unit/round_trips_test.cpp`.

## Native API

```cpp
struct round_trip_stat_table {
    std::vector<std::string> row_labels;
    std::vector<std::string> column_labels;
    std::vector<std::vector<double>> values;
};
struct round_trip_stats {
    round_trip_stat_table pnl, summary, duration, returns;
    std::vector<std::string> symbol_labels;
    std::vector<std::string> symbol_stat_labels;
    std::vector<std::vector<double>> symbol_values;
};
round_trip_stats gen_round_trip_stats(const dataframe& round_trips, ...);
```

## Contract

- Rows: `All trades`, `short` (long == false), `long` (long == true);
  absent groups are omitted; empty input returns empty tables.
- PNL: total/gross profit/gross loss/profit factor (NaN when no losses),
  means, winning/losing means, win:loss ratio, largest win/loss.
- SUMMARY: count, percent profitable, winners, losers, even.
- DURATION (days as double, declared unit): mean/median/longest/shortest,
  plus group-aware trips per day and per month using the group's
  `close_dt.max - open_dt.min` span (21 trading days per month).
- RETURNS: mean/median for all/winning/losing, largest win/loss.
- SYMBOLS: per symbol sorted ascending, the eight return statistics with
  `max`/`min` labels matching the source's symbol table.

## Tests

One round-trip test with oracle-exact PNL/SUMMARY/DURATION/RETURNS/SYMBOLS
values (duration converted from Timedelta to days in the oracle), group
ordering, absent-loss NaN classes, and the empty-input guard.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
