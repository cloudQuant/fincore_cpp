# Phase 43 Native Burke Ratio Plan

**Goal:** Add native `burke_ratio`: annualized excess return divided by the
square root of summed squared drawdown episodes, with the source's
no-drawdown fallback to downside-return dispersion.

**Baseline:** `dev` after Phase 42 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/ratios.py::burke_ratio` (lines 889–960).

**Files:** `include/fincore/metrics/ratios.hpp`, `src/metrics/ratios.cpp`,
`tests/unit/ratios_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double burke_ratio(const dataframe& returns,
                   double risk_free = 0.0,
                   double annualization = daily_annualization,
                   std::string_view column = returns_column);
```

## Exact control flow

```text
NUL selector check -> require_double_column; raw size < 2 -> NaN
drawdowns = get_all_drawdowns(returns, column)      (accepted native kernel)
if drawdowns empty or every entry == 0:
    clean = NaN-only values; downside = clean < 0
    risk = downside empty ? max(population_std(clean), 1e-10)
                          : population_std(downside)
else:
    risk = sqrt(sum(dd^2))
ann = annual_return(returns, annualization, column)   (CAGR; NaN-as-zero
                                                      compounding, raw count)
risk == 0 or risk < 1e-10 -> +infinity if ann - risk_free > 0 else NaN
result = (ann - risk_free) / risk
```

No finite-result filter: infinities from the risk gate are intentional.

## Tests

Signature; drawdown-path and fallback-path goldens; risk-free and
annualization variants; monotone, flat, constant, tiny-drawdown, NaN-mixed,
all-NaN, and ±infinity classes; raw-count and nonfinite-risk-free gates;
selectors; NUL/structural precedence; lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan; ImPlot
compile/link; acceptance record and README ledger.
