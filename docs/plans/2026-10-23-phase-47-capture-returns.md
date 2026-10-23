# Phase 47 Native Capture-Return Plan

**Goal:** Add native `up_capture_return` and `down_capture_return`: the
annualized strategy return over periods with a positive (negative) benchmark
return.

**Baseline:** `dev` after Phase 45 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/ratios.py` lines 1405–1516. These are return slices distinct
from the existing capture *ratios*; no `up_down_capture_return` exists at
this pin.

**Files:** `include/fincore/metrics/ratios.hpp`, `src/metrics/ratios.cpp`,
`tests/unit/ratios_test.cpp`, `tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double up_capture_return(const dataframe& returns, const dataframe& benchmark,
                         double annualization = daily_annualization,
                         returns_column, benchmark_column);
double down_capture_return(/* same */);
```

## Exact control flow

```text
NUL checks on both selectors -> align_on_common_index (inner)
select aligned strategy returns where benchmark > 0 (up) or < 0 (down)
empty selection -> NaN
CAGR over the selection: product(1+v, NaN->0),
  wealth <= 0 -> -1, else wealth^(annualization/selection_size) - 1
```

There is **no minimum selection size**: a single up-market observation yields
`(1+r)^annualization - 1` (oracle `11.274002099240244` for one 1% period).

## Tests

Ordinary and annualization-52 goldens; empty-window NaNs; single-observation
up-market value and down-market NaN; NaN benchmark/return pairs; null-
benchmark negative windows; named selectors; NUL and missing-column errors;
lifetime ownership.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan; ImPlot
compile/link; acceptance record and README ledger.
