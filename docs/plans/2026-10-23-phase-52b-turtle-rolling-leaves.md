# Phase 52b — Residual Metric Ledger, Slice 2: Turtle R³ and Rolling Leaves

**Goal:** Close the remaining executable gaps from the Phase 39 ledger:
`r_cubed_turtle`, `roll_annual_volatility`, `roll_sortino_ratio`. The
Empyrical-compatible `roll_alpha_aligned`, `roll_beta_aligned`, and
`roll_alpha_beta_aligned` wrappers map to the already accepted native
`roll_alpha`, `roll_beta`, and `roll_alpha_beta` (label-aligned frames), and
are recorded as mappings rather than duplicated positional APIs.

**Baseline:** `dev` after Phase 52b slice 1 (oracle pin `4b8f31b`). Python
sources: `metrics/stats.py` lines 578–657; `metrics/rolling.py` lines 55–98.

**Files:** `include/fincore/metrics/stats.hpp`, `src/metrics/stats.cpp`,
`include/fincore/metrics/rolling.hpp`, `src/metrics/rolling.cpp`,
`tests/unit/stats_test.cpp`, `tests/unit/rolling_metrics_test.cpp`.

## Native API

```cpp
double r_cubed_turtle(const dataframe&, double annualization = 252,
                      column = returns_column);
dataframe roll_annual_volatility(const dataframe&, std::size_t window,
                                 frequency period = daily,
                                 std::optional<double> annualization = nullopt,
                                 column = returns_column);
dataframe roll_sortino_ratio(const dataframe&, std::size_t window,
                             double required_return = 0.0,
                             frequency period = daily,
                             std::optional<double> annualization = nullopt,
                             column = returns_column);
```

## Contracts

- Turtle R³: raw length < 2 → NaN; cumulative NAV with NaN positions
  retained as NaN; OLS slope of NAV vs position over non-NaN NAV rows
  (SciPy `linregress` slope); `rar = slope × annualization`; per-UTC-year
  maximum drawdown (absolute, native drawdown leaf); mean of those; average
  `< 1e-15` → `+inf` when `rar > 0`, else NaN; otherwise `rar / average`.
- Rolling leaves: endpoint index (window end) output frames; each window
  delegates to the accepted `annual_volatility` / `sortino_ratio` leaves,
  preserving their NaN-dropping and annualization semantics. No extra
  NaN-propagation gate (the oracle shows NaN-dropping windows produce
  values).

## Oracle evidence

Root `/tmp/fincore-phase52b2-python.eDN5xv`, detached at `4b8f31b`, module
path asserted, exit `0`, zero stderr bytes. Script SHA-256
`9455b669dd3eecdeb5676cf2e318726b482759f7dcb252e8c1462f1dd2773dec`; raw
output SHA-256
`b90a64e2cd38a88c96a4702937e35de26753d6312632d6e672e9c42a228ad6fe`.

Goldens: turtle 10-value `28.394903817964696`, 400-day (from 2019-11-01)
`-0.10352738136922`, singleton NaN; volatility window-3 array
`[0.6009991680526688, …, 0.42]`, NaN fixture
`[0.6734983296193096, …]`, annualization-52 array; Sortino window-3
`[-3.6660605559646724, -6.359601310784501e-16, …, inf, 0.0]`, NaN fixture
`[-8.979977728257458, …]`.

## Tests

Two stats tests (goldens, short/empty, selectors/NUL) and two rolling tests
(endpoint shapes, NaN/inf-aware value checks, annualization override, empty
and invalid windows).
