# Phase 66 Native Walk-Forward VaR Plan

**Goal:** Add `risk_model_spec` and `walk_forward_var`: an auditable one-step
lower-tail VaR path with rolling-window refits and an out-of-sample backtest.

**Baseline:** `dev` after Phase 65 (oracle pin `4b8f31b`). Python source:
`fincore/risk/specs.py` 1–100, `fincore/risk/diagnostics.py` 46–450.

**Files:** `include/fincore/detail/digest.hpp` (new),
`src/detail/digest.cpp` (extracted SHA-256), `include/fincore/risk/walk_forward.hpp`
(new), `src/risk/walk_forward.cpp` (new), `CMakeLists.txt`,
`tests/unit/risk_walk_forward_test.cpp` (new), `tests/CMakeLists.txt`,
`src/risk/backtesting.cpp` (uses the extracted digest).

## Native API

```cpp
enum class risk_distribution { historical, normal };   // spec field is text
enum class risk_forecast_target { var, es, pair };
enum class risk_tail { lower, upper };
enum class walk_forward_status { ok, insufficient_data, unsupported };
struct risk_model_spec { target; confidence; distribution; tail; sign;
                         window; refit_cadence; model_version; };
struct refit_parameters { date; forecast; mean; stddev; n_observations; };
struct walk_forward_var_result { spec; index; forecast; realized;
    refit_timestamps; fits; inputs_digest; status; unsupported_reason;
    backtest; has_backtest; };
walk_forward_var_result walk_forward_var(returns, spec, column);
```

## Contract

- Returns must have a strictly increasing, duplicate-free index and finite
  values.
- Spec validation: confidence finite in (0,1), window ≥ 2, cadence ≥ 1,
  model version `"1.0"`, distribution `historical`/`normal`.
- Unsupported contracts (non-var target, non-lower tail, non-losses-negative
  sign, historical rank outside `[1, window]` with 8-epsilon tolerance)
  return the structured `unsupported` status and reason instead of a
  substitute forecast.
- `n <= window` returns `insufficient_data`.
- Refits at the first forecast and every `refit_cadence` positions; each fit
  sees exactly the preceding `window` observations.
- Historical forecasts use the Hyndman-Fan type-6 (Weibull) quantile; normal
  forecasts use mean + sample std × `norm.ppf(alpha)`, falling back to the
  mean on zero deviation.
- The backtest is computed over the returned out-of-sample path.
- `inputs_digest` hashes spec fields and return values in a canonical binary
  form (native provenance; not pandas-CSV-equal — same declared divergence as
  Phase 65).

## Tests

Normal and historical oracle goldens (path count/first/last values, refit
counts/timestamps, backtest status/exceptions/Kupiec), unsupported target,
insufficient data, rank-range unsupported, window/confidence validation, and
digest shape.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
