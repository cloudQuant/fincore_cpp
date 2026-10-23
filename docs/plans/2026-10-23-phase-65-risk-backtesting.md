# Phase 65 Native Risk Backtesting Plan

**Goal:** Add the VaR/ES backtest surface: Kupiec POF, Christoffersen
independence, `backtest_var`, `backtest_es`, and status classes.

**Baseline:** `dev` after Phase 64 (oracle pin `4b8f31b`). Python source:
`fincore/risk/backtesting.py` (whole module).

**Files:** `include/fincore/risk/backtesting.hpp` (new),
`src/risk/backtesting.cpp` (new), `CMakeLists.txt`,
`tests/unit/risk_backtesting_test.cpp` (new), `tests/CMakeLists.txt`.

## Native API

```cpp
enum class risk_backtest_status { pass, fail, inconclusive, experimental };
enum class risk_backtest_method { var, es };
struct risk_backtest_result { method; confidence_level; observations;
    exceptions; expected_exceptions; exception_rate; kupiec_lr;
    kupiec_pvalue; christoffersen_lr; christoffersen_pvalue;
    inputs_digest; diagnostics; status; };

double kupiec_lr(observations, exceptions, confidence_level);
double christoffersen_lr(span<const int> exceptions);
risk_backtest_result backtest_var(forecast, realized, confidence = 0.99,
                                  significance = 0.05, columns);
risk_backtest_result backtest_es(forecast, realized, confidence = 0.975,
                                 n_bootstrap = 1000, seed = 20260817, columns);
```

## Contract

- Forecast/realized align on common timestamps and drop NaN pairs; no
  overlap raises.
- VaR exceptions are strict `realized < forecast`.
- Kupiec uses the `xlogy` 0-log-0 limit and the `max(0, 2*(...))` clamp;
  Christoffersen uses the multinomial form with `0*log(0)=0`.
- Chi-square p-values use the df=1 closed form `erfc(sqrt(lr/2))`.
- Status: fewer than 3 observations or expected exceptions below 5 →
  `inconclusive`; otherwise any p-value below significance → `fail`; else
  `pass`. ES always reports `experimental`.
- ES: realized tail = realized below the linear `1-confidence` quantile;
  realized ES is the tail mean; forecast ES the aligned forecast mean; a
  bootstrap (native `mt19937_64`, seed default 20260817) estimates the tail
  standard deviation; z-score uses it.
- `inputs_digest` is a SHA-256 over a canonical binary serialization
  (dates + double bits). It is deterministic natively but **not
  byte-identical to the Python CSV digest**; this divergence is documented
  and covered by determinism/sensitivity tests rather than equality.

## Tests

Oracle-exact Kupiec/Christoffersen values, VaR backtest fields and
inconclusive/fail/short statuses, bad confidence rejection, ES deterministic
fields (exceptions 7, realized ES `-0.009935211388974519`, forecast
`-0.012`, experimental status), digest determinism, and native z-score
determinism.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
