# Phase 52b Slice 2 Design — Turtle R³ and Rolling Leaves

## Source contracts

- `stats.py::r_cubed_turtle` (578–657): NAV compounding, NaN positions
  retained, SciPy `linregress` slope over non-NaN NAV positions,
  `rar = slope * ann`, per-UTC-year absolute max drawdown, mean, `1e-15`
  gate with +infinity for positive RAR.
- `rolling.py::roll_annual_volatility` / `roll_sortino_ratio` (80–98):
  endpoint arrays (`n - window + 1`) applying the scalar leaves to each
  complete window.
- `roll_alpha_aligned`, `roll_beta_aligned`, `roll_alpha_beta_aligned`
  (62–78): positional wrappers around `roll_alpha`/`roll_beta`/
  `roll_alpha_beta`; native equivalents are the existing label-aligned
  `roll_alpha`/`roll_beta`/`roll_alpha_beta` (accepted in the foundation),
  recorded as mappings.

## Native decisions

- Turtle year grouping uses `utc_year` from the calendar leaf; the index is
  always UTC, so the Series-with-datetime-index branch of the source is the
  only reachable path. The ndarray 252-row chunk branch is not reproduced
  because the native boundary is always a timestamped frame (mapping note).
- Turtle NAV keeps NaN rows as NaN so the linregress mask matches Python's
  position-preserving mask. The slope uses the closed-form OLS
  covariance/variance, equivalent to `linregress`'s slope.
- Rolling leaves use `endpoint_output` and delegate to the accepted scalar
  leaves; the oracle shows NaN windows produce values (NaN-dropping), so the
  native code deliberately does **not** reuse the `contains_nan ? nan` gate
  used by `rolling_volatility`/`rolling_sharpe`.

## Oracle evidence

Root `/tmp/fincore-phase52b2-python.eDN5xv`, detached at `4b8f31b`, exit `0`,
zero stderr. Script SHA-256
`9455b669dd3eecdeb5676cf2e318726b482759f7dcb252e8c1462f1dd2773dec`; raw
output SHA-256
`b90a64e2cd38a88c96a4702937e35de26753d6312632d6e672e9c42a228ad6fe`.

Goldens listed in the plan. The long turtle fixture starts 2019-11-01 to
match the oracle's year split (2019/2020); an initial mismatch (3.5e-4) was
traced to a different start date, not to the formula.

## Tests

`stats_test.cpp`: turtle goldens (short and long), NaN/short gates,
selectors/NUL. `rolling_metrics_test.cpp`: endpoint shapes, NaN/inf-aware
value checks for volatility/Sortino, annualization override, empty result for
window > n, zero-window error.
