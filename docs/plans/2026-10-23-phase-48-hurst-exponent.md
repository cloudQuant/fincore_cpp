# Phase 48 Native Hurst Exponent Plan

**Goal:** Add native `hurst_exponent`: rescaled-range analysis over lagged
subseries with a log-log regression, including the source's short-series
fallback.

**Baseline:** `dev` after Phase 47 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/stats.py::hurst_exponent` (lines 126–222).

**Files:** `include/fincore/metrics/stats.hpp`, `src/metrics/stats.cpp`,
`tests/unit/stats_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double hurst_exponent(const dataframe& returns,
                      std::string_view column = returns_column);
```

## Exact control flow

```text
NUL selector check -> require_double_column
raw size < 8 -> NaN; NaN-only clean size < 8 -> NaN
mean and cumulative deviation path over clean values
range = max(cumsum) - min(cumsum); deviation = sample std (ddof=1)
deviation == 0 or range == 0 -> NaN
n = clean size; max_lag = max(n/3, 3); min_lag = 2
if (max_lag - 1) > 30: 30-point log10-spaced lags, truncated to int,
  unique, endpoint forced to max_lag (NumPy geomspace endpoint rule)
else: lags 2..max_lag
for each lag: subseries blocks of size lag; per block,
  range/sample-std ratio; mean over valid blocks (both positive)
< 2 lag entries -> fallback 0.5 + ln(range/deviation)/ln(2n), clamped [0,1]
else OLS slope of ln(lag) vs ln(ratio), clamped [0,1]
```

## Tests

Ordinary (clamped 1.0), 120-point sin and cos series goldens, short/constant/
zero gates, NaN-mixed, spike, exactly-eight-value clamp, selectors, NUL and
missing selectors, lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan; ImPlot
compile/link; acceptance record and README ledger.
