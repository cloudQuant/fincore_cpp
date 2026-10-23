# Phase 84 Acceptance — Factor Report Projection (Four Source Sections)

## Scope

Rebuilds `build_factor_report` as a projection of the compute-once model into
the pinned four-section layout (`factor_summary`, `factor_information`,
`factor_returns`, `factor_turnover`) with source-shaped tables, series, units,
legends/notes and dataset metadata. No recalculation happens at render time;
the shared `report_document` schema and offline HTML renderer are unchanged.

## Sections

`factor_summary`

- Metrics: `forward_period_count`, `mean_information_coefficient.<period>`,
  `observation_count`, `finite_factor_count`, `coverage_ratio`,
  `quantile_count`, `forward_return_count.<period>`.
- Tables: `quantile_statistics` (min/max/mean/std/count/count %),
  `alpha_beta` (rows `Ann. alpha`, `beta`; one column per period),
  `mean_returns_by_quantile`, `information_coefficient` (date x period).
- Series: `factor_cumulative_returns.<period>` in `growth_multiple`.

`factor_information`

- Table `information_summary` with IC Mean, IC Std., Risk-Adjusted IC,
  t-stat(IC), p-value(IC), IC Skew and IC Kurtosis per period.
- Series `ic.<period>` in `correlation`.

`factor_returns`

- Tables `returns_summary` (alpha/beta plus top, bottom and spread in basis
  points), `alpha_beta`, `mean_returns_by_quantile`.

`factor_turnover`

- Tables `mean_turnover_by_quantile`, `rank_autocorrelation_summary` and
  `turnover_summary`, one column per lag (`1D`, ...).

Metadata carries `factor_result_fingerprint`, `forward_periods`,
`asset_count`, `date_count`, `start` and `end`; summary and returns sections
carry the two research notes.

## Values

Projecting the Phase 79 oracle fixture (three dates x four assets, spreads
0.02/0.1) yields a 100 bps top-quantile return, a 200 bps 1D spread, a 1.0
mean IC and 50% count shares for both quantiles. The report test asserts the
100 bps cell from `returns_summary` and the structural counts (4/1/3/3
tables, 1/1 cumulative and IC series, 2 notes per noted section).

## Declared divergences

A degenerate constant IC series (zero standard deviation) leaves t-stat and
p-value missing rather than emitting the source's infinite statistic; skew
and excess kurtosis use population (`bias=True`) moments and are missing for
constant input. Native JSON/HTML is deterministic but not byte-equal to the
pinned Python document, and the HTML renderer keeps its declared ECharts-to-
inline-SVG substitution. Group/event tables remain outside the model
(Phase 79 gaps), so they are also absent from the report.

## Tests and gates

`FactorReport.ProjectsModelToSharedDocument` updated for four sections,
metadata keys, table/series counts, the 100 bps summary cell, deterministic
JSON and zh/en HTML. Full headless CTest: **710/710 passed**. Sanitizer not
re-run for this slice; visual rendering remains **NOT_RUN**.
