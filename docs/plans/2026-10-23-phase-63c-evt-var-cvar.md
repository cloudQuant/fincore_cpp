# Phase 63 Slice 3 — EVT VaR/CVaR and Extreme Risk (GPD Path) Plan

**Goal:** Add `evt_var`, `evt_cvar`, and `extreme_risk` for the GPD model with
the automatic threshold policy, leaving the GEV branches explicitly rejected
until the GEV slice.

**Baseline:** `dev` after Phase 63 slice 2 (oracle pin `4b8f31b`). Python
source: `fincore/risk/evt.py` lines 33–103 (threshold), 371–700.

**Files:** `include/fincore/risk/evt.hpp`, `src/risk/evt.cpp`,
`tests/unit/risk_evt_test.cpp`.

## Native API

```cpp
enum class evt_model { gpd, gev };
double evt_var(data, alpha = 0.05, model = gpd, tail = lower,
               threshold = nullopt, block_size = nullopt, column);
double evt_cvar(... same ...);
struct extreme_risk_result {
    double var, cvar, tail_index, threshold;
    std::size_t n_exceedances;
    double alpha;
};
extreme_risk_result extreme_risk(... same ...);
```

## Contract

- Alpha must be finite in `(0, 1)`; otherwise `input_error` with the source
  message.
- Automatic threshold: `max(10, floor(alpha*n)+1)` exceedances required; the
  90th percentile is retained when it leaves that many strict exceedances,
  otherwise the required order statistic one representable step below is
  used; too few tail observations raise the source message.
- GPD fit is re-run at the selected threshold with the MLE path (source
  behaviour); VaR/CVaR formulas transcribe the source, including
  `alpha > n_exceed/n_total` rejection and `xi >= 1` CVaR rejection.
- GEV branches raise `input_error{"GEV model is not available in this
  build"}` — an explicit unsupported-feature rejection for this slice, not a
  silent stub.

## Tests

GPD VaR/CVaR at 0.05/0.1 with a xi<1 fixture, extreme-risk GPD fields, and
alpha/GEV validation. A first fixture (xi driven to the 1.0 bound by the
source MLE defect) makes CVaR raise; a scan selected a moderate fixture
(amplitude 0.02, two shocks) with `xi ~ 0.72`.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger. `gev_fit` and the GEV branches remain
for Phase 63 slice 4.
