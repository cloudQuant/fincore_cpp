# Phase 52b — Residual Ledger Slice 3: Aggregate Consecutive Stats and Alpha Percentile Rank

**Goal:** Close the last two executable metric gaps found by the iteration-1
API audit: `consecutive_stats` (six-run aggregate) and
`alpha_percentile_rank` (peer-universe percentile).

**Baseline:** `dev` after Phase 52b slice 2 (oracle pin `4b8f31b`). Python
sources: `metrics/consecutive.py` 69–138; `metrics/alpha_beta.py` 805–860.

**Files:** `include/fincore/metrics/consecutive.hpp`,
`src/metrics/consecutive.cpp`, `include/fincore/metrics/alpha_beta.hpp`,
`src/metrics/alpha_beta.cpp`, `tests/unit/consecutive_test.cpp`,
`tests/unit/alpha_beta_test.cpp`.

## Native API

```cpp
std::map<std::string, double> consecutive_stats(
    const dataframe&, column = returns_column);

double alpha_percentile_rank(
    const dataframe& strategy_returns,
    std::span<const dataframe> peer_returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    returns_column = returns_column,
    benchmark_column = benchmark_column);
```

## Contract

- `consecutive_stats`: exactly the six keys
  `max_consecutive_up_days/down_days/up_weeks/down_weeks/up_months/down_months`,
  each delegating to the accepted leaves; empty input yields NaN values.
- `alpha_percentile_rank`: strategy length `< 3` → NaN; annualized alpha NaN
  → NaN; peers shorter than three rows are skipped; NaN peer alphas are
  excluded; no valid peers → NaN; otherwise `count(peer_alpha <
  strategy_alpha) / valid_peer_count`. Peer frames are passed as a span and
  never mutated or retained.

## Oracle evidence

Root `/tmp/fincore-phase52b3-python.UKksTC`, detached at `4b8f31b`, module
path asserted, exit `0`, zero stderr bytes. Script SHA-256
`5816ded8c37bf3114bb3d130f2cd882b026b844056d426a5360731658303bc98`; raw
output SHA-256
`259283123fa6155fea0024b082ac3247569741f50bc29f6d4ee1670e0e45e2cb`.

Goldens: the ten-day January 2020 fixture gives up/down days 3/1, weeks 2/0,
months 1/0; empty input NaN. Alpha percentile rank `0.6666666666666666`
(with and without a nonzero risk-free rate), short strategy NaN, no peers
NaN.

## Tests

`consecutive_stats` composition (dated index; an integer-index fixture all
falls in one week and was replaced), empty/NUL; alpha percentile rank
goldens, risk-free invariance, short strategy, empty peer span, and
ownership. The peer span includes a two-row peer that must be skipped.
