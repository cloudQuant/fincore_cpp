# Phase 83 Completion Slice — Precomputed Forward-Return Labels

## Scope

Adds an evaluator overload that consumes a caller-supplied
`forward_return_table` instead of deriving labels from prices:

```cpp
evaluate_factor_research(factors, precomputed_returns, spec)
```

This closes the precomputed-label validation gap for the bounded research
surface while leaving grid/next-open price bases and multi-factor assembly
open.

## Behaviour

- The table must contain non-empty labels, dates and assets, be label-major,
  have exactly `dates x assets` values per label, and have unique
  `(date, asset)` keys.
- Each requested period maps to the canonical label `<N>D`; a missing label
  fails with `factor_data_error`.
- Quantiles are assigned once from the factor panel (before return
  availability), then each horizon keeps only rows with a finite precomputed
  value and an assigned quantile, matching the prepare-by-horizon semantics.
- The result manifest records `label=precomputed`; segments and other
  provenance are unchanged.

## Tests and gates

`FactorResearch.EvaluatesPrecomputedForwardReturns` uses a 5-date x 3-asset
panel with monotonic factors and two precomputed horizons: status OK, labels
`1D`/`5D`, five IC dates, all IC values `~1`, coverage 15 per horizon, two
quantile rows, and the manifest label. Missing labels, malformed value
lengths and duplicate `(date, asset)` keys fail closed.

Full Release and ASan+UBSan suites: **729/729 passed**. Visual NOT_RUN.

## Remaining research gaps

Grid/next-open price bases, universe/group inputs, multi-factor evaluator
assembly remain open.
