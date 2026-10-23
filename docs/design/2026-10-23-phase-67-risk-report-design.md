# Phase 67 Design — Risk Spec and Validation Report

## Source contract

Pinned routes `fincore/risk/specs.py`, `fincore/risk/diagnostics.py`,
`fincore/risk/report.py`, and the calibration helpers used by the report
payload (`fincore/risk/calibration.py`).

### `RiskModelSpec`

Frozen value with fields `forecast_target`, `confidence_level`, `horizon`,
`distribution`, `tail`, `sign_convention`, `window`, `refit_cadence`,
`model_version`.

- Supported sets: target `{"var","es","pair"}`, distribution
  `{"historical","normal"}`, tail `{"lower","upper"}`, sign
  `{"losses_negative"}`, version `{"1.0"}`.
- `confidence_level` must be a finite real in `(0,1)`; booleans and strings
  are rejected before conversion. `Decimal("0.95")` normalizes to `0.95`.
- `horizon >= 1`, `window >= 2`, `refit_cadence >= 1`; booleans fail the
  integer test.
- Validation error classes/messages are part of the contract (oracle lists
  sixteen rejection cases).

### `RiskValidationReport`

Frozen record with `status`, `inputs_digest`, `specification`,
`forecast_events`, `refits`, `diagnostics`, `backtest`,
`timestamp_index_name`, `timestamp_timezone`, `disclosure`,
`schema_version` (`1`).

- `forecast_events` has one entry per out-of-sample timestamp:
  `timestamp` (ISO-8601), `forecast`, `realized`,
  `exception = realized < forecast`, `refit` flag.
- `refits` carries `timestamp` plus the fit parameters; normal fits expose
  `mean`, `standard_deviation`, `n_observations`, `forecast`; historical
  fits expose `n_observations`, `quantile_method = "weibull"`, `forecast`.
- `diagnostics` is the walk-forward diagnostics without `fit_parameters`:
  ok → `method`, `quantile_method`, `window`, `refit_cadence`,
  `forecast_count`, `backtest_status`; insufficient → `reason`,
  `n_observations`, `window`; unsupported → `reason`.
- `backtest` payload: method, confidence, observations, exceptions,
  expected exceptions, inputs digest, exception rate, Kupiec and
  Christoffersen statistics/p-values, diagnostics (`significance`,
  `small_sample` bool), status, and the Basel traffic-light block
  `{zone, observations, confidence_level}`.
- `to_json()` is `json.dumps(to_dict(), indent=2, sort_keys=True,
  allow_nan=False) + "\n"`. Non-finite numerics, non-string mapping keys,
  non-JSON values, and non-scalar index names are rejected.
- `build_risk_validation_report` snapshots and revalidates the result:
  path shape, status-class emptiness, refit cadence, fit provenance,
  backtest recomputation and confidence, spec equality, digest shape. A
  result whose backtest does not match its path is rejected rather than
  serialized.
- `write_json` performs filesystem I/O; the native library returns the JSON
  string and leaves directory creation/retention to the caller.

### Calibration helpers

`expected_exception_count(n, c) = n * (1 - c)`.
`basel_traffic_light` compares `exceptions` against the 95% and 99.99%
binomial quantiles of `Binomial(n, 1-c)` (green below the first, yellow
below the second, red at/above it); `observations <= 0` is green.
`es_calibration_score` uses the linear realized quantile at `1-c`, the mean
of `realized <= threshold`, and `(realized_es - forecast_es)/|forecast_es|`,
returning NaN when `|forecast_es| < 1e-15`.

## Native decisions

- `include/fincore/risk/specs.hpp` owns `risk_forecast_target`,
  `risk_tail`, `risk_model_spec` (now including `horizon`) and
  `validate_risk_model_spec`; `walk_forward_var` calls the validator and
  reports `horizon != 1` as a structured `unsupported` result, matching the
  source reason.
- `walk_forward_var_result` gains `n_observations` (total clean input
  length) so the insufficient-data diagnostics can be projected without
  guessing. All other Phase 66 fields are unchanged.
- `include/fincore/risk/report.hpp` exposes `risk_forecast_event`,
  `risk_refit_event`, `risk_report_diagnostics`,
  `risk_validation_report`, `build_risk_validation_report`,
  `basel_reference_disclosure`, `risk_validation_report_schema_version`.
  The report owns typed events and a copy of the backtest result; JSON is
  produced by `to_json()` with sorted keys, two-space indentation and a
  trailing newline.
- JSON numbers reproduce Python `float.__repr__`: shortest round-trip
  digits, fixed notation for decimal exponent in `[-4, 15]`, scientific
  outside it, and `.0` appended to integral values. String escaping is
  `ensure_ascii`-style; non-ASCII code points become `\uXXXX`.
- `timestamp` rendering is naive UTC ISO-8601 (`...T00:00:00`) because the
  native index carries no timezone metadata. `timestamp_timezone` is always
  JSON `null`; a tz-aware Python index would emit an IANA/offset token, so
  that Python capability is declared unreproduced rather than approximated.
  `timestamp_index_name` is an optional builder argument (native DataFrame
  has no index-name field); the default is null, matching the unnamed index
  produced by `walk_forward_var`.
- `inputs_digest` is the Phase 66 native canonical SHA-256 recipe (spec
  fields plus IEEE-754 returns). It remains a versioned native digest, not
  equal to the pandas-CSV digest; the report carries the native digest
  verbatim and does not claim cross-language equality.
- Report validation mirrors `_validated_snapshot`: for ok results it
  recomputes `backtest_var` from the path and compares every auditable
  field (`rtol 1e-12, atol 1e-15` on floating fields); it checks refit
  timestamps against `index[::refit_cadence]`, fit count/date/observation
  count, forecast-segment reproduction, and the normal mean/std → forecast
  identity using the shared `detail::inverse_standard_normal_cdf`
  (tolerance `rtol 1e-12, atol 1e-15`). Empty statuses must have empty
  paths, no backtest, and a non-empty reason.
- Calibration helpers are public in `backtesting.hpp`. The binomial
  quantile is an exact discrete CDF inversion: log-PMF via `lgamma`, seeded
  at the floor/ceiling boundary and summed toward the requested tail, with
  binary search for the smallest `k` with `CDF(k) >= q`. Expected error is
  far below the integer threshold; tested thresholds are exact against
  SciPy for n ∈ {0,1,10,250,1000} and confidence ∈ {0.95,0.975,0.99}.
- `write_json` and `report/risk.py` projection are not ported here; the
  shared `ReportDocument` projection is Phase 88a/91, and the JSON string
  is the Phase 67 artifact.

## Oracle evidence

Root `/tmp/fincore-phase67-python.H7e4mx`, detached at `4b8f31b`, module
resolved under the detached source root, Python 3.11.8, NumPy 1.26.4,
pandas 3.0.3, SciPy 1.17.1, exit `0`, empty stderr, clean pre-status.
Script SHA-256 `5679fa2b95cf81c74b0ceb06db955183ada60ee223a2d7283fa13dc1a4df4f89`;
raw output SHA-256 `68d9ae1be876f8020127d45c68b46a640296bf295f675935d6814f84b41842da`.

Goldens (normal, 12-day fixture, window 5, cadence 2, confidence 0.9):

- status `ok`, 7 events, 4 refits, schema 1.
- Event 0 `2021-01-06T00:00:00`, forecast `0.000513085814624476`,
  realized `-0.0035078322768961985`, exception `true`, refit `true`;
  event 6 `2021-01-12T00:00:00`, forecast `-0.012453071489010743`,
  realized `0.009881682338770003`, exception `false`, refit `true`.
- Refit 0 parameters `{forecast: 0.000513085814624476,
  mean: 0.00565572986806186, n_observations: 5.0,
  standard_deviation: 0.004012826476671656}`.
- Diagnostics `{backtest_status: inconclusive, forecast_count: 7,
  method: normal, quantile_method: normal_ppf, refit_cadence: 2,
  window: 5}`.
- Backtest: 7 observations, 3 exceptions, expected `0.6999999999999998`,
  Kupiec `5.0976812174202815` (p `0.02395784976379568`),
  Christoffersen `3.81908500976888` (p `0.050671902346990025`),
  status `inconclusive`, traffic light `yellow`.
- Full `to_json()` text is 3811 bytes, SHA-256
  `9fc88fe32585993d4b92db7ed2d0a9b35290c113db6ea6b510fd9e75429273d2`.

300-day fixture (window 50, cadence 5, confidence 0.99): 250 events,
50 refits, first forecast `-0.015891935566622144`, last
`-0.016807926786139734`, diagnostics `forecast_count 250`, backtest
`inconclusive`/green with 0 exceptions and Kupiec `5.025167926750753`.
Historical fixture (window 100, cadence 10, confidence 0.95): 200 events,
20 refits, first `-0.009674773367508039`, last
`-0.009982395371701296`, backtest `fail`/yellow with 19 exceptions and
Kupiec `6.823658870683893`. Unsupported/insufficient reports have empty
events/refits and `backtest: null`.

Calibration: traffic-light boundaries n=250 c=0.99 → first yellow 5, first
red 10; n=1 c=0.99 → first yellow 0, first red 1; n=10 c=0.99 → 1/3;
n=250 c=0.975 → 11/17; n=1000 c=0.95 → 62/77. Expected exceptions
`2.500000000000002`, `50.00000000000004`, `0.0`. ES scores `-0.5`,
NaN for zero forecast, `4.333333333333334` for an empty tail.

Named index report: `timestamp_index_name = "trade_date"`,
`timestamp_timezone = null`. UTC-localized index: timezone `"UTC"` and
timestamps ending `+00:00` (Python-only capability, not native).

## Tests

- `risk_report_test.cpp`: spec validation matrix; small normal report
  fields/JSON exact text for stable scalar fields; large normal and
  historical summaries; empty report shapes; named-index projection;
  backtest-mismatch/cadence/fit tampering rejection; JSON float formatting
  and non-finite rejection.
- `risk_backtesting_test.cpp`: expected-exception values, traffic-light
  boundaries, ES calibration scores, threshold validation.
- `risk_walk_forward_test.cpp`: horizon field and structured unsupported
  status.
