# Phase 57 Native Sharpe Inference Plan

**Goal:** Add the three inference leaves: standard error of the mean, Sharpe
standard error, and the Sharpe confidence interval.

**Baseline:** `dev` after Phase 56 (oracle pin `4b8f31b`). Python source:
`fincore/performance/inference.py` (whole module).

**Files:** `include/fincore/performance/inference.hpp` (new),
`src/performance/inference.cpp` (new), `CMakeLists.txt`,
`tests/unit/performance_inference_test.cpp` (new), `tests/CMakeLists.txt`.

## Native API

```cpp
double standard_error_of_mean(const dataframe& values, column = returns_column);
double sharpe_standard_error(const dataframe& returns, double risk_free = 0.0,
                             column = returns_column);
std::pair<double, double> sharpe_confidence_interval(
    const dataframe& returns, double risk_free = 0.0, double z = 1.96,
    column = returns_column);
```

## Contract

- All functions drop NaN only; fewer than two clean values → NaN (both CI
  bounds NaN).
- Sample standard deviation (ddof=1).
- Sharpe `sr = mean/std` when `std > 1e-15`, else `0`.
- `SE(SR) = sqrt((1 + 0.5*sr^2)/n)` (Lo 2002).
- CI = `(sr - z*SE, sr + z*SE)`; the SE is recomputed on the already
  risk-free-adjusted values, matching the source's call.
- No finite-result filtering; `z` is a free scalar (NaN propagates).

## Tests

Oracle goldens for the ordinary fixture, risk-free and `z=2.58` variants,
NaN-mixed, singleton, constant (SE `0.5`), selector/NUL, and ownership.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
