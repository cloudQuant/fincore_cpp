# Phase 52b — Residual Metric Ledger, Slice 1: Weekly/Monthly Consecutive Runs

**Goal:** Close the first residual gap from the Phase 39 capability ledger:
`max_consecutive_up_weeks`, `max_consecutive_down_weeks`,
`max_consecutive_up_months`, `max_consecutive_down_months`.

**Baseline:** `dev` after Phase 52 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/consecutive.py` lines 52–70 and 252–330.

**Files:** `include/fincore/metrics/consecutive.hpp`,
`src/metrics/consecutive.cpp`, `tests/unit/consecutive_test.cpp`.

## Scope boundary

This is slice 1 of Phase 52b. Remaining ledger items after it: `r_cubed_turtle`,
`roll_annual_volatility`, `roll_sortino_ratio`, and aligned rolling
alpha/beta variants. The phase stays open until those slices land; no
"complete metrics domain" claim is made here.

## Native API

```cpp
double max_consecutive_up_weeks(const dataframe&, column = returns_column);
double max_consecutive_down_weeks(const dataframe&, column = returns_column);
double max_consecutive_up_months(const dataframe&, column = returns_column);
double max_consecutive_down_months(const dataframe&, column = returns_column);
```

## Contract

- Empty input → NaN.
- Bucket daily rows by pandas `W` (Sunday-ending week, right-closed) or
  `ME` (calendar month), in ascending calendar order.
- Bucket return = `prod(1 + value, NaN-as-zero) - 1` (the source's
  `cum_returns_final` applied per resample bucket).
- Return the longest run of strictly positive (or strictly negative) bucket
  returns; zero-return buckets break runs.

## Tests

Oracle goldens for a five-month daily fixture (up months 2, down months 1,
up weeks 8, down weeks 5) and a six-week Monday-to-Sunday fixture (up weeks
2, down weeks 2); empty/NaN, selector/NUL, and ownership coverage.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
