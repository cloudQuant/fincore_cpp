# Iteration 1 Close-Out — Metrics Completion

**Iteration:** iteration-1-metrics-completion (Phases 38, 39, 40–52, 52b)
**Oracle pin:** `4b8f31b` (re-pinned in Phase 39)
**Head evidence:** 572/572 headless CTest and 572/572 LLVM 22 ASan+UBSan at
the Phase 52b slice-3 acceptance head.

## 1. Phase ledger

| Phase | Deliverable | Acceptance record |
|---|---|---|
| 38 | `adjusted_sharpe_ratio` | 2026-10-23-phase-38-adjusted-sharpe-ratio-acceptance.md |
| 39 | Oracle re-pin + drift census | 2026-10-23-phase-39-oracle-repin-acceptance.md |
| 40 | `mar_ratio` | 2026-10-23-phase-40-mar-ratio-acceptance.md |
| 41 | `conditional_sharpe_ratio` | 2026-10-23-phase-41-conditional-sharpe-ratio-acceptance.md |
| 42 | `deflated_sharpe_ratio` | 2026-10-23-phase-42-deflated-sharpe-ratio-acceptance.md |
| 43 | `burke_ratio` | 2026-10-23-phase-43-burke-ratio-acceptance.md |
| 44 | `cal_treynor_ratio` | 2026-10-23-phase-44-cal-treynor-ratio-acceptance.md |
| 45 | `m_squared` | 2026-10-23-phase-45-m-squared-acceptance.md |
| 46 | `sterling_ratio` | 2026-10-23-phase-46-sterling-ratio-acceptance.md |
| 47 | `up/down_capture_return` | 2026-10-23-phase-47-capture-returns-acceptance.md |
| 48 | `hurst_exponent` | 2026-10-23-phase-48-hurst-exponent-acceptance.md |
| 49 | `stutzer_index` | 2026-10-23-phase-49-stutzer-index-acceptance.md |
| 50 | drawdown table/rank APIs | 2026-10-23-phase-50-drawdown-table-acceptance.md |
| 51 | `extract_interesting_date_ranges` | 2026-10-23-phase-51-interesting-date-ranges-acceptance.md |
| 52 | distribution stats + bootstrap | 2026-10-23-phase-52-perf-stats-bootstrap-acceptance.md |
| 52b.1 | weekly/monthly consecutive | 2026-10-23-phase-52b-consecutive-weeks-months-acceptance.md |
| 52b.2 | turtle R³ + rolling leaves | 2026-10-23-phase-52b-turtle-rolling-leaves-acceptance.md |
| 52b.3 | consecutive aggregate + alpha percentile | 2026-10-23-phase-52b-aggregate-percentile-acceptance.md |

## 2. Public-API audit method

Enumerated every `def name(` in `fincore/metrics/*.py` (166 names) and every
function name declared in `include/fincore/metrics/*.hpp` (143 names). The 47
Python names without a same-named C++ declaration were classified by code
role at the pinned revision. No name is left unclassified.

### 2a. Implemented under a different native name or mapped (19)

| Python | Native mapping |
|---|---|
| `value_at_risk` | `historical_value_at_risk` |
| `cum_returns` / `cum_returns_final` | `cumulative_returns` / `cumulative_return` |
| `get_max_drawdown` | `max_drawdown_window` |
| `get_max_drawdown_period` | `max_drawdown_period` (Phase 50) |
| `get_max_drawdown_underwater` | underwater input; native windows consume returns via `max_drawdown_window` |
| `treynor_ratio` | alias of `cal_treynor_ratio` (Phase 44) |
| `stock_market_correlation` / `futures_market_correlation` / `bond_market_correlation` | `safe_correlation` |
| `alpha_aligned` / `beta_aligned` / `alpha_beta_aligned` | `alpha` / `beta` on label-aligned frames |
| `roll_alpha_aligned` / `roll_beta_aligned` / `roll_alpha_beta_aligned` | `roll_alpha` / `roll_beta` / `roll_alpha_beta` |
| `calc_bootstrap` | `simulation::bootstrap_distribution` |
| `consecutive_stats` | implemented in Phase 52b.3 |
| `alpha_percentile_rank` | implemented in Phase 52b.3 |

### 2b. Private implementation kernels (8)

`beta_from_moments`, `mean_return_from_moments`, `sharpe_from_moments`,
`sortino_from_moments`, `volatility_from_moments`,
`roll_alpha_beta_vectorized`, `roll_max_drawdown_chunked`, `rolling_window`.
These live in `_rolling_moments.py` / `_rolling.py` and exist to vectorize the
public leaves; native leaves compute the same quantities directly. No public
name is dropped.

### 2c. Utilities / container adapters (5, non-port)

`aligned_series`, `ensure_datetime_index_series`, `flatten`, `to_pandas`,
`pandas_frequency`. These convert between pandas/numpy containers or expose
frequency strings; the native boundary is a timestamped `StdDataFrame` and a
`frequency` enum.

### 2d. Bayesian family (11, non-port)

`compute_bayes_cone`, `compute_consistency_score`, `forecast_cone_bootstrap`,
`model_best`, `model_returns_normal`, `model_returns_t`,
`model_returns_t_alpha_beta`, `model_stoch_vol`, `run_model`,
`simulate_paths`, `summarize_paths`. PyMC has no C++ equivalent; the accepted
Phase 28 Bayesian return cone is the native Bayesian surface. This is the
roadmap's explicit non-port.

### 2e. Cross-iteration risk routes (4)

`gpd_risk_estimates` (+ `_aligned`) and `beta_fragility_heuristic`
(+ `_aligned`) are owned by Iteration 3 Phases 63–64 and remain visible
dependencies. Iteration 1 therefore closes its **assigned slice**, not the
entire metrics domain; complete metrics parity waits for those two routes.

## 3. Exit-criteria check

- [x] Every assigned phase (38–52b) accepted with plan, design, oracle
      evidence, tests, acceptance record, README entry.
- [x] Phase 39 drift resolved with a measured zero-changed-files census over
      the ported domains.
- [x] Public leaf registrations and test translation units updated per phase;
      headers audited above.
- [x] Full CTest and ASan+UBSan evidence at the iteration head (572/572 each).
- [x] Residual ledger closed to the extent owned by this iteration; the four
      risk routes are explicitly assigned to Iteration 3.
- [ ] Not claimed: complete `metrics` domain parity (blocked on Phases 63–64),
      rendering, Python bindings, or push/release actions.

## 4. Open items / hand-off notes

- The Python `extract_interesting_date_ranges` tz-aware defect is documented
  in Phase 51; no source fix is attempted.
- Special-function and solver tolerances are declared per phase (1e-9 for
  deflated Sharpe, 1e-6 for Stutzer); no blanket bit-exact claim is made for
  iterative algorithms.
- Existing uncommitted review documents under `docs/iterations/` remain
  untouched by this close-out and require the owner's review before commit.
