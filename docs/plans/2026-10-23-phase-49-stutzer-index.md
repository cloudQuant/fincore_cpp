# Phase 49 Native Stutzer Index Plan

**Goal:** Add native `stutzer_index`: exponent-tilted index with a bounded
scalar optimization, replacing the LPM1 misconception with the source's
actual bounded minimization.

**Baseline:** `dev` after Phase 48 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/stats.py::stutzer_index` (lines 226–290). Phase 49 selects
its own bounded minimizer and does not wait for Phase 60's solver decision.

**Files:** `include/fincore/metrics/stats.hpp`, `src/metrics/stats.cpp`,
`tests/unit/stats_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double stutzer_index(const dataframe& returns,
                     double target_return = 0.0,
                     std::string_view column = returns_column);
```

## Exact control flow

```text
NUL selector check -> require_double_column; raw < 2 -> NaN
excess = raw - target; drop NaN only; < 2 -> NaN
mean excess; |mean| < 1e-15 -> 0.0
bounds: mean > 0 -> (-50, -1e-10), else (1e-10, 50)
objective(theta) = ln(mean(exp(theta*excess))); any nonfinite term -> 0.0
minimize objective by golden-section search to |b-a| <= 1e-12
ip = max(-objective(theta*), 0)
return sign(mean) * sqrt(2*ip)
```

## Numerical policy

SciPy `minimize_scalar(method="bounded")` defaults to `xatol=1e-5`; the
native solver is converged to `1e-12`, so agreement is limited by the flat
minimum. Declared tolerance: **1e-6 absolute** on the index. At a smooth
minimum, a 1e-5 optimum error contributes O(1e-10) to the objective and
therefore well under 1e-6 to the index; the constant-return fixture's oracle
`0.999999938...` is the explicit witness (native returns 1.0).

## Tests

Both signs with and without a nonzero target; zeros and short/clean gates;
NaN mixtures; constants; an overflow-guard fixture; ±infinity samples;
selectors; NUL/missing errors; lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan; ImPlot
compile/link; acceptance record and README ledger.
