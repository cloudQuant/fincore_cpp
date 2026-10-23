# Iteration 5 slice 1 Design — Factor Data and Performance

## Source contract

`factor_analysis/data.py`:

- `factor` is a `(date, asset)` Series with unique keys; prices are a wide
  date-indexed frame sorted by date; every factor asset must be a price
  column.
- `compute_forward_returns(factor, prices, periods, filter_zscore,
  cumulative_returns)`: `pct_change(period).shift(-period)` (cumulative) or
  `pct_change(1).shift(-period)` (non-cumulative), reindexed to the factor
  rows; the period labels come from the first valid interval (`1D`, `2D`,
  … for daily data).
- `quantize_factor`: period-wise `pd.qcut` (or `pd.cut` for integer/custom
  bins), optional `by_group`, `zero_aware` positive/negative splits, and
  `no_raise` returns unassigned labels on duplicate edges.
- `prepare_factor_data`: join forward returns to finite factors, drop rows
  with any missing forward/group, quantize the remaining rows, drop rows
  without a quantile, report input/finite/forward/binning counts and raise
  when total loss exceeds `max_loss`.
- `prepare_factor_data_by_horizon`: quantize finite factors **before**
  forward availability and build one table/loss report per horizon;
  `filter_zscore` is rejected.
- `prepare_factor_data_from_forward_returns`: the same cleaner without the
  price computation.
- `prepare_pit_factor_data` materializes a causal PIT ledger and is not yet
  ported.

`factor_analysis/performance.py` / `inference.py`:

- Spearman IC per date (and group), forward demeaning by date/group,
  factor weights (`demeaned`, `equal_weight`, `group_adjust`), weighted
  factor returns (all-missing periods sum to 0), annualized alpha/beta via
  OLS against the equal-weight universe, quantile mean/SE tables, quantile
  turnover, rank autocorrelation, IC mean/t/CI, Benjamini-Hochberg and
  IC inference (`n`, mean, t, p, adjusted p, rejected, testable).

## Native model

- `factor_series` owns parallel date/asset/value arrays; `forward_return_table`
  owns labels plus label×row values; `prepared_factor_data` owns a row list
  with factor, quantile, group and per-label forward returns plus a
  `factor_loss_report`.
- `factor_data_error`/`factor_loss_exceeded` are the native failure classes;
  `quantization_config` selects quantiles/cut/zero-aware/by-group/no-raise.
- Performance results are typed tables/series (`factor_ic_table`,
  `factor_ic_summary`, `factor_weight_row`, `factor_return_table`,
  `factor_alpha_beta_row`, `quantile_return_table`,
  `factor_series_result`, `fdr_result`, `ic_inference_result`).

## Numerics and divergences

- Forward returns use positional price rows (the native index is UTC
  nanosecond timestamps); the Python business-day calendar gap does not
  change the return formula but changes the label only when the first
  interval spans a non-trading gap. Native labels are `ND` (declared).
- qcut edges are linear quantiles with average ranks for Spearman ties;
  duplicate edges are an error unless `no_raise`, in which case rows are
  unassigned (and dropped) rather than NaN-labelled rows retained.
- Integer `cut` applies pandas' 0.1% edge expansion; custom bins must
  increase strictly.
- `quantile_turnover` uses the full prepared date index; the source can
  shorten its index when a quantile is absent on some dates (unbalanced
  universe). Native keeps the full date index and marks missing current
  sets NaN; this is a declared fidelity difference for the unbalanced case.
- Group IC tables and `by_date`/`by_group` quantile tables are not exposed
  in this slice; `mean_return_by_quantile` rejects those flags explicitly.
- PIT preparation, Fama-MacBeth and `factor_model_inference` remain open
  Phase 78/80 routes.

## Oracle evidence

Root `/tmp/fincore-phase77-python.NbR0og`, detached at `4b8f31b`, Python
3.11.8 / NumPy 1.26.4 / pandas 3.0.3, exit `0` for both scripts, empty
stderr, clean pre-status.

- Preparation oracle: script SHA-256
  `c75019e7d1ef7197bbf9c494df07bb3131b3d85d9824cb8ff84054a484388289`; raw
  SHA-256 `459bb09160dac45e889e0c9a8e94b1f17d05e06eddf598a84189fef272e71479`.
  15-row/3-asset fixture, periods (1,2), quantiles 3, groups x/y:
  prepared 14 rows, quantiles 2/1/3 per date (3/1 on the last date), loss
  input 15/finite 14/forward 14/binning 14, total loss `0.066666…`.
  Quantize goldens: qcut4 duplicate error, qcut3 `[1,1,1,3,3]`, cut3
  `[1,1,1,2,3]`, zero-aware `[1,1,2,3,3,4]`, all-equal no_raise empty.
- Performance oracle: script SHA-256
  `4cea6193ba22ad35c3f71db08791bb9aa087616164b4477626c6746f938614ab`; raw
  SHA-256 `c63ebc504f84f761cf4059821597500f3b58fdf0efc74fa4cbff258642e693bc`.
  60-day/5-asset panel: first ICs `0.9999999999999999`/`0.9`, mean IC
  `-0.1783333`/`-0.1033333`; factor returns first `0.00339208559`/
  `0.0163436132` and last `-0.00119122059`/`-0.00851199079`; alpha/beta
  `(-0.4490675, 0.1626165)` and `(-0.0973217, -0.1221705)`; quantile means
  `[0.002015254, -0.000463072, -0.001783718]`; IC t `-1.9944671`, p
  `0.0507280`, BH `[0.04, 0.0533333, 0.0533333, 0.2]`.

All native comparison tests pass at 1e-10/1e-12.

## Tests

`factor_data_test.cpp` (7 tests) and `factor_performance_test.cpp` (6
tests): forward returns, prepared rows/loss, by-horizon quantization,
from-forward parity, qcut/cut/zero-aware/all-equal, invalid inputs, IC,
weights/returns, alpha/beta, quantile means/SE, turnover, rank
autocorrelation, IC inference and BH.
