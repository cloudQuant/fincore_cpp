# Phase 80 Completion Slice Acceptance — Fama-MacBeth and Model Inference

## Scope

Closes two inference capabilities from the program ledger:
`factor_model_inference(model, alpha)` over the model's stored aggregate IC
snapshot, and a typed `fama_macbeth` cross-sectional regression with i.i.d.
or Bartlett Newey-West standard errors. Also makes an infinite t statistic
produce a zero two-sided p-value (matching the source's `scipy.stats.t.sf`
limit), which removes a prior degenerate-IC divergence.

## Fama-MacBeth

- Inputs: an asset-return panel and an exposure panel keyed by asset labels.
  A single-row exposure panel is broadcast across every return date; a
  multi-row panel is matched by timestamp, and missing exposure dates are
  skipped rather than imputed.
- For each usable date the kernel fits `R_i = alpha + beta * X_i` when at
  least two finite pairs remain and the exposure cross-section has
  non-negligible variance, then reports the time-series mean of the
  intercepts/slopes with i.i.d. or HAC standard errors.
- `fama_macbeth_result` carries both estimates, the fitted cross-section
  count, the covariance choice and the HAC lag count.
- Validation mirrors the source: at least two asset columns, two fitted
  cross-sections minimum for HAC, `newey_west_lags < n_cross_sections`,
  chronological index for Newey-West, and exposures covering every return
  asset column.

Oracle fixture (4 dates x 3 assets; script
`/tmp/fincore-phase80-python.b3xlmA/oracle.py`, raw SHA-256
`3b45d2d695b3b947aeff07cba08884b176bf03f51a29e30ac1ac980b1b46fb69`):

| case | intercept mean / SE | exposure mean / SE | t (exposure) |
| --- | --- | --- | --- |
| iid | -0.0006458333333333327 / 0.0019885013782950925 | 0.010125 / 0.000125 | 80.99999999999918 |
| Newey-West lag 1 | same mean / 0.0010145293235111015 | same mean / 8.267972847077034e-05 | 122.46048925498683 |
| Newey-West lag 2 | same mean / 0.0008447781801225179 | same mean / 6.25000000000015e-05 | 161.9999999999961 |
| static exposure | -8.333333333333462e-05 | 0.010125000000000004 / 0.000125 | 81.0 |
| one missing exposure cell | -0.0002708333333333306 | 0.010125 | 81.0 |

The test asserts these values (HAC t statistics within 1e-9/1e-8 due to
different linalg paths), the static broadcast, the missing row being
skipped, and the lag/coverage failures.

## Model inference

`factor_model_inference` delegates to `information_coefficient_inference`
over `model.information_coefficient` (the aggregate snapshot), with the same
alpha validation and FDR adjustment. The oracle model's constant IC now
yields finite zero p-values instead of throwing, and the test asserts the
two hypotheses, three observations per label and testability.

## Tests and gates

New `FactorPerformance.FamaMacbethMatchesCleanPythonOracle` and
`FactorAnalysis.InfersFromStoredAggregateIcSnapshot`. Full headless CTest:
**714/714 passed**. Sanitizer not re-run for this slice; visual NOT_RUN.
