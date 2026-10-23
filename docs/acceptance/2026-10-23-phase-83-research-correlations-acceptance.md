# Phase 83 Completion Slice Acceptance — Research Correlations and Comparisons

## Scope

Closes the paired-factor correlation and comparison gaps in the bounded
research surface with two additive utilities:

- `research_factor_correlations(panels, min_assets)` computes per-date Pearson
  correlations across common assets for every factor pair, then the mean and
  sample standard deviation across dates. Dates without enough common finite
  assets are skipped.
- `research_factor_comparisons(results, comparisons)` computes, per period,
  the mean-IC and ICIR differences for explicitly named factor pairs from the
  already-stored IC tables. ICIR is `mean / sample_std`; a near-zero deviation
  below `1e-15` yields a missing ICIR instead of an exploding ratio.

Both consume already-evaluated data, so no research corpus is recomputed.
Names must be non-empty and unique; unknown comparison factors fail with
`factor_data_error`.

## Oracle

Pinned Python `4b8f31b1` (`_correlation_rows`), 4 dates x 3 assets:

| pair | mean correlation | std | n |
| --- | --- | --- | --- |
| X/Y | 0.27885564838462495 | 0.561086034683472 | 4 |
| X/Z (Z=-X) | -1.0 | 0.0 | 4 |
| Y/Z | -0.27885564838462495 | 0.561086034683472 | 4 |

JSON SHA-256:
`015a7eba1ec026fb9ead87388aa570d6cfbf286a1ab69ae2baae4e31dd1c27ce`.
With `min_assets=4` and only three assets, no date qualifies and the result
keeps the pair rows with zero dates and NaN moments.

Comparison fixture: left IC `0.1/0.2/0.3` (mean 0.2, ICIR 2.0), right IC
`0.05/0.05/0.05` (mean 0.05, ICIR missing): mean-IC difference 0.15, ICIR
difference missing, three dates per side.

## Declared gaps

Segments, grid/next-open labels, universe/group inputs, precomputed-label
validation and multi-factor `evaluate_factor_research` assembly remain open;
the utilities above operate on caller-supplied panels/results rather than
extending the single-factor evaluator.

## Gates

Full Release and ASan+UBSan suites: **726/726 passed**. Visual NOT_RUN.
