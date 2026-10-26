# Phase 55 Native XIRR Design

## Source contract

`fincore/performance/returns.py`:

- `_normalise_xirr_inputs`: finite one-dimensional amounts; labelled inputs
  must share unique index labels; dates are normalized to local calendar days
  (mixed timezones rejected); same-day amounts are summed; fewer than two
  distinct dates raise.
- `_irr`: aggregate equal periods, drop zero coefficients, require exactly
  one sign transition after aggregation; otherwise NaN.
- `_scaled_npv`: `log|CF| - period*log(1+r)`, anchored by its maximum,
  sign-preserving summation to avoid rate-space overflow.
- `_solve_conventional_irr`: endpoints over
  `[log1p(nextafter(-1,0)), log(DBL_MAX)]` must be finite and opposite-signed
  (a sign-changing bracket is the existence certificate); `brentq` with
  `xtol=eps`, `rtol=4*eps`, `maxiter=256`; `expm1` conversion; non-finite
  rates clamp to `DBL_MAX`; rates `<= -1` become NaN.
- `xirr`: day differences `/365`.

## Native decisions

- The native time representation is UTC nanoseconds; local calendar days are
  therefore UTC days, stated explicitly rather than silently substituted.
  Mixed-timezone inputs cannot exist at this boundary.
- Input classes: non-finite amounts, fewer than two distinct dates, and
  missing/empty selectors throw `input_error` (Python raises `ValueError`).
  Non-conventional streams and all-one-sign streams return NaN.
- The root solver is deterministic bisection on the same scaled objective to
  a `1e-15` relative log-rate interval (200-iteration cap). The oracle's
  `brentq` and this bisection agree on every golden at 1e-12; the declared
  numerical budget is the interval, and bracket opposition remains the only
  existence certificate.
- Same-day aggregation uses day-bucketed ordered sums; the aggregation order
  is ascending day, matching the Python stable sort on periods.

## Oracle evidence

Root `/tmp/fincore-phase55-python.GGx9A6`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes. Script SHA-256
`1896193f1ee6cd4690edbbbbe0f2a49a53a5bbcbe8328f22c3ecf15ea2ba0077`; raw
output SHA-256
`4f48c351529e4fe5a19652e38b562e218c85f77719ea75ea4fae9c9155412d8a`.

Goldens: two flows `0.09971358593414104`; three flows
`0.11684905803770408`; same-day aggregation `0.06647859284144313`; zero
amount dropped and unsorted dates both equal to the two-flow result;
non-conventional and all-negative NaN; loss `-0.4990521803938817`; single
date and non-finite amount raise.

## Tests

Three performance tests: conventional goldens (aggregation, dropping,
sorting), rejection/error classes, selector/NUL/lifetime.
