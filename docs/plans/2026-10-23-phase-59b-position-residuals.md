# Phase 59b — Portfolio Ledger Slice: Top Positions, Notional, Stack

**Goal:** Close three pure-position gaps from the Iteration 2 API audit:
`get_top_long_short_abs`, `get_long_short_notional`, and `stack_positions`.

**Baseline:** `dev` after Phase 58 (oracle pin `4b8f31b`). Python source:
`fincore/portfolio/positions.py` (66–96, 207–229, 509–536).

**Files:** `include/fincore/metrics/positions.hpp`,
`src/metrics/positions.cpp`, `tests/unit/positions_test.cpp`.

## Native API

```cpp
struct ranked_position { std::string symbol; double value; };
struct top_positions { std::vector<ranked_position> long_positions,
                       short_positions, absolute_positions; };
struct position_record { timestamp date; std::string symbol; double value; };

top_positions get_top_long_short_abs(positions, top = 10, cash = "cash");
std::pair<std::vector<double>, std::vector<double>>
    get_long_short_notional(positions, cash = "cash");
std::vector<position_record> stack_positions(positions,
    pos_in_dollars = true, cash = "cash");
```

## Contract

- Top: per non-cash column, max/min/abs-max skipping NaN; longs are
  positives sorted descending, shorts are negatives sorted ascending,
  absolutes sorted descending; each truncated to `top` (ties keep first by
  column order; goldens avoid ties).
- Notional: per row, sum of positive asset values and the absolute sum of
  negatives; NaN cells are skipped; cash excluded.
- Stack: row-major `(date, symbol, value)` over non-cash columns in frame
  column order; NaN cells are retained, matching the oracle's pandas
  behaviour. `pos_in_dollars` is inert in the source and is accepted but
  unused.

## Tests

Oracle-exact top lists, notional vectors, and a 12-entry stack including a
NaN cell; NUL selector rejection.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
