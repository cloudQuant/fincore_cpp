# Phase 45 Native M-Squared Measure Plan

**Goal:** Add native `m_squared` (Modigliani M²): portfolio excess return
scaled by the benchmark-to-portfolio volatility ratio, plus the risk-free
rate.

**Baseline:** `dev` after Phase 44 (oracle pin `4b8f31b`, shared oracle).
Python source: `fincore/metrics/ratios.py::m_squared` (lines 748–826).

**Files:** `include/fincore/metrics/ratios.hpp`, `src/metrics/ratios.cpp`,
`tests/unit/ratios_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double m_squared(const dataframe& returns,
                 const dataframe& benchmark,
                 double risk_free = 0.0,
                 double annualization = daily_annualization,
                 std::string_view returns_column_name = returns_column,
                 std::string_view benchmark_column_name = benchmark_column);
```

## Exact control flow

```text
NUL checks on both selectors -> align_on_common_index (inner)
aligned rows < 2 -> NaN
ann = annual_return(aligned returns)
port_vol = annual_volatility(aligned returns)
bench_vol = annual_volatility(aligned benchmark)
port_vol == 0, port_vol < 0, or NaN -> NaN
result = (ann - risk_free) * (bench_vol / port_vol) + risk_free
```

Reuses native `annual_return` and `annual_volatility`; no volatility
reimplementation and no finite-result filter.

## Tests

Plain-benchmark goldens (ordinary, risk-free 0.001, annualization 52);
zero-portfolio-volatility NaN; short input NaN; aligned NaN-pair golden;
named/bounded selectors; NUL/structural precedence; lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b` (Phase 44 root); full CTest; LLVM 22
ASan+UBSan; ImPlot compile/link; acceptance record and README ledger.
