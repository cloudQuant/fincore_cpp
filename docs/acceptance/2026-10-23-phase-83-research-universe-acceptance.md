# Phase 83 Completion Slice — Research Universe Mask

## Scope

Adds an evaluator overload with a wide boolean eligibility mask:

```cpp
evaluate_factor_research(factors, prices, spec, universe)
```

Cells whose `(date, asset)` is absent or `false` are removed before quantile
assignment and forward-return preparation.

## Behaviour

- The mask must be a `dataframe` indexed by date with genuine boolean asset
  columns; any double column, an empty mask, or a duplicate index fails with
  `factor_data_error`.
- A missing date or asset column means ineligible (mask reindex semantics).
- If the mask removes every factor cell, evaluation fails closed.
- The result manifest records `universe=applied`.
- Wide date x asset masks only; MultiIndex/long masks and group mappings are
  not represented by this native overload.

## Tests and gates

`FactorResearch.AppliesWideUniverseMask` compares the 300-cell baseline
(60 dates x 5 assets) with a mask that excludes asset E: coverage becomes 240
per horizon, status stays OK and the manifest records the mask. A four-asset
four-date mask yields coverage 16; an all-false mask and a numeric mask both
fail with `factor_data_error`.

Full Release and ASan+UBSan suites: **730/730 passed**. Visual NOT_RUN.

## Remaining research gaps

Group mappings and multi-factor evaluator assembly remain open. Segments are provenance only in the source, so no
multi-segment result table is missing.
