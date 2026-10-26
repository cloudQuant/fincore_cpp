# Phase 79 Acceptance — Compute-Once Factor Analysis Model

## Scope

Extends `analyze_factor` from the initial kernel summary into a renderer-ready,
compute-once model covering quantile statistics, by-date quantile returns,
top-minus-bottom spreads, per-period cumulative curves and positions, lagged
turnover/rank autocorrelation, monthly IC aggregation and the first-period
portfolio inputs. Adds two performance kernels (`mean_return_by_quantile_by_date`,
`mean_return_spread`) so assembly does not duplicate numerical logic. No new
dependency.

## Config and model

`factor_analysis_config` now carries `periods`, `turnover_periods`,
`time_aggregation`, `include_portfolio_inputs` and
`portfolio_benchmark_period` in addition to the existing factor-construction
flags. Selected periods are validated against the prepared labels; unselected
forward columns are removed from the internal analysis snapshot while the
model retains the full prepared data.

`factor_analysis_model` adds typed, owning fields:

- `quantile_statistics` (min/max/mean/sample std/count/count %),
- `mean_returns_by_date` (dates x quantiles x labels, means and within-date
  standard errors),
- `mean_return_spread` (upper-minus-lower per selected label and the
  quadrature-combined standard error),
- `factor_cumulative_returns`, `factor_positions` and
  `quantile_cumulative_returns` per selected period,
- `quantile_turnover` (`quantile`, `lag`, series) and
  `rank_autocorrelation` (`lag`, series) for every requested lag,
- `time_aggregated_information` (monthly IC),
- optional `portfolio_inputs` for the first selected period.

The IC kernel now receives `group_neutral` as its group adjustment, matching
the source assembly. The model fingerprint covers the new config fields and
the selected period order.

## Oracle fixture

Pinned Python `4b8f31b1`, three dates x four assets, factors 1-4 with
quantiles 1,1,2,2, periods 1D/5D, lags 1/2, monthly aggregation,
`include_portfolio_inputs=True`, benchmark `1D`
(script `/tmp/fincore-phase79-python.XTj2ru/oracle.py`, JSON SHA-256
`a4425245a0ef595730b403d9397ec019d2fbafd0d50b5c603c9674d50140f270`).

| quantity | oracle value |
| --- | --- |
| quantile 1 statistics | min 1, max 2, mean 1.5, sample std 0.5477225575051661, count 6, 50% |
| IC / mean IC | 1.0 on all three dates and both labels |
| mean by quantile | `-0.01, -0.05` (Q1) and `0.01, 0.05` (Q2) |
| by-date means | dates 3, quantiles 2, labels 2; Q1 `-0.01/-0.05`, Q2 `0.01/0.05`; within-date SE 0.005 |
| spread | 0.02 (1D), 0.1 (5D) |
| factor cumulative | 1.0379707031249998 (1D), 1.199462890625 (5D) |
| quantile cumulative | 1D Q2/Q1 `1.01/0.99` → `1.030301/0.970299`; 5D first `1.0098057976734853/0.9897937816869885` |
| positions | 16 rows (1D), 24 rows (5D) |
| turnover / rank | 2 quantiles x 2 lags; lag-1 rank `NaN, 1, 1`; lag-2 `NaN, NaN, 1` |
| monthly IC | 2022-01-31, 1.0 per label |
| portfolio first period | daily returns `0, 0.0125, 0.0125`; 20 asset+cash rows; benchmark `0, 0.026, 0.027` |

## Declared gaps

No group-level analysis (`by_group`), event-window model, aggregate-versus-
group snapshot duplication, capital-scaled portfolio inputs, or weekly/
quarterly/yearly time aggregation — only monthly `"M"`/`"ME"` is implemented
natively. By-date standard errors share the `mean_returns_by_date` field
instead of a separate model member, and the native `result_fingerprint` is a
deterministic input/config digest rather than the Python result fingerprint.
Event inputs and source-compatible tear-sheet turnover quirks are not part of
this enhanced API.

## Tests and gates

New `FactorAnalysis.MatchesCleanPythonModelSnapshot` asserts the fixture
values above plus fail-closed validation (unknown period, zero lag,
unsupported weekly aggregation) and the `include_portfolio_inputs=false`
path. Existing analysis, tears and report tests continue to consume the new
typed turnover/rank series. Full headless CTest: **710/710 passed**.
Sanitizer not re-run for this slice; visual NOT_RUN.
