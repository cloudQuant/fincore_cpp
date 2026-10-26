# Phase 67 Native Risk Spec and Validation Report Plan

**Goal:** Add the versioned `RiskModelSpec` validation contract and the
compute-once `RiskValidationReport` projection with deterministic JSON, plus
the calibration helpers its backtest payload requires.

**Baseline:** `dev` after Phase 66 (oracle pin `4b8f31b`). Python source:
`fincore/risk/specs.py`, `fincore/risk/report.py`,
`fincore/risk/calibration.py`, `fincore/risk/diagnostics.py`.

**Files:** `include/fincore/risk/specs.hpp` (new), `src/risk/specs.cpp` (new),
`include/fincore/risk/report.hpp` (new), `src/risk/report.cpp` (new),
`include/fincore/risk/walk_forward.hpp` / `src/risk/walk_forward.cpp`
(horizon, observation count, status names), `include/fincore/risk/backtesting.hpp`
/ `src/risk/backtesting.cpp` (calibration helpers), `CMakeLists.txt`,
`tests/unit/risk_report_test.cpp` (new), `tests/unit/risk_backtesting_test.cpp`,
`tests/unit/risk_walk_forward_test.cpp`, `tests/CMakeLists.txt`, design record.

## Native API

```cpp
struct risk_model_spec { target; confidence; horizon; distribution; tail;
                         sign; window; cadence; version; };
void validate_risk_model_spec(const risk_model_spec&);

struct risk_forecast_event { date; forecast; realized; exception; refit; };
struct risk_refit_event { date; forecast; mean; stddev; n_observations; };
struct risk_report_diagnostics { method; quantile_method; window; cadence;
    forecast_count; backtest_status; reason; n_observations; };
struct risk_validation_report { status; inputs_digest; specification;
    forecast_events; refits; diagnostics; has_backtest; backtest;
    traffic_light_zone; timestamp_index_name; timestamp_timezone;
    disclosure; schema_version; std::string to_json() const; };
risk_validation_report build_risk_validation_report(result, index_name);

double expected_exception_count(observations, confidence);
std::string basel_traffic_light(exceptions, observations, confidence);
double es_calibration_score(forecast_es, realized, confidence);
```

## Contract

- Spec validation: target/tail enums, confidence finite in `(0,1)`, horizon
  ≥ 1, distribution in {historical, normal}, sign `losses_negative`, window
  ≥ 2, cadence ≥ 1, version `"1.0"`.
- `walk_forward_var` validates the spec, reports `horizon != 1` as
  structured unsupported, and records the total observation count.
- The report mirrors `RiskValidationReport`: status, native digest, spec echo,
  per-timestamp forecast events, refit parameters, diagnostics without
  `fit_parameters`, backtest payload, optional index name/timezone, exact
  baseline disclosure, schema version 1.
- `build_risk_validation_report` revalidates the result: path/count shape,
  refit cadence and fit provenance (including normal mean/std → forecast),
  finite values, digest shape, backtest confidence/observations and a full
  recomputation against the path.
- `to_json()` reproduces Python's deterministic JSON: sorted keys, two-space
  indentation, trailing newline, `float.__repr__` numbers, `ensure_ascii`
  escaping, and rejection of non-finite numbers or invalid UTF-8.
- `timestamp_timezone` is null because the native index has no timezone
  metadata; `timestamp_index_name` is an explicit optional builder argument.
- Calibration helpers reproduce `calibration.py`; the traffic light inverts
  the exact binomial CDF (SciPy-equivalent thresholds on tested sizes).

## Tests

Spec validation matrix; normal/historical/small report oracle fields; exact
unsupported JSON text; insufficient report shape; named-index projection;
tampered backtest/fit/cadence rejection; float/timestamp/escaping formatting;
non-finite and invalid-text rejection; calibration thresholds and ES scores;
walk-forward horizon status.

## Gates

Clean detached oracle at `4b8f31b`; byte-level JSON structure comparison
against the Python artifact; full CTest; LLVM 22 ASan+UBSan; acceptance
record and README ledger.
