# Phase 59d — Portfolio Ledger Slice: Exposures and Daily Transaction Bars

**Goal:** Close the remaining executable portfolio gaps from the Iteration 2
API audit: `get_sector_exposures`, `compute_style_factor_exposures`,
`compute_sector_exposures`, `compute_cap_exposures`,
`compute_volume_exposures`, and `daily_txns_with_bar_data`.

**Baseline:** `dev` after Phase 59c (oracle pin `4b8f31b`). Python sources:
`portfolio/positions.py` 167–560, `portfolio/transactions.py` 63–93.

**Files:** `include/fincore/metrics/positions.hpp`,
`src/metrics/positions.cpp`, `include/fincore/metrics/transactions.hpp`,
`src/metrics/transactions.cpp`, `tests/unit/positions_test.cpp`,
`tests/unit/transactions_test.cpp`.

## Native API

```cpp
struct sector_assignment { std::string symbol, sector; };
struct sector_definition { double id; std::string name; };
struct exposure_bundle { dataframe long_exposure, short_exposure,
                         gross_exposure, net_exposure; };

dataframe get_sector_exposures(positions, span<sector_assignment>, cash);
dataframe compute_style_factor_exposures(positions, risk_factors, cash);
exposure_bundle compute_sector_exposures(positions, sectors,
                                         span<sector_definition>, cash);
exposure_bundle compute_cap_exposures(positions, caps, cash);
exposure_bundle compute_volume_exposures(shares, volumes, percentile = 0.5);

struct daily_txn_bar { timestamp date; std::string symbol;
                       double amount, price, volume; };
std::vector<daily_txn_bar> daily_txns_with_bar_data(trades, prices, volumes);
```

## Contracts

- Sector sums (`get_sector_exposures`): group assets by the caller's
  assignment into ascending sector columns, then append `cash`; unmapped
  symbols are omitted (the source only warns).
- Style exposure: `sum(position * factor)/sum(|position|)` per row, NaN
  factors skipped, zero gross → 0, non-finite → 0.
- Panel exposures (sector/cap): long = positive sum / total long, short =
  negative sum / total short (negative sign retained), gross = |sum| /
  total gross, net = long − short; zero denominators and non-finite values
  become 0; definitions fix the column order. Cap buckets use the source's
  inclusive boundaries.
- Volume exposures: active nonzero shares require finite positive volumes;
  common columns only; `fraction = shares/volume`; per-row linear quantile ×
  100; zero/NaN → 0.
- Daily bars: absolute amount summed per (symbol, day), joined with that
  day's price and volume; output in symbol-major then date order, matching
  the oracle's groupby ordering.

## Tests

Oracle-exact sector sums, style series, sector/cap bundles, volume bundle
values plus validation errors, and four daily-bar records with ordering.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
