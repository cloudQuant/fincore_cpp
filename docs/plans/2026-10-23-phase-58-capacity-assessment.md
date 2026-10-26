# Phase 58 Native Capacity Assessment Plan

**Goal:** Add `capacity_config`, `capacity_assessment`, and `assess_liquidity`
composing the Phase 59 kernels.

**Baseline:** `dev` after Phase 59 (oracle pin `4b8f31b`). Python source:
`fincore/portfolio/capacity.py` (whole module).

**Files:** `include/fincore/portfolio/capacity.hpp` (new),
`src/portfolio/capacity.cpp` (new), `CMakeLists.txt`,
`tests/unit/capacity_test.cpp` (new), `tests/CMakeLists.txt`.

## Native API

```cpp
struct capacity_config {
    double max_bar_consumption = 0.2;
    double capital_base = 1'000'000.0;
    std::size_t mean_volume_window = 5U;
    std::optional<std::size_t> last_n_days;
};
struct capacity_assessment {
    dataframe liquidation_days;
    std::vector<ticker_liquidity> ticker_maximums;
    std::vector<transaction_liquidity> low_liquidity_transactions;
};
capacity_assessment assess_liquidity(positions, transactions, prices,
                                     volumes, config = {});
```

## Contract

- Config validation mirrors the Python dataclass: `0 < max_bar <= 1`,
  `capital_base > 0`, `mean_volume_window >= 1`, `last_n_days >= 1` when
  present — all `input_error`.
- Composition: liquidation days (full grid), ticker maximums (date-filtered),
  and low-liquidity transactions (date-filtered) from the Phase 59 kernels.
- No invented capacity sweep or max-commission/max-slippage API.

## Correction discovered during this phase

The Phase 59 ticker-maximum filter was implemented as `date > max - n days`,
but the source slices `dtlp.loc[cutoff:]` — **inclusive** of the cutoff date.
The low-liquidity filter remains strict `date > md`. The native kernel was
corrected and the Phase 59 test strengthened with the AAA maximum
(`55.47177006951307`, allocation `13.008130081300814`). This is recorded as a
correction, not a silent rewrite.

## Tests

Composition equality against the oracle values (liquidation grid, ticker
maximums, low-liquidity rows) and the four config-validation error classes.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
