# Iteration 8 — Native Full Parity (DataFrame + ImPlot): Design

**Revision:** R3, native-core scope covering visualization backends, plugins,
native error/container behaviour and the Python test corpus.
**Status:** Planned, **NOT_RUN**.
**Oracle:** `3e9ef205ef2023f92e89fddd1052d46b16429c64`.
**Base:** C4 native manifest
`245ae75336912e30bcf6f34016ed4451116ec4ed719771a02d4b6e46ab0516e3`.
[Requirements](requirements.md) and the [shared contract](../delivery-contract.md)
govern.

## 1. Architecture

```text
pinned Python source (oracle only)
        │  fixtures / expected tables / chart values / behavior cases
        ▼
C++23 fincore native core
  - DataFrame/vector owning results; structured errors
  - factor/risk/portfolio/performance/runtime models
  - chart data → ImPlot, Plotly/Bokeh-compatible documents, PDF
  - plugin ABI + registry + native serialization
  - native HTML/ECharts/XLSX/PDF report writers
        │
        ▼
native tests, offscreen captures, artifact checks, installed consumer
```

There is no Python binding, pandas compatibility layer or pytest execution in
this iteration. Python is used only to generate oracle fixtures and expected
behavior cases. Python object identity is an explicit non-claim.

## 2. P100 — Baseline, route split and harness

1. Freeze C4 and the Python pin; record the dependency environment.
2. Import the 952-route ledger and 273-operation inventory into a single table
   with deterministic IDs.
3. Classify every row:
   - **NATIVE_TARGET** — computational/domain/artifact behaviour implemented
     natively, including Plotly/Bokeh/PDF artifacts, the plugin ABI, structured
     errors, DataFrame/vector containers and serialized models;
   - **EXTERNAL_OPTIONAL** — PyMC posterior APIs and live provider SDK/network
     routes only; the native data model/offline path is still implemented;
   - **EXTERNAL_BLOCKED** — a credential/service/runner needed by a gate.
4. Build the native parity harness:
   - Python fixture generator writing CSV/JSON for tables, scalars, errors,
     chart series, Plotly/Bokeh document values and PDF text expectations;
   - C++ fixture loaders and artifact comparators;
   - route validator;
   - ImPlot offscreen capture and PDF page rendering checks.
5. Copy Iteration 7's benchmark baseline for native no-regression checks.

Expected split shape (exact IDs in P100):

- native: 10 PARTIAL classes' native members, computational DEFERRED rows,
  Plotly/Bokeh/PDF routes, plugin/extension routes, exception/container
  behaviour, specialized chart/tear projections, native runtime operations;
- optional: PyMC/posterior routes and live provider network SDKs only.

## 3. P101 — DataFrame, value and error semantics

### Mapping

| Python contract | Native contract |
|---|---|
| pandas DataFrame | `fincore::dataframe` (`hmdf::StdDataFrame<int64_t>`) |
| pandas Series / NumPy 1-D | owning `std::vector<double>` or span |
| NumPy 2-D | row-major dense matrix with explicit rows/columns |
| MultiIndex labels | index plus asset/group key columns or keyed rows |
| pandas object/string columns | `std::string` columns |
| tz-aware timestamps | UTC nanoseconds at the native boundary |
| NaN/None/NA | NaN for doubles; explicit absent/mask for other columns |
| Python exception classes | native error category + code + report + aliases |

### Rules

- Result order is explicit: dates ascending where the source sorts, assets by
  documented key/insertion order, columns in source order.
- Absent `(date, asset)` differs from present NaN; eligibility uses explicit
  membership.
- Returned objects own data; caller mutation cannot corrupt caches.
- Native errors carry category, stable code, message and optional report map;
  aliases mirror source predicates. Python exception inheritance is not
  claimed.
- Conversion helpers live in `src/dataframe.cpp`/private headers; no pandas
  API emulation at runtime.

Tests: empty/single-row/unbalanced/duplicate/nonfinite/string/bool tables,
index/column order, missingness, dtype class, mutation isolation and error
predicates/aliases.

## 4. P102–P105 — Native route closure

### P102 metrics/portfolio/performance

- Ratios, drawdown, consecutive, rolling, stats, yearly, alpha/beta, timing.
- Positions, transactions, round-trips, capacity.
- Performance returns/cashflows/inference/disclosures.
- Aliases/facades map to canonical kernels with fixtures; 10 PARTIAL classes
  member-by-member.

### P103 risk/optimization/simulation/attribution

- GARCH/EGARCH/GJR, EVT/GPD/GEV, backtesting/calibration, risk models/report,
  walk-forward.
- Optimizer objectives, frontier, risk parity, constraints.
- Simulation paths/bootstrap/scenarios/Monte Carlo.
- Brinson/Fama-French/style/performance native leaves; stateful attribution
  facades as native owning models.
- PyMC/posterior routes remain `EXTERNAL_OPTIONAL`.

### P104 factor analysis

- Data/calendar/PIT/performance/inference/analysis/models/tears/portfolio/
  costs/research/report.
- Grouped/full snapshots, event models, research tables/manifests/exports,
  specialized chart-data projections.
- 10 PARTIAL classes member-by-member.
- Custom calendar/timedelta helpers native where they operate on
  timestamps/labels.

### P105 runtime and data models

- Register every native-compatible operation among 273 with typed inputs,
  source IDs, deterministic digests and tests.
- DataSnapshot/provider request/response models and offline validation.
- Live provider SDK/network operations are `EXTERNAL_OPTIONAL` with named
  credentials; no placeholders.
- Native serialization of selected models and extension fingerprints.

## 5. P106 — Native visualization backends

### ImPlot

Extend chart-data projections to every selected panel. ImPlot primitives:

| Panel | Projection | Primitive |
|---|---|---|
| Returns / rolling Sharpe / drawdown | `chart_series` | line/fill |
| Monthly / IC heatmaps | matrix table | heatmap |
| Grouped bars | categories/series | bars |
| Histogram / event counts | bins/counts | bars |
| Correlation/frontier | matrix/points | heatmap/scatter |
| QQ | quantile pairs | scatter + reference |
| Violin/KDE | density samples | filled polygon/line |
| Quantile cumulative | per-quantile series | multi-line |
| Date-wise spread | series | line/zero line |
| Grouped tears | neutral/group series | panels |

Offscreen captures use the existing hidden-window/glReadPixels harness; images
are inspected and hashed. Interactive windows remain `NOT_RUN` without a
display.

### Plotly-compatible documents

- A native `chart_document` model (traces, axes, legend, annotations) is
  serialized to a Plotly.js-compatible JSON object and a self-contained HTML
  file with a pinned Plotly asset and license notice.
- Tests compare data/axes/legend values against the Python oracle; the exact
  third-party schema is covered for the emitted subset and documented.
- No Python `plotly.graph_objects.Figure` object is claimed.

### Bokeh-compatible documents

- Bokeh's document/model schema is large; native Bokeh routes emit a
  self-contained HTML/JSON chart document with equivalent data, axes, legends
  and series using the native chart-document model. BokehJS model identity is
  not cloned.
- Equivalence tests compare chart values/axes/legends; format differences are
  recorded in the route row.

### PDF

- A native PDF writer emits text, tables and vector chart paths for the selected
  PDF report route. A bundled open-source font covers UTF-8 labels; its license
  is installed.
- Tests verify page count, extracted text, table rows and rendered page images.
  Byte identity with the Python PDF backend is not claimed.

## 6. P107 — Plugins, extensions and serialization

- New `include/fincore/plugin.hpp`:
  - `plugin_manifest` (name, version, ABI, capabilities, fingerprint),
  - `plugin_registry` with deterministic ordering,
  - `load_plugin(path)` using `dlopen`/`LoadLibrary`, static built-ins, and
    explicit lifecycle hooks.
- Manifest-based discovery scans configured directories; load failures produce
  structured native errors. Python `entry_points` semantics are replaced by the
  native manifest contract; discovery records, ordering and fingerprints are
  tested with sample shared-library plugins.
- Extension snapshots include registered hooks, renderer registrations and
  fingerprints in a native value model.
- Native serialization covers the selected model types with versioned
  envelopes; Python object wire compatibility is not claimed.

## 7. P108 — Reports, tears and Python test port

### Reports/tears

- Shared report schema (units, legends, assets, tables, series) remains the
  owning document.
- Factor/risk/portfolio report projections consume native models; grouped tear
  sections use the new panels.
- HTML/ECharts/XLSX/PDF writers are native; rendering consumes prepared models
  and does not recompute kernels.

### 228-file test port

A `test-port-ledger.tsv` maps every pinned Python test file (228) to:

- native GoogleTest case(s), fixture(s) and oracle data; or
- `PYTHON_IDENTITY_VARIANCE` for assertions that depend only on Python object
  identity, monkeypatching or import machinery, with a native-equivalent
  contract test where one exists and an owner-approved reason where none does.

Rules:

- Every file appears exactly once; no silent gaps.
- Numerical/behavioral assertions are ported, not merely counted.
- The native tests execute in CTest; pytest is not run.
- A test-port progress dashboard counts files mapped, native tests added and
  variances; the final acceptance requires all 228 accounted.
- The port is expected to expose missing native behavior; those gaps feed back
  into P102–P107.

## 8. P109 — Native parity campaign

1. Route validator: every row `NATIVE_PASS` or named `EXTERNAL_OPTIONAL`/
   `EXTERNAL_BLOCKED`; no unexplained gap.
2. Oracle fixture differential for every native route.
3. DataFrame/error/container matrix.
4. ImPlot/Plotly/Bokeh/PDF artifact checks and offscreen captures.
5. Plugin discovery/registry/fingerprint/lifecycle tests.
6. 228-file test-port ledger complete; native tests pass.
7. Native performance no >10% median regression.
8. Optional manifest exact, named and signed.

## 9. P110 — Packaging and closeout

- Native install/export and headless consumer on macOS arm64 and Ubuntu
  arm64/x86_64; macOS Release/ASan+UBSan; Windows `EXTERNAL_BLOCKED` unless a
  runner exists.
- Record source manifests, fixtures, test-port ledger, visual/artifact captures,
  benchmark samples and route ledger.
- Acceptance states native behavioral/artifact parity and the explicit
  non-claims for Python object identity/pytest.

## 10. Optional integrations

Only PyMC posterior APIs and live provider SDK/network routes are optional.
They have native data-model/offline coverage where applicable and a named
external prerequisite; they are not PASS and do not block native route closure.

## 11. Performance and artifact protocol

- Native benchmark: AB/BA fresh processes, predeclared batches, CPU affinity;
  >5% dispersion is INCONCLUSIVE.
- No-regression: >10% median increase beyond noise is FAIL or dispositioned.
- Visual: offscreen ImPlot at fixed size; inspect image and chart values.
- Plotly/Bokeh: compare document data/axes/legend/series to oracle values and
  render the self-contained HTML in a headless browser; declare format
  differences.
- PDF: extract text/rows, count pages and inspect rendered page images.

## 12. Risks and mitigations

| Risk | Mitigation |
|---|---|
| DataFrame does not equal arbitrary pandas API | Implement only required native operations; document the native contract |
| Plotly/Bokeh schema differs | Emit a documented compatible subset; validate data/axes/legend; do not claim object identity |
| PDF fonts/Unicode | Bundle an open-source font with license; verify extracted text and rendered pages |
| Plugin ABI portability | Versioned ABI/manifest; macOS/Linux first; Windows only with a runner |
| 228 test files are large | Ledger-driven porting; prioritize behavioral assertions; explicit identity variances |
| Python tests monkeypatch internals | Native-equivalent contract tests; owner-approved variance only when no equivalent exists |
| Route split hides gaps | Owner-approved optional manifest; every route named; no PASS for optional |
| Performance noise | CPU affinity, larger batches, platform replication; INCONCLUSIVE reported |
| Shared C++ files across phases | One editor per path; phase order; integration wave |

## 13. Sequence

```
P100 split/harness
  → P101 DataFrame/errors
  → P102/P103/P104/P105 native routes
  → P106 visualization backends (ImPlot/Plotly/Bokeh/PDF)
  → P107 plugins/serialization
  → P108 reports/tears + 228-file port
  → P109 native parity campaign
  → P110 packaging/closeout
```

P102–P105 may overlap on disjoint paths; P106 consumes chart data; P108
consumes all native models and the port ledger. P109 is the only close-out.

## 14. Acceptance mapping

| Requirement | Evidence |
|---|---|
| FR-1 | frozen C4/oracle; route split counts |
| FR-2 | DataFrame shape/order/missingness/isolation tests |
| FR-3 | native route tests and alias fixtures |
| FR-4 | 273-operation registration table |
| FR-5 | provider model/offline fixtures; optional live rows |
| FR-6 | ImPlot captures, Plotly/Bokeh documents, PDF pages |
| FR-7 | plugin ABI/registry/fingerprint/lifecycle tests |
| FR-8 | report/tear HTML/XLSX/PDF fixtures |
| FR-9 | 228-file port ledger and native tests |
| FR-10 | oracle fixture differentials |
| FR-11 | native benchmark report |
| FR-12 | platform/consumer matrices |
| FR-13 | optional manifest with owner sign-off |
| FR-14 | manifests, hashes and logs |
