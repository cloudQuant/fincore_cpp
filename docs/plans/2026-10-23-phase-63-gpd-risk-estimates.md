# Phase 63 Slice 1 — GPD Risk Estimates Plan

**Goal:** Port `gpd_risk_estimates` from `fincore/metrics/risk.py`: fit a GPD
to the loss tail with a descending threshold loop, then report threshold,
scale, shape, VaR, and ES.

**Baseline:** `dev` after Phase 62 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/risk.py` lines 501–700.

**Files:** `include/fincore/metrics/risk.hpp`, `src/metrics/risk.cpp`,
`tests/unit/risk_test.cpp`.

## Native API

```cpp
struct gpd_risk_estimates_result {
    double threshold, scale, shape, var, es;
};
gpd_risk_estimates_result gpd_risk_estimates(
    const dataframe& returns, double var_p = 0.01, column = returns_column);
```

## Contract

- Raw length `< 3` → all-zero result (the aligned wrapper is a mapping).
- Losses are `-return` restricted to positive values.
- Threshold starts at `0.2`; while above `1e-9`: fit the GPD tail with
  Nelder-Mead from `[1, 1]` with the source's default optimizer tolerances
  (`1e-4` on function and simplex, 400 iterations for two parameters);
  accept when the optimizer converges, `shape > 0`, and the closed-form VaR
  is positive; otherwise halve the threshold.
- VaR: `threshold + (scale/shape)*((n/exceedances)*p)^(-shape) - 1)`.
- ES: `var/(1-shape) + (scale - shape*threshold)/(1-shape)` unless the
  denominator is zero.
- Failure returns the zero-initialized result.

## Tests

Oracle-scale 200-point heavy-tail fixture (threshold 0.1, scale/shape/VaR/ES
goldens), short-input zeros, ordering invariants, and NUL rejection.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger. `risk/evt.py`'s GPD/GEV/VaR surface is
the remaining slice of Phase 63.
