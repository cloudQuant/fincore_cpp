# Iteration 3 Close-Out — Risk and Optimization

**Oracle:** Python `4b8f31b1b59707480d5db9603704d83bbf7e6946`.
Status: **accepted** for the assigned risk and optimization slice.

## 1. Delivered phases

| Phase | Native delivery | Acceptance |
|---|---|---|
| 60 | `detail::{minimize_nelder_mead,project_onto_simplex,minimize_quadratic_on_simplex}` | 2026-10-23-phase-60-solver-spike |
| 61 | `fit_garch11`, likelihood, forecasts | 2026-10-23-phase-61-garch11 |
| 62 | EGARCH/GJR fits, `forecast_volatility`, `conditional_var/es`, `risk_estimate` + `forecast_var/es` | 2026-10-23-phase-62/62b |
| 63 | Hill, GPD (MLE/PWM), GEV, EVT VaR/CVaR, `extreme_risk`, aligned metric helpers | 2026-10-23-phase-63/63b/63c/63d |
| 64 | `beta_fragility_heuristic` (and aligned mapping) | 2026-10-23-phase-64 |
| 65 | Kupiec/Christoffersen, `backtest_var/es`, calibration helpers | 2026-10-23-phase-65/67 |
| 66 | `risk_model_spec`, `walk_forward_var` | 2026-10-23-phase-66 |
| 67 | `risk_validation_report`, deterministic JSON, spec validation | 2026-10-23-phase-67 |
| 68–71 | portfolio solver, objectives, efficient frontier, risk parity | 2026-10-23-phases-68-71 |

## 2. Capability ledger

Legend: `A` accepted existing/ported route, `M` mapped alias/consolidated,
`D` deferred with owner, `N` non-port.

### `risk/garch.py`

| Python route | Native route | Disposition |
|---|---|---|
| `GARCH`, `EGARCH`, `GJRGARCH` classes | `garch_model` enum + `fit_garch11/fit_egarch/fit_gjr_garch` | M |
| `GARCHResult`, `EGARCHResult`, `GJRGARCHResult` | `garch11_result`, `egarch_result`, `gjr_garch_result` | M |
| `forecast_volatility` | `forecast_volatility` + per-model forecast functions | A |
| `conditional_var`, `conditional_es` | same names | A |

### `risk/evt.py`

| Python route | Native route | Disposition |
|---|---|---|
| `hill_estimator` | `hill_estimator` | A |
| `gpd_fit` | `gpd_fit` (MLE/PWM) | A |
| `gev_fit` | `gev_fit` | A |
| `evt_var`, `evt_cvar`, `extreme_risk` | same names (GPD and GEV) | A |

### `risk/backtesting.py`, `risk/calibration.py`

| Python route | Native route | Disposition |
|---|---|---|
| `kupiec_lr`, `christoffersen_lr` | same names | A |
| `RiskBacktestResult` + status constants | `risk_backtest_result` + `risk_backtest_status` | A |
| `backtest_var`, `backtest_es` | same names | A |
| `expected_exception_count`, `basel_traffic_light`, `es_calibration_score` | same names | A |

### `risk/diagnostics.py`, `risk/specs.py`, `risk/report.py`, `risk/models.py`

| Python route | Native route | Disposition |
|---|---|---|
| `RiskModelSpec`, supported-value constants | `risk_model_spec`, `validate_risk_model_spec`, enum/name helpers | A |
| `WalkForwardVaRResult`, status constants | `walk_forward_var_result`, `walk_forward_status` | A |
| `walk_forward_var`, `HISTORICAL_QUANTILE_METHOD` | `walk_forward_var`, `"weibull"` | A |
| `RiskValidationReport`, `build_risk_validation_report`, disclosure/schema constants | `risk_validation_report`, `build_risk_validation_report`, constants | A |
| `RiskEstimate`, status/sign constants | `risk_estimate`, `risk_estimate_status`, sign constant | A |
| `forecast_var`, `forecast_es` | same names | A |

### `optimization/`

| Python route | Native route | Disposition |
|---|---|---|
| `objectives.optimize` | `optimize_portfolio` | A |
| `frontier.efficient_frontier` | `efficient_frontier` | A |
| `risk_parity.risk_parity` | `risk_parity` | A |
| `exceptions.OptimizationError` | `fincore::optimization_error` | A |
| `operations.operations` (OperationSpec registry) | — | D (Phase 92 runtime) |
| `_utils.validate_result`, `normalize_weights`, `check_feasibility`, `make_positive_semidefinite` | private solver validation/PSD check | M (private; no public route) |

### `metrics/risk.py` cross-owner routes

| Python route | Native route | Disposition |
|---|---|---|
| `annual_volatility`, `downside_risk`, `value_at_risk`, `conditional_value_at_risk`, `tail_ratio`, `tracking_error`, `residual_risk`, `var_excess_return`, `var_cov_var_normal`, `trading_value_at_risk` | accepted Iteration 1 metric routes | A |
| `gpd_risk_estimates` | `gpd_risk_estimates` | A |
| `gpd_risk_estimates_aligned` | `gpd_risk_estimates` (alias) | M |
| `beta_fragility_heuristic` | `beta_fragility_heuristic` (inner alignment) | A (inner policy only) |
| `beta_fragility_heuristic_aligned` | `beta_fragility_heuristic` (alias) | M |

Remaining `beta_fragility_heuristic` alignment policies (`outer`, timezone
normalization) are not exposed natively; the inner common-index policy is the
supported native contract, recorded here as a bounded gap rather than an
invented API.

## 3. Defect log and declared divergences

- **GPD MLE source defect (reproduced):** the pinned source minimizes the raw
  log-likelihood (wrong sign), driving the shape to its 1.0 bound; the native
  port reproduces this, and `evt_cvar` therefore raises for that fit. Not
  silently corrected.
- **GEV MLE local optima:** `scipy.stats.genextreme.fit` returns the `fmin`
  local optimum and GEV likelihoods are multimodal; the native port reproduces
  the same start/optimizer path and does not promise a global MLE.
- **GARCH objective-first:** the SciPy L-BFGS-B fit can stall below the
  native likelihood; acceptance compares likelihoods (`native >= oracle`)
  and forecasts within a declared budget, not parameter equality.
- **Optimizer tolerance parity:** SLSQP paths are not reproduced. Portfolio
  objectives/frontier/risk parity are compared by objective and feasibility
  within declared budgets; unique optima are also compared by weight.
- **Digests:** risk digests (`inputs_digest`) are versioned native SHA-256
  recipes, not pandas CSV bytes; report JSON carries them without claiming
  cross-language equality.
- **Timezone tokens:** the Phase 67 report emits `null`
  `timestamp_timezone` because the native index has no timezone metadata;
  tz-aware Python tokens are unreproduced. `timestamp_index_name` is an
  explicit builder argument.
- **Historical stress windows (Iteration 1 carry-over):** tz-aware Python
  indexes silently yield no ranges; native applies the UTC calendar and this
  divergence remains documented.
- **GPD/GEV source optimization limits:** reported parameters can sit at
  bounds; native validations fail closed rather than clipping.

## 4. Public headers and tests

Risk: `risk/{garch,evt,backtesting,specs,walk_forward,report,models}.hpp`.
Optimization: `optimization/{objectives,frontier,risk_parity}.hpp`.
Metrics: `metrics/risk.hpp` (cross-owner routes). Internal solver/adapters:
`detail/optimization.hpp`, `src/optimization/portfolio_solver.hpp`.

Tests: `risk_garch_test`, `risk_evt_test`, `risk_backtesting_test`,
`risk_walk_forward_test`, `risk_report_test`, `risk_models_test`,
`optimization_numerics_test`, `optimization_portfolio_test`, plus the
Iteration 1 risk metric tests.

## 5. Gates (current tree)

- Full headless CTest: **661/661 passed**.
- LLVM 22 ASan+UBSan: **661/661 passed**, no sanitizer reports.
- ImPlot compile/link unchanged.

## 6. Exit and residuals

Assigned Phases 60–71 (including 63 slices and 62/65 residuals) are
accepted. The only visible residual in this domain is runtime
`OperationSpec` registration for the three optimization routes, owned by
Iteration 6 Phase 92. GPD/GEV/GARCH results remain reference computations,
not regulatory approvals; no GIPS or risk-model certification is implied.
