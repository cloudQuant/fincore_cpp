# Phase 83 Acceptance — Weighted Factor Combinations

## Scope

`research_spec::combinations` (name → factor → weight) is now honoured by
`evaluate_factor_research_panel`. Each combination is built from the processed
panels of the named factors and appended to the multi-factor result alongside
the input factors.

## Behaviour

- Combinable factors are the already-preprocessed panels of the input factors
  (`research_panel`); weights are applied to those processed values, matching
  the Python oracle's `_combination_values`, which blends `processed` matrices.
- The blend is restricted to cells present in every non-zero weighted factor
  (intersection of `(date, asset)` keys); zero weights are skipped.
- A combination is evaluated with `preprocess = "none"`, no direction overrides
  and without recursive combinations, so the blend is scored as supplied.
- Combination names must be non-empty, must not collide with factor names, all
  referenced factors must exist, weights must be finite, and at least one
  weight must be non-zero; violations fail with `factor_data_error` before any
  factor is evaluated.
- Comparisons may reference combinations; correlations and comparisons include
  the blended panels.
- Signals gain a combination row set whose `factor` field is the combination
  name. Aggregate status counts combinations (`OK` / `UNEVALUABLE` / `PARTIAL`).
- Manifest records `factor_count` (inputs + combinations), `combination_count`,
  `correlation_rows` and `comparison_rows`.

## Test evidence

`FactorResearch.EvaluatesWeightedFactorCombinations` on the 60-date x 5-asset
fixture with factors F and G = -F, combination
`combo = 0.5*F - 0.5*G`, and comparison `F_vs_combo`:

- status OK, three factor results (F, G, combo), 900 signal rows;
- correlations: F/G `-1.0` (60 dates), F/combo positive (60 dates), G/combo
  negative (60 dates); the sign follows the processed-panel blend because
  F/combo compare raw against ranked values;
- both `F_vs_combo` period rows have mean-IC difference `0.0` (F and combo are
  the same processed panel, so their IC series agree exactly);
- manifest counts `factor_count=3`, `combination_count=1`,
  `correlation_rows=3`;
- unknown factor, empty/zero weights, a name colliding with a factor, and a
  non-finite weight each raise `factor_data_error`.

## Remaining research gap

Grid-driven non-day label naming (exchange-calendar labels other than `<N>D`)
is the only bounded research route still open. All preparation, label,
universe, signal/group, segment, correlation, comparison, combination,
multi-factor and export routes are accepted.

## Gates

- COMPUTE: parity asserted by deterministic identities and accepted kernels.
- BUILD: Release and ASan+UBSan suites: **735/735 passed** (Homebrew LLVM
  22.1.8; ASan+UBSan Debug).
- VISUAL: NOT_RUN for this slice; no rendering change.
