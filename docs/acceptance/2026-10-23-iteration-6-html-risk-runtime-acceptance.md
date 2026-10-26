# Iteration 6 slice 1 — HTML, Risk Projection and Runtime Core

## Scope

Adds the offline HTML renderer (`report/html.hpp`), the Phase 67 risk
projection (`report/risk.hpp`) and the native runtime core
(`runtime/runtime.hpp`) with catalog/spec/request/plan/batch/run/session/
snapshot/artifact-boundary types and a builtin catalog. Nine new/changed
paths plus tests; no dependency change.

## HTML renderer (Phase 89 core / 86 substitution)

- Renders `report_document` sections as semantic HTML: metric cards, tables
  with captions, notes, metadata definition list and inline SVG line charts.
- All label/text/cell content is escaped (`& < > " '`); `<script>` in a
  title/metric/note is rendered as text.
- Self-contained: inline CSS and SVG, no external asset or network request;
  deterministic bytes with no timestamps. `zh`/`en` labels are selectable via
  the `lang` option. Short series render an explicit no-data message.
- Malformed table rows, mismatched series lengths and non-finite chart
  coordinates fail with `input_error`.
- **Declared substitution:** the factor HTML requirement names an offline
  ECharts asset; the native renderer uses inline SVG instead and does not
  claim ECharts or pixel-identical HTML bytes.

## Risk projection (Phase 91 slice)

`build_risk_report(risk_validation_report)` produces the `risk_validation`
section with status/forecast/refit metrics, diagnostics, backtest metrics
(including the Basel zone), forecast-event and refit tables (normal vs
Weibull parameter columns), the exact disclosure note and digest/schema
metadata. The insufficient/unsupported shape has empty tables and a reason
metric, with no backtest metrics. No render-time recalculation.

## Runtime core (Phase 92 slice)

- `operation_spec` carries operation/capability/domain, a typed
  `std::function<runtime_value(const runtime_inputs&)>`, deterministic/
  rng/semantic-mode/approval/provenance metadata.
- `operation_catalog` validates non-empty identifiers, callable presence,
  unique operation IDs and the one-capability/one-semantic-mode rule, sorts
  operations and derives a SHA-256 catalog digest over canonical metadata.
- `operation_request` snapshots input/config maps and computes their digests;
  `plan` validates every request without running; `batch` enforces the plan's
  catalog digest and executes in order; `run` invokes the callable with the
  isolated inputs only (config is recorded in metadata but not forwarded,
  matching the source).
- `analysis_session` caches `(operation, input_digest, config_digest)` with
  hit/miss metadata and `clear`; `analysis_snapshot` owns an isolated copy and
  digest; `artifact_bundle` is a data-only artifact boundary.
- `builtin_catalog` registers two selected native equivalents:
  `metrics.sharpe_ratio` (returns dataframe) and
  `risk.backtesting.kupiec_lr` (observations/exceptions/confidence).
  Operation catalog fingerprints are native implementation identities, not
  Python implementation hashes or output-value hashes.

## Declared gaps

- XLSX renderer (Phase 90), CMake install/export packaging and installed
  consumer validation (Phase 93), full 88b field completion, factor research/
  PIT/portfolio constructor, full factor tear set, ImPlot tears (Phase 85) and
  the Phase 87 performance study remain open.
- Runtime registration covers two operations; the source catalog's remaining
  native equivalents are classified in the close-out ledger, not silently
  dropped.
- HTML has no ECharts asset, PDF, or visual regression fixture.

## Tests and gates

`report_html_test.cpp` (2), `report_risk_test.cpp` (2), `runtime_test.cpp`
(5): escaping/determinism/SVG/no-data, risk projection (ok and insufficient),
plan/batch order, catalog validation/resolution, session caching, snapshot
digests, builtin operations and artifact boundary. Full headless CTest:
**699/699 passed**. Sanitizer: not re-run (no claim). ImPlot unchanged.
