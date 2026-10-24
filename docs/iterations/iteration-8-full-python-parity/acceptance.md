# Iteration 8 — Native Full Parity (DataFrame + ImPlot): Acceptance

**Revision:** R3, native-core scope covering visualization backends, plugins,
native error/container behaviour and the Python test corpus.
**Status:** **NOT_RUN** — plan template. No row below is accepted until the
candidate source snapshot and evidence exist.

**Execution note (2026-10-24):** the candidate acceptance is recorded in
[2026-10-24-iteration-8-native-parity-acceptance.md](../../acceptance/2026-10-24-iteration-8-native-parity-acceptance.md).
Windows 11 Release (1044/1044 CTest) and the relocated consumer now pass;
Ubuntu remains `EXTERNAL_BLOCKED`; P109 performance remains `INCONCLUSIVE`
because the C4 baseline archive is unavailable. The checklist below is retained
as the plan template and is not itself execution evidence.
**Oracle:** `3e9ef205ef2023f92e89fddd1052d46b16429c64` (`0.5.2.dev0`).
**Base:** C4 native manifest
`245ae75336912e30bcf6f34016ed4451116ec4ed719771a02d4b6e46ab0516e3`.
**References:** [requirements](requirements.md), [design](design.md),
[shared contract](../delivery-contract.md).

## 1. Evidence and status rules

- Every row cites the source snapshot, exact command, exit code, raw log and
  fixture/manifest/artifact hash.
- Route status is `NATIVE_PASS`, `EXTERNAL_OPTIONAL` or `EXTERNAL_BLOCKED`.
  `PLANNED`, `DEFERRED`, `NON_PORT`, `PARTIAL` and unexplained `GAP` block
  close-out.
- `EXTERNAL_OPTIONAL` is limited to PyMC posterior routes and live provider
  SDK/network routes. It is not a PASS; the native data/offline path must still
  be implemented and tested, and the prerequisite is named.
- `EXTERNAL_BLOCKED` names the credential/service/runner; the native/offline
  behaviour must still pass.
- Numerical budgets are fixed before implementation and never lowered after
  results.
- The 228 Python test files are **ported** to native tests, not run through
  pytest. Every file must appear in the test-port ledger.

## 2. P100 — Baseline and route split

- [ ] C4 manifest/archive and Python pin frozen; dependency freeze recorded.
- [ ] 952 route rows imported with stable IDs.
- [ ] 273 runtime operations imported and classified.
- [ ] Split produces exact `NATIVE_TARGET`, `EXTERNAL_OPTIONAL` and
  `EXTERNAL_BLOCKED` counts; no row unclassified.
- [ ] Owner reviews and approves the split and the optional manifest.
- [ ] Route validator fails on missing/duplicate IDs or non-approved statuses.
- [ ] Python oracle fixture generator and C++ fixture loaders exist.
- [ ] Iteration 7 native performance baseline copied.

## 3. P101 — DataFrame, value and error semantics

- [ ] pandas DataFrame ↔ `fincore::dataframe`; Series/NumPy vectors ↔ owning
  vectors/spans; 2-D ↔ dense matrices.
- [ ] Index/row/column order, absent-vs-NaN, dtype class, nonfinite values and
  duplicate/boundary cases covered.
- [ ] UTC nanosecond timestamps at the native boundary; no local-time coercion.
- [ ] Structured native errors expose category, code, message and report map;
  aliases/predicates match source behavior without claiming Python classes.
- [ ] Returned objects own their data; caller mutation/destruction cannot
  corrupt caches or dangle views.

## 4. P102–P105 — Native route closure

- [ ] Metrics/ratios/drawdown/consecutive/rolling/stats/yearly/alpha-beta/timing
  native routes closed.
- [ ] Portfolio positions/transactions/round-trips/capacity native routes
  closed.
- [ ] Performance returns/cashflows/inference/disclosures native routes closed.
- [ ] Risk GARCH/EVT/backtesting/calibration/models/report/walk-forward native
  routes closed.
- [ ] Optimization objectives/frontier/risk-parity native routes closed.
- [ ] Simulation paths/bootstrap/scenarios/Monte-Carlo native routes closed.
- [ ] Attribution Brinson/Fama-French/style/performance native routes closed.
- [ ] Factor data/calendar/PIT/performance/inference/analysis/models/tears/
  portfolio/costs/research/report native routes closed.
- [ ] All 10 `PARTIAL` classes accepted member-by-member with native tests.
- [ ] All `NATIVE_TARGET` runtime operations registered with typed inputs and
  tests; optional operations named.
- [ ] Provider data/snapshot models and offline validation covered; live
  providers are `EXTERNAL_OPTIONAL` with named prerequisites.
- [ ] No `NATIVE_TARGET` route lacks a fixture/test.

## 5. P106 — Native visualization backends

- [ ] Every selected chart route has a native chart-data projection.
- [ ] ImPlot panels exist for: returns/rolling Sharpe/drawdown, monthly and IC
  heatmaps, grouped bars, histogram/event counts, correlation/frontier, QQ,
  violin/KDE, quantile cumulative, date-wise spread, grouped tears.
- [ ] Offscreen ImPlot captures at fixed size are inspected; image hashes
  retained.
- [ ] Plotly-compatible chart documents emit traces/axes/legend/series values
  matching the oracle; self-contained HTML with pinned asset/license.
- [ ] Bokeh-compatible chart documents emit equivalent data/axes/legend/series;
  documented format differences are recorded.
- [ ] Native PDF emits the selected report route's text/tables/vector content;
  page count, extracted text and rendered page images verified; byte identity
  is not claimed.
- [ ] Interactive window/input behaviour is `NOT_RUN` unless a display is
  explicitly provided.

## 6. P107 — Plugins, extensions and serialization

- [ ] Native plugin ABI/version/manifest defined; `dlopen`/`LoadLibrary` and
  static built-ins work on macOS/Linux.
- [ ] Manifest discovery, registry ordering, load errors, fingerprints and
  lifecycle hooks tested with sample shared-library plugins.
- [ ] Extension snapshot/hook/renderer registration behavior covered.
- [ ] Selected native models serialize with versioned envelopes; Python object
  wire compatibility is not claimed.
- [ ] No Python entry-point/importlib dependency.

## 7. P108 — Reports, tears and Python test port

- [ ] Shared report schema covers factor/risk/portfolio sections with units,
  legends, assets and series.
- [ ] Native HTML/ECharts, XLSX and PDF outputs match expected table/series
  values; grouped tears use the new panels; no kernel recomputation.
- [ ] `test-port-ledger.tsv` contains all 228 Python test files exactly once.
- [ ] Every file maps to native GoogleTest case(s)/fixture(s) or an
  owner-approved `PYTHON_IDENTITY_VARIANCE` with a native-equivalent contract
  test where one exists.
- [ ] Native tests for the ported behavior execute in CTest; no pytest run.

## 8. P109 — Native parity campaign

- [ ] Route ledger: every row `NATIVE_PASS` or named
  `EXTERNAL_OPTIONAL`/`EXTERNAL_BLOCKED`; zero unexplained gaps.
- [ ] Oracle fixture differential: every native route compares tables/scalars
  under its budget; integer/order/shape exact.
- [ ] DataFrame/error/container matrix passes.
- [ ] ImPlot/Plotly/Bokeh/PDF artifacts verified; offscreen captures inspected.
- [ ] Plugin ABI/registry/fingerprint/lifecycle tests pass.
- [ ] 228-file test-port ledger complete; native tests pass.
- [ ] Native performance no median regression >10% versus Iteration 7.
- [ ] Optional manifest exact, named and signed; no optional route claimed as
  PASS.

## 9. P110 — Packaging and closeout

- [ ] Native install/export and headless consumer on macOS arm64 and Ubuntu
  arm64/x86_64.
- [ ] macOS Release and ASan+UBSan; Ubuntu release; execution mode declared.
- [x] Windows result recorded or `EXTERNAL_BLOCKED` with a named runner.
  Windows 11 x86_64 Release: 1044/1044 CTest and relocated consumer exit 0
  (`docs/evidence/iteration-8/p110/windows-*`).
- [ ] Source manifests, fixtures, test-port ledger, visual/artifact captures,
  benchmark samples and route ledger hashed and retained under
  `docs/evidence/iteration-8/`.
- [ ] Acceptance record states native behavioral/artifact parity and explicit
  non-claims for Python object identity/pytest.

## 10. Gate matrix

| Gate | Evidence | Status |
|---|---|---|
| Route split | exact native/optional/blocked counts; owner approval | NOT_RUN |
| Native route closure | zero unexplained; fixtures/tests | NOT_RUN |
| DataFrame/error semantics | shape/order/missingness/dtype/isolation/error matrix | NOT_RUN |
| Runtime | native operations registered; optional named | NOT_RUN |
| ImPlot visualization | offscreen captures + chart values | NOT_RUN |
| Plotly/Bokeh documents | document values + HTML render checks | NOT_RUN |
| PDF | page/text/table/visual checks | NOT_RUN |
| Plugin ABI | discovery/registry/fingerprint/lifecycle tests | NOT_RUN |
| Reports/tears | HTML/XLSX/PDF fixtures | NOT_RUN |
| 228-file test port | ledger complete; native tests pass | NOT_RUN |
| Oracle differential | route budgets | NOT_RUN |
| Performance | native no-regression | INCONCLUSIVE (macOS dispersion >5%; C4 archive unavailable for a Windows run) |
| Platforms/consumer | macOS/Ubuntu/install | macOS and Windows PASS; Ubuntu `EXTERNAL_BLOCKED` |
| Optional manifest | exact list + owner sign-off | NOT_RUN |
| Reproducibility | manifests, hashes, logs | PASS (Windows logs added; source archive regenerated) |

## 11. Performance and artifact protocol

- Native analytics: no median regression >10% versus the frozen Iteration 7
  baseline beyond measured noise.
- AB/BA fresh processes, predeclared batches, CPU affinity; >5% dispersion is
  INCONCLUSIVE, not PASS.
- Plotly/Bokeh documents: compare data/axes/legend/series to oracle values and
  render the self-contained HTML headlessly; format differences declared.
- PDF: extract text/rows, count pages and inspect rendered page images.
- No Python binding overhead is in scope; any future adapter is separate.

## 12. Sign-off

| Phase | Source snapshot | Manifest | Status | Evidence |
|---|---|---|---|---|
| 100 | | | NOT_RUN | |
| 101 | | | NOT_RUN | |
| 102 | | | NOT_RUN | |
| 103 | | | NOT_RUN | |
| 104 | | | NOT_RUN | |
| 105 | | | NOT_RUN | |
| 106 | | | NOT_RUN | |
| 107 | | | NOT_RUN | |
| 108 | | | NOT_RUN | |
| 109 | | | NOT_RUN | |
| 110 | | | NOT_RUN | |

**Reviewer:** ____________________  **Date:** __________

## 13. Claim boundaries

- This iteration claims **native behavioural and artifact parity** for the
  approved native target using DataFrame/vector results, ImPlot, Plotly/Bokeh-
  compatible documents, native PDF and native reports.
- It does **not** claim pandas/NumPy/Python object identity, Python exception
  inheritance, pytest execution, byte/schema identity with the Python Plotly/
  Bokeh/PDF backends, Python wheels, or Windows/hosted CI without a runner.
- PyMC and live provider SDK/network routes are `EXTERNAL_OPTIONAL`; they are
  not claimed as PASS.
- No threshold is lowered to match a result; all raw failures are retained.
