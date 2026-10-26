# Phase 44 Native Cal-Treynor Ratio Plan

**Goal:** Add native `cal_treynor_ratio`: annualized excess return divided by
beta versus a benchmark. `treynor_ratio` is a Python alias and is mapped, not
duplicated.

**Baseline:** `dev` after Phase 46 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/ratios.py::cal_treynor_ratio` (lines 606–700), alias at
`treynor_ratio`.

**Files:** `include/fincore/metrics/ratios.hpp`, `src/metrics/ratios.cpp`,
`tests/unit/ratios_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double cal_treynor_ratio(
    const dataframe& returns, const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

## Exact control flow

```text
NUL checks on both selectors -> align_on_common_index (inner)
aligned rows < 2 -> NaN
ann = annual_return(aligned returns, annualization)
b = beta(aligned returns, aligned benchmark, risk_free)
b == 0 or b < 0 or NaN -> NaN
result = (ann - risk_free) / b        (no finite filter)
```

The alias `treynor_ratio` is not declared natively; the acceptance records the
name mapping. No multi-column (2D) API is introduced.

## Tests

Positive-beta goldens (ordinary, risk-free, annualization 52); zero,
negative, and undefined beta gates; short input; aligned NaN pairs; named and
bounded selectors; NUL/structural precedence; lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan; ImPlot
compile/link; acceptance record and README ledger.
