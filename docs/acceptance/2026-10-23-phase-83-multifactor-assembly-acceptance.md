# Phase 83 Acceptance — Multi-Factor Research Assembly

## Scope

Adds `evaluate_factor_research_panel(factors, prices, spec)` for multiple
named factor panels. It evaluates each panel through the accepted
single-factor route, concatenates signals with the correct factor name,
derives pairwise correlations (`min_assets` guard) and explicit comparisons,
and reports an aggregate status.

## Behaviour

- Factor names must be non-empty and unique; comparisons must reference known
  factors and fail before evaluation otherwise.
- Status is `OK` when every factor is OK, `UNEVALUABLE` when every factor is
  unevaluable, and `PARTIAL` for any mixed outcome.
- Signals from each factor are relabelled with that factor name; all other
  signal fields (direction/value/processed/group) follow the single-factor
  contract.
- Correlations use per-date Pearson correlations across common assets;
  comparisons use the accepted mean-IC/ICIR utilities.
- Manifest records factor, correlation-row and comparison-row counts.

## Test evidence

`FactorResearch.EvaluatesMultipleFactorPanelsWithComparisons` evaluates F and
G = -F on the 60-date x 5-asset fixture:

- status OK, two factor results, 600 signal rows labelled F/G;
- one correlation row F/G: mean `-1.0`, 60 dates;
- two comparison rows; the `1D` mean-IC difference is
  `-0.35666666666666664` with 60 dates per side;
- duplicate factor names and unknown comparison factors fail with
  `factor_data_error`.

## Remaining research gap

Grid-driven non-day label naming (exchange-calendar labels other than `<N>D`)
is not implemented. All other bounded research routes — preparation,
precomputed/next-open labels, universe, signals/groups, segments, correlation
and comparison utilities, multi-factor assembly and export — are accepted.

## Gates

Full Release and ASan+UBSan suites: **733/733 passed**. Visual NOT_RUN.
