# Iteration 5 slice 1 Acceptance — Factor Data and Performance

## Scope

Adds the native factor panel/preparation core (`factor/data.hpp`,
`src/factor/data.cpp`, `factor/exceptions.hpp`) and the first
performance/inference kernel set (`factor/performance.hpp`,
`src/factor/performance.cpp`), with two oracle-test TUs. Five new paths plus
CMake/test registrations; no dependency change.

## Oracle parity

Preparation oracle `/tmp/fincore-phase77-python.NbR0og` (script
`c75019e…`, raw `459bb09…`), performance oracle (script `4cea619…`, raw
`c63ebc5…`), both at `4b8f31b`, exit `0`, empty stderr.

- `compute_forward_returns` reproduces the 1D/2D values on the 15-row
  fixture (first `0.01`/`0.02`, last `0.009615…`/`0.019230…`).
- `prepare_factor_data` reproduces 14 rows, per-date quantiles (2/1/3, last
  date 3/1), groups x/y, forward values and the loss report
  (15/14/14/14, total `0.066666…`).
- `prepare_factor_data_by_horizon` quantizes before availability and returns
  the same 1D/2D tables; `filter_zscore` is rejected.
- `prepare_factor_data_from_forward_returns` matches the price-derived path.
- Quantization matches pandas: duplicate qcut edges raise; qcut3
  `[1,1,1,3,3]`; cut3 `[1,1,1,2,3]`; zero-aware `[1,1,2,3,3,4]`; all-equal
  `no_raise` yields unassigned labels.
- Spearman IC, mean IC, factor weights/returns, alpha/beta, quantile
  means/SE, turnover, rank autocorrelation, IC mean/t/CI and BH inference
  match the 60-day performance oracle at 1e-10/1e-12 (e.g. IC t
  `-1.9944671`, p `0.0507280`, alpha/beta `-0.4490675/0.1626165`).

## Delivered vs open

Delivered: Phases 77/78 panel, forward returns, quantization, three prepare
routes (non-PIT), and Phases 80/81 IC/weights/returns/alpha-beta/quantile
means/turnover/rank-autocorrelation/IC-inference/BH.

Open (next factor slices): `prepare_pit_factor_data`, group-IC tables,
`by_date`/`by_group` quantile tables, `factor_model_inference` (needs the
analysis model), Fama-MacBeth, `factor_alpha_beta(returns=...)` overrides,
analysis assembly, portfolio/costs, research, tears and reports.

## Declared divergences

- Forward-period labels are `ND`; the source may derive business-gap labels
  from the first valid calendar interval.
- Unbalanced-universe turnover keeps the full date index with NaN current
  sets; the source can shorten its index when a quantile is absent.
- Group IC is aggregated per date rather than represented as a
  (date, group) table; `mean_return_by_quantile` rejects `by_date`/
  `by_group` explicitly instead of returning a differently shaped table.
- `no_raise` drops unassigned rows (matching the source's final dropna).
- PIT/Fama-MacBeth/model inference remain explicit gaps.

## Test coverage

`factor_data_test.cpp` (7 tests) and `factor_performance_test.cpp` (6 tests)
covering preparation, loss, by-horizon, quantization edge cases, IC,
weights/returns, alpha/beta, quantile tables, turnover, rank autocorrelation,
IC inference and BH.

## Gates (current tree)

- Full headless CTest: **681/681 passed**.
- Sanitizer: not re-run in this slice (no claim).
- ImPlot compile/link unchanged.

## Limits

This is a compute slice. PIT/analysis/cost/research/report routes are not
claimed complete; group-shaped outputs and unbalanced turnover semantics
remain documented gaps.
