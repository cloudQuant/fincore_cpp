# Phase 46 Native Sterling Ratio Plan

**Goal:** Add native `sterling_ratio`: annualized excess return divided by
the absolute average drawdown, with the source's downside-return and
population-deviation fallbacks.

**Baseline:** `dev` after Phase 43 (oracle pin `4b8f31b`). Phase 44/45 remain
open; phase numbering does not force execution order. Python source:
`fincore/metrics/ratios.py::sterling_ratio` (lines 827–888).

**Files:** `include/fincore/metrics/ratios.hpp`, `src/metrics/ratios.cpp`,
`tests/unit/ratios_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double sterling_ratio(const dataframe& returns,
                      double risk_free = 0.0,
                      double annualization = daily_annualization,
                      std::string_view column = returns_column);
```

## Exact control flow

```text
NUL selector check -> require_double_column; raw size < 2 -> NaN
drawdowns = get_all_drawdowns(returns, column)
if empty or every entry == 0:
    clean = NaN-only values; downside = clean < 0
    average = downside empty ? max(|population_std(clean)|, 1e-10)
                             : |mean(downside)|
else:
    average = |mean(drawdowns)|
ann = annual_return(returns, annualization, column)
average == 0 or < 1e-10 -> +infinity if ann - risk_free > 0 else NaN
result = (ann - risk_free) / average
```

## Tests

Drawdown and fallback goldens; risk-free/annualization variants; monotone,
flat, constants, tiny drawdown, NaN-mixed, all-NaN, ±infinity (including the
signed-zero result for a negative-infinite sample); raw-count and nonfinite
risk-free gates; selectors; NUL/structural precedence; lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b` (shared with Phase 43's script); full
CTest; LLVM 22 ASan+UBSan; ImPlot compile/link; acceptance record and README
ledger.
