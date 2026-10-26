# Phase 64 Native Beta Fragility Heuristic Plan

**Goal:** Add `beta_fragility_heuristic` from `metrics/risk.py`.

**Baseline:** `dev` after Phase 63 slice 3 (oracle pin `4b8f31b`). Python
source: `fincore/metrics/risk.py` lines 702–768.

**Files:** `include/fincore/metrics/risk.hpp`, `src/metrics/risk.cpp`,
`tests/unit/risk_test.cpp`.

## Native API

```cpp
double beta_fragility_heuristic(
    const dataframe& returns, const dataframe& benchmark,
    returns_column = returns_column, benchmark_column = benchmark_column);
```

## Contract

```text
NUL checks -> inner alignment; either side < 3 -> NaN
drop NaN pairs; stable sort by benchmark factor
start = first, mid = nearbyint(n/2) (banker's rounding like np.around),
end = last
if factor range != 0: start_weight = (mid_f-start_f)/range,
                      end_weight = (end_f-mid_f)/range
else both weights = 0.5
result = start_weight*start_r + end_weight*end_r - mid_r
```

Empty filtered pairs return NaN (the source would index-error; the native
boundary chooses a defined NaN class and records it).

## Tests

Oracle goldens for the ordinary pair `0.0125`, short input NaN, NaN-pair
`-0.02`, constant factor `-0.005`, two-clean-pair `-0.02`, and NUL rejection.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
