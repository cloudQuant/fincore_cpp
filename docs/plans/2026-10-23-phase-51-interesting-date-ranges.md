# Phase 51 Native Interesting Date Ranges Plan

**Goal:** Add native `extract_interesting_date_ranges`: slices of the input
frame for the pinned historical crisis/regime windows.

**Baseline:** `dev` after Phase 50 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/timing.py` (lines 281–307) with static data in
`fincore/metrics/_historical_periods.py`.

**Files:** `include/fincore/metrics/timing.hpp`, `src/metrics/timing.cpp`,
`tests/unit/timing_test.cpp`.

## Native API

```cpp
struct named_returns {
    std::string name;
    dataframe returns;
};

std::vector<named_returns> extract_interesting_date_ranges(
    const dataframe& returns,
    std::string_view column = returns_column);
```

## Exact control flow

```text
NUL selector check -> require_double_column
for each pinned period in insertion order:
    collect rows with start <= index <= end (inclusive both ends)
    empty -> skip
    append {name, frame owning the selected rows and values}
```

Twenty periods are ported verbatim from `_historical_periods.py` (Dotcom …
New Normal). The native index is UTC-ns, so windows are applied as UTC
calendar dates.

## Source-behaviour note

The Python function converts the index with `pd.Timestamp` and slices with
naive label strings. On a tz-aware (UTC) index that slice raises inside the
per-period `try` and **all** ranges are silently skipped (oracle `count=0`).
The native API applies the documented UTC calendar instead; the divergence
is recorded rather than reproduced. Oracle goldens therefore come from the
tz-naive working path.

## Tests

78-day 2008 fixture yields Lehman (62 rows, .007…0.068), Sept08 (31 rows),
GFC Crash (78 rows) in pinned order with boundary rows included; a 2020
two-day fixture yields a single New Normal window; empty input → empty;
selector/NUL/lifetime coverage.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
