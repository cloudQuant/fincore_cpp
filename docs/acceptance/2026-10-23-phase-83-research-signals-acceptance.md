# Phase 83 Acceptance — Research Signals and Group Coverage

## Scope

`research_result` now owns a `signals` table and `research_spec.groups`
supplies an asset-to-group mapping:

- Each signal row records date, asset, factor label (`factor` for the
  single-factor evaluator), direction (+1/-1), adjusted value, processed
  value, preprocess name and group.
- Directions come from `direction_overrides`; the adjusted value is the raw
  factor times the direction, while `processed_value` is the post-preprocess
  panel value.
- Groups must cover every evaluated asset; missing coverage and empty
  asset/group names fail with `factor_data_error`. Without a mapping every
  signal is labelled `default`, matching the source resolver.
- The manifest records the number of explicit group entries.
- Signals are emitted by the price, next-open, precomputed and universe
  routes; no kernel is re-entered.

## Tests and gates

`FactorResearch.RecordsSignalsWithGroupsAndDirections` asserts 300 signal
rows for the 60-date x 5-asset fixture, G1/G2 labels, a `-1` direction whose
value equals the negated raw factor, the default group when no mapping is
supplied, and fail-closed missing/empty groups.

Full Release and ASan+UBSan suites: **732/732 passed**. Visual NOT_RUN.

## Remaining research gap

Multi-factor evaluator assembly (multiple factor columns, paired comparisons
inside one result and combination signals) remains open; group mappings,
signals, correlations/comparisons utilities, segments, precomputed labels,
next-open labels and universe masks are accepted.
