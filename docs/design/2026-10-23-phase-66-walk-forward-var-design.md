# Phase 66 Design — Walk-Forward VaR

## Source contract

`diagnostics.py`:

- `_validate_returns`: timestamp index, unique, ascending, numeric finite.
- `_unsupported_reason`: target must be `var`; tail `lower`; sign
  `losses_negative`; horizon 1; historical rank
  `(window+1)*(1-confidence)` must lie within `[1, window]` (8-epsilon
  tolerance).
- `n <= window` → structured `insufficient_data`.
- Refit at the first forecast and every `refit_cadence`; estimation window is
  exactly the `window` observations strictly before the forecast timestamp.
- Historical fit: Hyndman-Fan type 6 (`weibull`) quantile
  `h=(n+1)p; j=floor(h); x[j-1]+(h-j)(x[j]-x[j-1])` with endpoint clamps.
- Normal fit: mean, sample std; `mean + std*norm.ppf(alpha)`, or mean when the
  standard deviation is zero.
- Backtest over the out-of-sample pairs via `backtest_var`.

## Native decisions

- Spec mirrors the Python dataclass fields (target, confidence, distribution
  string, tail, sign convention, window, cadence, version) with validation
  errors mapped to `input_error`.
- Unsupported contracts are returned as data (`unsupported` status + reason),
  matching the source's structured refusal.
- `refit_parameters` records date/forecast/mean/std/n as an owning vector,
  replacing the Python diagnostics dictionary.
- SHA-256 was extracted to `fincore::detail` so backtesting and walk-forward
  share one implementation; walk-forward hashes spec fields plus return
  values (native canonical form, declared non-CSV-compatible).
- A column-name bug was caught by the tests: the internal backtest call must
  pass `"var"`/`"realized"` explicitly, since the generic backtest defaults
  to `forecast`/`realized`.

## Oracle evidence

Root `/tmp/fincore-phase66-python.fm7dfz`, detached at `4b8f31b`, exit `0`,
zero stderr. Script SHA-256
`9b5f8b3277afc8866909e5c8c9a297cb883c01ed0525077157369eecbd270407`; raw
output SHA-256
`5b88f43e4fb07591ce74660ed0711b1d8257d0bd334f559bdfbc7bfe06b395ec`.

300-day fixture. Normal (window 50, cadence 5, confidence 0.99): 250
forecasts from 2021-02-20 to 2021-10-27, first `-0.015891935566622144`, last
`-0.016807926786139734`, 50 refits (first 2021-02-20, last 2021-10-23),
inconclusive backtest with 0 exceptions and Kupiec `5.025167926750753`.
Historical (window 100, cadence 10, confidence 0.95): 200 forecasts from
2021-04-11, first `-0.009674773367508039`, last `-0.009982395371701296`, 20
refits, failing backtest with 19 exceptions and Kupiec
`6.823658870683893`.

## Tests

Three tests: normal and historical goldens, structured
unsupported/insufficient/rank cases, validation errors, digest shape.
