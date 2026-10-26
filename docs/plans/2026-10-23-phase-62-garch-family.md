# Phase 62 Native GARCH Family Plan

**Goal:** Port the remaining GARCH-family surface: EGARCH, GJR-GARCH, and the
model-agnostic `forecast_volatility`, `conditional_var`, `conditional_es`
convenience functions.

**Baseline:** `dev` after Phase 61 (oracle pin `4b8f31b`). Python source:
`fincore/risk/garch.py` lines 295–760.

**Files:** `include/fincore/risk/garch.hpp`, `src/risk/garch.cpp`,
`tests/unit/risk_garch_test.cpp`.

## Native API

```cpp
struct egarch_parameters { double omega, alpha, gamma, beta; };
struct egarch_result { parameters; conditional_variances; residuals;
                       log_likelihood; converged; };
struct gjr_garch_parameters { double omega, alpha, gamma, beta; };
struct gjr_garch_result { ... };
enum class garch_model { garch, egarch, gjr_garch };

egarch_result fit_egarch(returns, column);
std::vector<double> forecast_egarch(result, horizon);
gjr_garch_result fit_gjr_garch(returns, column);
std::vector<double> forecast_gjr_garch(result, horizon);

struct garch_forecast_result { forecasts; log_likelihood; converged; };
garch_forecast_result forecast_volatility(returns, model, horizon, column);

struct garch_conditional_estimate { value; conditional_variances; converged; };
garch_conditional_estimate conditional_var(returns, model, alpha = 0.05,
                                           horizon = 1, column);
garch_conditional_estimate conditional_es(... same ...);
```

## Contracts

- EGARCH: `T >= 10` else `Insufficient data for EGARCH estimation`; requires
  finite returns with positive variance; params `(omega, alpha, gamma, beta)`
  initialized `(0.01, 0.1, -0.1, 0.95)`; bounds `alpha >= 1e-6`,
  `gamma in [-1, 1]`, `beta in [1e-6, 1]`; recursion
  `log s2_t = omega + alpha|z_{t-1}| + gamma z_{t-1} + beta log s2_{t-1}`
  with `z` standardized by the preceding conditional variance; burn-in one.
- GJR-GARCH: `T >= 10` else `Insufficient data for GJR-GARCH estimation`;
  init `(0.01, 0.05, 0.05, 0.9)`; bounds `omega >= 1e-6`, `alpha,gamma,beta
  in [1e-6 or 0, 1]`; recursion with the negative-shock indicator; burn-in
  one; stationarity `alpha + 0.5 gamma + beta < 1`.
- `forecast_volatility` dispatches on the enum; forecasts are conditional
  variances, with the source's persistence recursions.
- `conditional_var = z_alpha * sqrt(sum(forecast variances))`;
  `conditional_es = -sqrt(total) * pdf(z_alpha)/alpha`; `z_alpha` from a
  private erfc/Acklam+Newton normal quantile; `alpha` outside `(0,1)` → NaN.

## Optimizer

Phase 60 Nelder-Mead in transformed parameter space. EGARCH and GJR
likelihoods are multimodal, so each fit runs a fixed deterministic multi-start
set and keeps the best objective; an initial single-start EGARCH run settled
at `alpha` floor with loglik 745.4 versus the oracle's 749.63, which motivated
the multi-start.

## Tests

Oracle-tolerance fits for EGARCH (2e-3) and GJR (1e-4; gamma at the 0 bound),
forecast goldens, conditional VaR/ES values for EGARCH/GJR, closed-form VaR
composition for the native GARCH optimum (the source optimizer stalled there),
invalid-alpha NaN, short-input errors, NUL rejection, and horizon validation.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
