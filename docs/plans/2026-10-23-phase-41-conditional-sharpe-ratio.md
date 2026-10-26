# Phase 41 Native Conditional Sharpe Ratio Plan

**Goal:** Add native `conditional_sharpe_ratio`: annualized Sharpe over the
left tail at or below a percentile cutoff.

**Baseline:** `dev` after Phase 40 (oracle pin `4b8f31b`).
Python source: `fincore/metrics/ratios.py::conditional_sharpe_ratio`
(lines 314–367).

**Files:** `include/fincore/metrics/ratios.hpp`, `src/metrics/ratios.cpp`,
`tests/unit/ratios_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double conditional_sharpe_ratio(
    const dataframe& returns,
    double cutoff = 0.05,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view column = returns_column);
```

Python's `period` maps to the native annualization factor; `cutoff` is a
left-tail probability in `(0, 1)`.

## Exact control flow

```text
NUL selector check -> require_double_column
raw size < 2 -> NaN
not (0 < cutoff < 1) -> NaN
any NaN in raw -> NaN                     (NumPy percentile propagates NaN)
sort a copy ascending
virtual = (n-1) * cutoff; lerp NumPy linear semantics:
  t < 0.5  -> lo + (hi-lo)*t
  t >= 0.5 -> hi - (hi-lo)*(1-t)
tail = raw values <= cutoff (raw order, NaN already excluded)
tail size < 2 -> NaN
excess mean = mean(tail) - risk_free; sample std (ddof=1)
std == 0 or non-finite excess mean/std -> NaN
result = excess/std * sqrt(annualization)   (infinite annualization -> ±inf)
```

No alignment, no sorting of the returned sample, no mutation of the input.
Structural validation precedes the raw-count and cutoff gates.

## Tests

Exact five-parameter signature; finite goldens at cutoff 0.2/0.5, nonzero
risk-free, and annualization 52; raw/cutoff/tail-size gates (including the
default cutoff selecting a single observation); duplicate equality
boundaries; NaN/infinity cases including an infinity excluded from the tail;
infinite/NaN annualization and non-finite risk-free; selectors; NUL and
structural precedence; lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b` (exit 0, zero stderr, zero FAILURES);
full CTest; LLVM 22 ASan+UBSan; ImPlot compile/link; acceptance record and
README ledger.
