# Phase 50 Native Drawdown Table and Rank APIs Plan

**Goal:** Complete the Python drawdown-domain surface that the native tree
does not yet expose: iterative top-drawdown ranking, the padded drawdown
table value type, and the maximum-drawdown period pair. Underwater series,
recovery days/weeks/months, and second/third maxima already exist and are
reused.

**Baseline:** `dev` after Phase 49 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/drawdown.py` lines 223–438.

**Files:** `include/fincore/metrics/drawdown.hpp`,
`src/metrics/drawdown.cpp`, `tests/unit/drawdown_test.cpp`.

## Native API

```cpp
struct drawdown_table_row {
    double net_drawdown_percent = NaN;
    std::optional<timestamp> peak, valley, recovery;
    std::optional<std::size_t> duration_business_days;
};

std::vector<drawdown_window> get_top_drawdowns(
    const dataframe&, std::size_t top = 10, column = returns_column);
std::vector<drawdown_table_row> gen_drawdown_table(
    const dataframe&, std::size_t top = 10, column = returns_column);
std::pair<std::optional<timestamp>, std::optional<timestamp>>
max_drawdown_period(const dataframe&, column = returns_column);
```

## Exact control flow

- Ranking: cumulative wealth (NaN-as-zero compounding), running max,
  underwater; repeat `top` times — valley is the first argmin, peak is the
  last zero at or before the valley (else the first active row), recovery is
  the first zero at or after the valley; then keep `[0..peak] + [recovery..]`,
  or `[0..peak]` when unrecovered.
- Table: one row per requested `top`, padded with NaN/nullopt exactly like
  the pandas `range(top)` index; `net_drawdown_percent =
  -underwater[valley] * 100`; duration counts Monday–Friday calendar days
  inclusively between peak and recovery (pandas `freq="B"`).
- Period: first argmin valley, last zero peak, or `(first, first)` when the
  series never dips; empty input → `(nullopt, nullopt)`.

## Tests

Oracle-exact windows for three ranked drawdowns (one unrecovered), padded
monotone table, unrecovered table, period pairs for drawdown/monotone/empty,
selectors/NUL validation, and result ownership.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan; acceptance
record and README ledger.
