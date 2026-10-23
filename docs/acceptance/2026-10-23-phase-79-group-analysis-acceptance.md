# Phase 79 Acceptance — Grouped Factor Analysis Model

## Scope

Adds `factor_analysis_config::by_group` and `factor_analysis_model::
grouped_results`: one `factor_group_analysis` per sorted group, each owning
quantile statistics, weights, factor returns, quantile returns, IC and mean
IC, lagged turnover and rank autocorrelation. The model fingerprint binds the
`by_group` flag.

## Behaviour

- `by_group=true` requires every prepared row to carry a group; a missing
  group fails with `factor_data_error`.
- Groups are processed in sorted label order. Per-group kernels use the
  source `_group_analysis` contract: weights/returns use `long_short` and
  `equal_weight` without group adjustment, IC uses no group adjustment, and
  turnover/rank use the configured lags.
- Aggregate model tables continue to describe the whole panel; the source's
  duplicated aggregate-versus-group snapshot fields are not mirrored.

## Oracle

Pinned Python `4b8f31b1`, 4 dates x 4 assets, groups G1=A/B and G2=C/D,
quantiles 1/2 per group, windows 1D, lags 1/2; script
`/tmp/fincore-group-python.*/oracle.py`, JSON SHA-256
`d7edd32618e412d115a9ec28c2acdfe13b036b36fe7d0719128f4f3fc02626fb`.

| quantity | G1 | G2 |
| --- | --- | --- |
| quantile means | 1.0 / 2.0 | 3.0 / 4.0 |
| quantile counts | 4 / 4 (50% each) | 4 / 4 (50% each) |
| mean quantile returns | -0.005000000000000001 / 0.004999999999999999 | -0.0050000000000000044 / 0.0049999999999999975 |
| weights | A -0.5, B 0.5 | C -0.5, D 0.5 |
| first factor return | 0.005 | 0.005000000000000001 |
| IC / mean IC | 0.9999999999999999 | 0.9999999999999999 |
| turnover lags / last values | 1, 2 / 0 | 1, 2 / 0 |
| lag-1 rank autocorrelation (last) | 0.9999999999999998 | 0.9999999999999998 |

The test asserts these values, the 8 weight rows per group, the fingerprint
difference versus `by_group=false`, and the missing-group failure.

## Declared gaps

Group tear-sheet rendering and group sections in the factor report are not
modelled (the source factor report is aggregate-only). Group labels are sorted
as strings; heterogeneous label ordering is not reproduced. Per-group
`group_adjust` is not applied, matching the source group helper.

## Gates

Full Release CTest: **724/724 passed**. Full ASan+UBSan: **724/724 passed**.
Visual NOT_RUN.
