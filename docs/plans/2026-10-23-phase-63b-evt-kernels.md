# Phase 63 Slice 2 — EVT Hill and GPD Fits Plan

**Goal:** Port the deterministic EVT kernels from `fincore/risk/evt.py`:
`hill_estimator` and `gpd_fit` (MLE and PWM), including threshold defaults
and validation.

**Baseline:** `dev` after Phase 63 slice 1 (oracle pin `4b8f31b`). Python
source: `fincore/risk/evt.py` lines 104–300.

**Files:** `include/fincore/risk/evt.hpp` (new), `src/risk/evt.cpp` (new),
`CMakeLists.txt`, `tests/unit/risk_evt_test.cpp` (new), `tests/CMakeLists.txt`.

## Native API

```cpp
enum class evt_tail { lower, upper };
enum class gpd_estimation_method { mle, pwm };
struct hill_estimate { double xi; std::vector<double> tail_observations; };
struct gpd_parameters { double xi, beta, threshold; std::size_t n_exceed; };

hill_estimate hill_estimator(data, threshold = nullopt,
                             tail = upper, column);
gpd_parameters gpd_fit(data, threshold = nullopt,
                       method = mle, tail = lower, column);
```

## Contract

- Finite-value filtering; lower-tail magnitudes are reflected losses,
  upper-tail magnitudes are gains; missing selected tail raises the source
  messages for GPD (Hill allows an empty tail and then fails the 10-exceedance
  gate).
- Default threshold is the 90th linear percentile of the selected magnitudes;
  explicit thresholds must be finite and positive.
- Exceedances are strict (`> threshold`); fewer than 10 raises.
- Hill: `mean(log(observation/threshold))` over strict exceedances.
- GPD PWM: L-moment formulas `b0`, `b1`, `l2 = 2b1-b0`,
  `xi = 2 - b0/l2`, `beta = b0*(1-xi)`, with finite positive-scale checks.
- GPD MLE: reproduces the **source objective**, which minimizes the raw GPD
  log-likelihood (not its negative) under bounds `xi in [-0.5, 1]`,
  `beta >= 1e-6`, starting from `[0.1, population std]`; the bound search
  uses transform + Nelder-Mead at the source's default 1e-4 tolerances.
  This drives `xi` to its upper bound on the oracle fixture; that is recorded
  as source behaviour rather than silently corrected.

## Tests

Exact Hill xi/tail sizes for both tails, exact PWM parameters, MLE
xi near the 1.0 bound with positive beta, validation errors (too few
exceedances, invalid threshold, missing tail, NUL).

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger. `evt_var`, `evt_cvar`, `gev_fit`, and
`extreme_risk` remain for Phase 63 slice 3.
