# Phase 42 Native Deflated Sharpe Ratio Plan

**Goal:** Add native `deflated_sharpe_ratio`: probability that the observed
Sharpe ratio survives multiple-testing and non-normality correction.

**Baseline:** `dev` after Phase 41 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/ratios.py::deflated_sharpe_ratio` (lines 1020–1134) with
bias-corrected sample moments at lines 1136–1162.

**Files:** `include/fincore/metrics/ratios.hpp`, `src/metrics/ratios.cpp`,
`tests/unit/ratios_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double deflated_sharpe_ratio(const dataframe& returns,
                             double risk_free = 0.0,
                             int num_trials = 1,
                             std::string_view column = returns_column);
```

Python's unused `period`/`annualization` parameters are absent.

## Exact control flow

```text
NUL selector check -> require_double_column
NaN-only clean values; clean size < 3 -> NaN
excess = clean - risk_free
sample mean/std (ddof=1); sr_hat = std > 1e-15 ? mean/std : 0
gamma3 = bias-corrected sample skewness (0 if n<3 or std==0)
gamma4 = bias-corrected excess kurtosis + 3 (0+3 if n<4 or std==0)
sr_var = 1 - gamma3*sr_hat + (gamma4-1)/4*sr_hat^2
N = max(num_trials, 1)
N <= 1 -> sr_star = 0
else sr_star = sqrt(max(sr_var,0)) * ((1-euler)*ppf(1-1/N) + euler*ppf(1-1/(N*e)))
sr_var <= 0 -> 1 if sr_hat > sr_star else 0
z = (sr_hat - sr_star) * sqrt(T-1) / sqrt(sr_var)
return normal_cdf(z)
```

Special functions are private `ratios.cpp` helpers: `erfc`-based CDF and the
Acklam inverse with one Newton refinement, under a declared 1e-9 absolute
error budget. No public special-function API, no new dependency.

## Tests

Exact signature; trial-count goldens (1/10/100), risk-free, trial counts
normalized to one; clean-count gates (including NaN dropping and T=3);
zero-variance 0.5; intentionally nonpositive-variance fixtures returning
exact 0/1; nonfinite returns and risk-free; selectors; NUL/structural
precedence; lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan; ImPlot
compile/link; acceptance record and README ledger.
