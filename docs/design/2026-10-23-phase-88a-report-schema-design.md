# Phase 88a Design — Shared Report Schema and JSON

## Source contract

`report/models.py` defines `ReportDocument`/`ReportSection` as owning,
versioned containers of domain, title, sections, metrics/tables/series and
metadata. `risk/report.py` and `factor_analysis/report/*` project computed
results into that model; renderers consume it without recalculating metrics.
The Python JSON serialization is deterministic (`sort_keys=True`,
`allow_nan=False`) and keeps metric/table/series units, labels and notes.

## Native decision

- `include/fincore/detail/json.hpp` extracts the deterministic writer already
  proven in Phase 67 (Python-style float repr, UTF-8/ASCII escaping,
  sorted-key structural order). `src/risk/report.cpp` now includes it; its
  report tests are unchanged and green.
- `include/fincore/report/models.hpp` adds typed `report_document`,
  `report_section`, `report_metric`, `report_table`, `report_series` and
  `report_cell` (missing/number/text/boolean). Tables carry column labels and
  row-major cells; series carry x/y plus axis labels/units; metrics carry
  key/label/unit and exactly one of number/boolean/text (or missing).
- `report_document::to_json()` writes a deterministic schema:
  `domain`, `metadata`, `schema_version`, `sections`, `title`; each section
  writes `key`, `metrics`, `notes`, `series`, `tables`, `title`; rows must
  match their column count and series x/y must match, otherwise
  `input_error`. Non-finite values are rejected by the writer.
- A reduced factor projection (`factor/report.hpp`) builds the
  `factor_summary` and `factor_performance` sections from the compute-once
  model: observations/periods/quantiles, mean IC/alpha/beta per period, mean
  turnover per quantile, IC/quantile/alpha-beta/turnover tables and factor
  return series, with a fingerprint metadata entry and reference notes.

## Tests

`report_models_test.cpp` (2 tests): exact deterministic JSON for a document
with number/text/missing metrics, a mixed table, a series and metadata;
malformed rows/series and non-finite values rejected. `factor_report_test.cpp`
(1 test): model projection fields, table/series counts, metric keys and
deterministic JSON.

## Gates

Full headless CTest 690/690; sanitizer not re-run for this slice. Rendering
(HTML/XLSX) and the full factor tear set remain open.
