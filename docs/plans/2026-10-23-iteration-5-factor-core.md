# Iteration 5 slice 1 Plan — Factor Data and Performance Kernels

**Goal:** Deliver the panel contract (Phase 77), factor preparation (Phase
78) and the first performance/inference slice (Phases 80/81) for the pinned
factor analysis package.

**Baseline:** `dev` after Iteration 4 (oracle pin `4b8f31b`). Python source:
`factor_analysis/{data,calendar,performance,inference}.py`.

**Files:** `include/fincore/factor/{data,exceptions,performance}.hpp`,
`src/factor/{data,performance}.cpp`,
`tests/unit/{factor_data,factor_performance}_test.cpp`, CMake lists, records.

## Native API

```cpp
struct factor_series { dates; assets; values; };       // unique (date, asset)
struct forward_return_table { labels; dates; assets; values; };
struct prepared_factor_data { rows(date,asset,factor,quantile,group,forwards);
                              forward_labels; loss_report; };
compute_forward_returns(factor, prices, periods, filter_zscore,
                        cumulative_returns);
quantize_factor(factor, quantization_config, grouping);
prepare_factor_data / prepare_factor_data_by_horizon /
prepare_factor_data_from_forward_returns;
factor_information_coefficient / mean_information_coefficient;
factor_weights / factor_returns / factor_alpha_beta;
mean_return_by_quantile / quantile_turnover /
factor_rank_autocorrelation;
ic_mean / ic_t_stat / ic_confidence_interval;
benjamini_hochberg / information_coefficient_inference;
```

## Contract

- `(date, asset)` keys are unique; prices are sorted and every factor asset
  must exist; periods are positive; factor dates must intersect prices.
- Forward returns are `P[t+p]/P[t]-1` (cumulative) or the one-period return
  at `t+p`, aligned to the factor rows.
- Quantization reproduces pandas qcut (linear edges, duplicate-edge error or
  `no_raise` empty labels), integer cut (0.1% edge adjustment), zero-aware
  positive/negative splits, per-date/per-group buckets.
- Prepared data drops rows with missing forwards/groups/quantiles, reports
  input/finite/forward/binning counts and fails above `max_loss`.
- Performance kernels reproduce the pinned formulas (Spearman IC, demeaned
  weights, weighted factor returns, yearly alpha/beta, quantile means/SE,
  turnover, rank autocorrelation, IC t/CI and BH inference).
- Group-IC tables, `by_date`/`by_group` quantile tables, PIT preparation,
  Fama-MacBeth and `factor_model_inference` remain for the next slice.

## Tests and gates

Oracle parity tests for the 15-row preparation fixture and the 60-day
five-asset performance fixture; invalid-input/loss cases. Full CTest.
