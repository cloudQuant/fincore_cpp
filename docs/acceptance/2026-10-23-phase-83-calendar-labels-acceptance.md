# Phase 83 Acceptance — Calendar-Derived Research Labels

## Scope

Research labels on price-derived paths now follow the price-grid spacing
instead of always using `<period>D`:

- `canonical_period_label` measures the first valid whole-day grid gap for a
  period (`1D` on daily data, `7D` on weekly data, `5D` across five calendar
  days, etc.).
- The close-to-close route relabels prepared horizons to those canonical
  labels; duplicate canonical labels fail with `factor_data_error`.
- The next-open route uses canonical labels directly.
- The precomputed route still prefers `<N>D`; if that label is absent and the
  table has exactly one label per requested period, it accepts the supplied
  labels positionally (allowing canonical `7D` tables).

Sub-day remainders fall back to `<period>D`; intraday label formatting is not
implemented.

## Test evidence

`FactorResearch.DerivesCalendarPeriodLabelsForWeeklyGrid` evaluates a
10-week price grid with period 1:

- next-open and close labels are both `7D`;
- coverage is 15 rows on both routes;
- all five finite IC values are `~1`.

Existing daily fixtures keep `1D`/`5D` labels, so prior oracle values and
acceptance records are unchanged.

## Remaining research gaps

Weighted factor combinations (`spec.combinations`) and intraday label
formatting are not implemented; factor-preparation labels outside the
research evaluator remain `<N>D`.

## Gates

Full Release and ASan+UBSan suites: **734/734 passed**. Visual NOT_RUN.
