# Phase 48 Native Hurst Exponent Design

## Source contract

`fincore/metrics/stats.py::hurst_exponent` (lines 126–222):

- raw length below 8 → NaN; NaN-only clean length below 8 → NaN;
- cumulative-deviation range and sample deviation; either zero → NaN;
- `max_lag = max(n // 3, 3)`; lags 2..max_lag unless the span exceeds 30,
  in which case `np.geomspace(2, max_lag, 30).astype(int)` is uniqued;
- per lag, each block's range divided by its sample deviation, averaged over
  blocks with both positive;
- fewer than two lag entries → `0.5 + ln(range/std)/ln(2n)` fallback;
- otherwise the OLS slope of `ln(lag)` vs `ln(ratio)`; result clamped to
  [0, 1].

## Native decisions

- `hurst_lags` reproduces NumPy `geomspace` using `log10` spacing and forces
  the final lag to `max_lag` (NumPy's endpoint rule). The oracle's control
  line lists the 22 resulting lags for `n=120`.
- The regression is the closed-form OLS slope (covariance/variance) rather
  than `np.polyfit`'s SVD path; they agree to reduction tolerance.
- Floating-point note: a repeated `0.01` series retains a tiny non-zero
  deviation through mean rounding and therefore returns clamped `1.0`, while
  an exactly representable constant (`0.5`) returns NaN. Tests lock both.

## Oracle evidence

Root `/tmp/fincore-phase48-python.MTUMFS`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`d907d8d204697c7e075663d51924d431ea339675cbc7ce201c585d125f1de9f8`; raw
output SHA-256
`cfa09dbf6afcf71dedaf5359bf1bee81d10c4ade058421934dcd2219fc13e3e1`.

Goldens: ordinary `1.0`, 120-point sin `0.6317378848594388`, 120-point cos
`0.385513237072807`, spike series `0.7280957897994347`, eight values `1.0`,
NaN-mixed `1.0`; raw seven, exact constant 0.5, and flat zero NaN.

## Tests

Four tests in `stats_test.cpp` plus the umbrella consumer. Long-series
goldens use a 1e-6 tolerance because `std::sin`/`std::cos` and NumPy's
transcendentals may differ by ulps and the R/S ratio amplifies them; all
other fixtures use 1e-12 or exact classes.
