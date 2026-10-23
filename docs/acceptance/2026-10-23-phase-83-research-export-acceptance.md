# Phase 83 Export Acceptance — Research CSV and Manifest Artifacts

## Scope

Adds `export_factor_research(result, directory, charts=false)` and
`research_export_result` to the bounded research evaluator. Writes a
deterministic set of CSV tables plus a JSON manifest into a new directory and
never overwrites an existing export target. `research_result` now records the
`stability_lags` used, so exported stability rows carry their true lag.

## Behavior

- Target exists (file or directory) → `factor_data_error` ("evaluation
  export already exists and will not be overwritten").
- `charts=true` → `factor_data_error`; the native library has no raster chart
  renderer, so chart export fails closed rather than silently omitting files.
- Empty directory name → `factor_data_error`; filesystem creation/write
  failures also fail with `factor_data_error`.
- Files: `ic.csv` (date x horizon), `ic_summary.csv`, `quantile_returns.csv`
  (quantile, label, mean, standard error), `turnover.csv`, `stability.csv`,
  `spreads.csv`, `coverage.csv`, `diagnostics.csv` and `manifest.json`
  (status, exported file list and the evaluator manifest entries).
- Numbers use the shared Python-style float formatter with non-finite values
  written as empty cells; strings are RFC-4180 quoted; timestamps use the
  shared ISO-8601 formatter. CSV bytes are native-determined, not pandas
  `to_csv` bytes.

## Tests and gates

New `FactorResearch.ExportsCsvTablesWithoutOverwriting` exports a manually
built result to a unique temporary directory, asserts all nine files, the
`ic.csv` header/values, the manifest contents and turnover rows, then asserts
that a second export and a chart export both fail with `factor_data_error`,
and removes the directory. Full headless CTest: **711/711 passed**. Sanitizer
not re-run for this slice.
