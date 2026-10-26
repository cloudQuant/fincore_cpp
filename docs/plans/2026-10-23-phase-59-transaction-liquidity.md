# Phase 59 Native Transaction Liquidity Plan

**Goal:** Port the three liquidity kernels from
`portfolio/transactions.py`: `days_to_liquidate_positions`,
`get_max_days_to_liquidate_by_ticker`, and
`get_low_liquidity_transactions` (and the long-ledger dependency they need).

**Baseline:** `dev` after Phase 57b (oracle pin `4b8f31b`). Python source:
`fincore/portfolio/transactions.py` lines 63–223.

**Files:** `include/fincore/metrics/transactions.hpp`,
`src/metrics/transactions.cpp`, `tests/unit/transactions_test.cpp`.

## Native API

```cpp
struct trade_record { timestamp date; std::string symbol; double amount; };
struct ticker_liquidity { std::string symbol; double days_to_liquidate;
                          double position_allocation_pct; };
struct transaction_liquidity { std::string symbol; timestamp date;
                               double max_percent_bar_consumed; };

dataframe days_to_liquidate_positions(positions, prices, volumes,
    max_bar_consumption = 0.2, capital_base = 1e6,
    mean_volume_window = 5, cash_column = "cash");

std::vector<ticker_liquidity> get_max_days_to_liquidate_by_ticker(
    positions, prices, volumes, ..., std::optional<size_t> last_n_days);

std::vector<transaction_liquidity> get_low_liquidity_transactions(
    std::span<const trade_record> transactions, prices, volumes,
    std::optional<size_t> last_n_days);
```

## Data representation

- `positions`, `prices`, `volumes` are wide frames with one double column per
  symbol and an identical timestamp index; positions may carry a `cash`
  column, excluded from allocation and liquidation outputs.
- The Python long transaction DataFrame (`symbol`, `amount`, ...) is carried
  as owning `trade_record` values; the market-data close price is taken from
  the `prices` frame, matching the source's join.

## Contract

```text
days_to_liquidate:
  allocation = position value / row sum (NaN-skipping), cash excluded
  dollar volume per symbol = price*volume; rolling mean over the previous w
    rows (shift(1)), any NaN in the window -> NaN, zero mean -> NaN
  rows 0..w-1 are dropped; day[r] = alloc[r]*capital/(max_bar*mean_dv[r])
ticker maximums:
  optional date filter > max_date - last_n_days; per symbol take the row with
  the largest finite days value and report the allocation percent of that row;
  output sorted by symbol
low liquidity:
  daily absolute amount per (symbol, day) / that day's volume * 100; optional
  date filter; per symbol take the maximum percent; output sorted by symbol
```

Parameter validation mirrors `CapacityConfig`: `0 < max_bar <= 1`,
`capital_base > 0`, `mean_volume_window >= 1`.

## Tests

Oracle-exact days matrix, ticker maximums (full and last-2-days), low
liquidity (full and last-2-days), and validation errors (window/bar/index
mismatch/NUL).

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
