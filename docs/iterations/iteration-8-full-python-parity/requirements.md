# Iteration 8 — Native Full Parity (DataFrame + ImPlot): Requirements

**Revision:** R3, native-core scope covering visualization backends, plugins,
native error/container behaviour and the Python test corpus.
**Phases:** 100–110. **Status:** Planned, **NOT_RUN**.
**Oracle:** Python fincore `3e9ef205ef2023f92e89fddd1052d46b16429c64`
(`0.5.2.dev0`). **Native base:** C4, source manifest
`245ae75336912e30bcf6f34016ed4451116ec4ed719771a02d4b6e46ab0516e3`.
The [shared delivery contract](../delivery-contract.md) applies.

## 1. Context and scope decision

The owner directed a native-core approach: DataFrame/vector types carry
pandas/NumPy-equivalent semantics, ImPlot and native report writers carry
plotting, and Python-only object identity is not the target. This revision
also brings the remaining native-equivalent surfaces into Iteration 8:

- Plotly/Bokeh/PDF routes are implemented as native chart/document/PDF
  artifacts with behaviour/visual equivalence;
- plugin discovery/registration is implemented with a native plugin ABI,
  manifests and registry;
- Python exception/container behaviour is implemented as native structured
  errors and DataFrame/vector containers;
- the 228 Python test files are **ported** into native C++ tests with a
  file-to-test ledger, rather than run through pytest.

Only PyMC posterior APIs and live provider network SDKs remain external
optional integrations; they are named, not silently dropped.

"Parity" in this iteration means **native behavioural and artifact parity**:
the same numeric results, shapes, ordering, missingness, validation/error
semantics and equivalent chart/report artifacts. It does not mean Python class
identity, pandas/NumPy object types, or executing the Python test files.

The current route ledger has 952 non-module routes: **508 PASS, 10 PARTIAL,
286 DEFERRED, 148 former NON_PORT**; the runtime inventory has 273 source
operations. P100 classifies every row into one of:

- `NATIVE_TARGET` — computational/domain/artifact behaviour implemented on the
  native core or with a native equivalent (including Plotly/Bokeh/PDF
  artifacts, plugin ABI, structured errors and DataFrame/vector containers);
- `EXTERNAL_OPTIONAL` — PyMC posterior APIs and live provider SDK/network
  routes that require an external runtime/service. The native offline/data
  model path must still be implemented and tested;
- `EXTERNAL_BLOCKED` — a platform runner or credential needed by a gate; the
  native/offline behaviour is still implemented.

No row may remain `PLANNED`, `DEFERRED`, `NON_PORT`, `PARTIAL` or unexplained
`GAP`.

## 2. Definition of native parity

For a `NATIVE_TARGET` route, parity requires:

1. a native API (DataFrame/vector/owning model or native artifact writer)
   exposes the capability;
2. numeric outputs match the Python oracle on shared fixtures under declared
   exact/tolerance budgets;
3. index/row/column ordering, missingness, dtype class (double/int/bool/string)
   and validation/error predicates match;
4. the route has a C++ test or oracle fixture and an acceptance row;
5. chart/report routes produce equivalent artifacts: native chart data plus
   ImPlot, Plotly-compatible/Bokeh-compatible chart documents, HTML/ECharts,
   or native PDF, with declared format/layout differences;
6. plugin routes expose equivalent registration/discovery/fingerprint/
   lifecycle behaviour through the native plugin ABI;
7. error routes expose native error categories, codes, report maps and
   aliases with source-compatible predicates/messages.

Python object identity, pandas/NumPy classes and pytest execution are
explicit non-claims, not route exclusions.

## 3. DataFrame, numeric and error representation

- pandas DataFrame ↔ `fincore::dataframe` (`hmdf::StdDataFrame<std::int64_t>`).
- pandas Series / NumPy 1-D ↔ owning `std::vector<double>` or spans.
- NumPy 2-D ↔ row-major dense matrices with explicit rows/columns.
- MultiIndex ↔ index plus asset/group key columns or keyed rows.
- Object/string columns ↔ `std::string` columns.
- UTC timestamps remain integer nanoseconds; timezone objects are not native.
- Missing values: NaN for doubles; explicit absent/mask for other columns.
- Native errors carry category, code, message and optional report map; aliases
  mirror the source predicates. Python exception inheritance is not claimed.
- Returned objects own data; caller mutation cannot corrupt caches.

The needed group/quantile/rolling/alignment operations are native kernels; this
is not a claim of arbitrary pandas API compatibility.

## 4. External/optional integrations

Only these remain outside Iteration 8's native target:

- **PyMC/posterior APIs** — the native Bayesian cone remains selected; PyMC
  model/posterior objects require the Python runtime.
- **Live provider SDK/network routes** — native provider data models and
  offline validation are in scope; live Tushare/AkShare/Yahoo/AlphaVantage/
  Fama-French/style fetching requires credentials and third-party SDKs.

They are listed in the P100 manifest with a named prerequisite and are not
claimed as PASS. No other route is deferred.

## 5. Phase requirements

| Phase | Deliverable | Exit criteria |
|---|---|---|
| 100 | Baseline, route split and native parity harness | C4/Python pin frozen; 952 routes and 273 operations classified; optional list exact; oracle-fixture generator and route validator ready |
| 101 | DataFrame/native value and error semantics | Index/column/order/missingness/nonfinite/time semantics; structured native errors/codes/reports and aliases; DataFrame/vector conversion tests |
| 102 | Metrics/portfolio/performance native closure | Ratios/drawdown/consecutive/rolling/stats/yearly/alpha-beta/timing, positions/transactions/round-trips/capacity, returns/cashflows/inference/disclosures |
| 103 | Risk/optimization/simulation/attribution closure | GARCH/EVT/backtesting/calibration/models/report/walk-forward, optimizer objectives/frontier/risk parity, simulation paths/bootstrap/scenarios/Monte Carlo, Brinson/Fama-French/style/performance |
| 104 | Factor-analysis full native surface | Data/calendar/PIT/performance/inference/analysis/models/tears/portfolio/costs/research/report; 10 PARTIAL classes member-by-member; specialized chart data |
| 105 | Runtime catalog and data models | Native-compatible operations of 273 registered with typed inputs; DataSnapshot/provider models and offline validation; optional live routes named |
| 106 | Native visualization backends | ImPlot panels for all chart routes; Plotly-compatible and Bokeh-compatible chart documents; native PDF for the selected report route; offscreen/artifact verification |
| 107 | Plugins, extensions and serialization | Native plugin ABI, manifest/discovery, registry, fingerprints and lifecycle; extension snapshots; native serialization of selected models |
| 108 | Reports, tears and test port | Factor/risk/portfolio reports and grouped tears; 228-file Python test-port ledger with native tests or declared Python-identity variances |
| 109 | Native parity campaign | Route ledger has only NATIVE_TARGET PASS or named EXTERNAL_OPTIONAL; oracle/artifact/visual/performance evidence; 228 files accounted |
| 110 | Packaging, CI and closeout | Install/consumer on macOS arm64 and Ubuntu arm64/x86_64; sanitizers; Windows when runner; acceptance record and optional manifest signed |

Phases 102–105 share the route inventory and may proceed in parallel with one
editor per shared file. Phase 106 depends on chart data from 104/105. Phase 108
depends on the native models/renderers. Phase 109 is the only close-out.

## 6. Functional requirements

### FR-1 — Frozen identities and route split

Freeze C4, the Python pin, the 952-route ledger, the 273-operation inventory
and the oracle environment. P100 produces exact native/optional lists with
counts and IDs. No route class changes after results without a dated owner
amendment.

### FR-2 — DataFrame-native semantics

Every native route uses owning DataFrame/vector results. Tests cover index
order, column order, missingness (absent vs NaN), nonfinite values, dtype class
and caller-mutation isolation. No pandas/NumPy object identity is claimed.

### FR-3 — Native route closure

All `DEFERRED`/`PARTIAL` routes not in the optional list are implemented
through canonical native kernels/models or native equivalent artifacts.
Aliases and historical facades map to canonical capabilities with behavioural
fixtures; a name match alone is not evidence. Partial classes are accepted
member-by-member.

### FR-4 — Runtime catalog

Every native-compatible source operation among the 273 is registered with a
typed input contract, source ID, deterministic digest and test. Optional
provider/live operations are named, never registered as placeholders.

### FR-5 — Provider data models

Native request/response/data-snapshot models and offline validation are in
scope. Live SDK/network operations are `EXTERNAL_OPTIONAL` with named
credentials/services; offline fixtures cover the native models.

### FR-6 — Native visualization backends

Every selected chart route exposes native chart data and an ImPlot panel.
Plotly/Bokeh routes additionally produce native chart-document artifacts with
equivalent data, axes, legends and series; the exact third-party JSON schema is
not claimed unless separately recorded. The PDF route uses a native PDF writer
with equivalent text/tables/vector content; byte identity with the Python
backend is not claimed. Offscreen captures are inspected.

### FR-7 — Plugins and extensions

A native plugin ABI supports explicit registration, manifest-based discovery,
registry ordering, fingerprints and lifecycle. Python `entry_points` semantics
are replaced by the native manifest contract; behaviour (discovery records,
load errors, fingerprints, ordering) is tested with sample plugins.

### FR-8 — Reports and tears

The shared report schema and native HTML/ECharts/XLSX/PDF writers cover factor,
risk and portfolio reports, including grouped tear sections and specialized
panels. Renderers consume prepared models; no kernel recomputation.

### FR-9 — Python test port

Every one of the 228 pinned Python test files is mapped in a ledger to one or
more native tests/fixtures, or to an explicitly declared `PYTHON_IDENTITY_`
variance for assertions that only exist because of Python object identity or
monkeypatching. No file is unaccounted. The native tests are executed; pytest
is not run because no Python binding is introduced.

### FR-10 — Numeric and oracle parity

Each native route has a fixture from the pinned Python source or a shared
contract fixture. Integer counts/order/shapes are exact; floating results use
route-specific exact/tolerance budgets fixed before implementation.
Differential fuzzing covers empty, nonfinite, duplicate, boundary and
unbalanced data.

### FR-11 — Performance

Native analytics retain Iteration 7's accepted performance. No native route may
regress more than 10% median beyond noise without a recorded disposition. The
Iteration 7 harness/protocol applies; no Python binding overhead is in scope.

### FR-12 — Platforms and packaging

Native release and sanitizer gates on macOS arm64; release on Ubuntu arm64 and
x86_64 (native container acceptable with declared mode); installed headless
consumer. Windows is `EXTERNAL_BLOCKED` with a named runner unless executed. No
Python wheel is required.

### FR-13 — Optional manifest and no false parity claim

The optional manifest names every `EXTERNAL_OPTIONAL` route (PyMC, live
provider SDKs), its prerequisite and its offline native model. Iteration 8
acceptance must state that Python object identity and pytest execution are not
claimed.

### FR-14 — Reproducibility

Record source manifests, oracle fixtures, test-port ledger, artifact captures,
benchmark samples, dependency pins and raw logs. The final oracle pin is
immutable; later upstream commits need a change review.

## 7. Correctness and status rules

- Native route status: `NATIVE_PASS` or named `EXTERNAL_OPTIONAL`/`EXTERNAL_`
  `BLOCKED` for the optional list.
- An optional route is not a PASS; it is an owner-approved external
  integration with a named prerequisite.
- `EXTERNAL_BLOCKED` is allowed only for live credentials/services or platform
  runners; the native/offline behavior must pass.
- No test may be silently skipped; visual claims require an inspected offscreen
  capture; artifact claims require a checked document/PDF.
- No route may be reclassified after results without a dated amendment.

## 8. Exit criteria

- Exact native/optional split approved by the owner.
- Every `NATIVE_TARGET` route has `NATIVE_PASS` with fixture/test/artifact
  evidence.
- The 10 PARTIAL classes have member-level dispositions and native tests.
- ImPlot, Plotly/Bokeh-document and PDF artifacts are inspected/verified.
- Plugin ABI, registry, fingerprint and lifecycle tests pass.
- Every one of the 228 Python test files is accounted for in the test-port
  ledger; native tests pass.
- Native Release/ASan/Ubuntu/consumer/performance gates pass as declared.
- Optional manifest is complete, named and signed.
- Acceptance record states native behavioral/artifact parity and the explicit
  non-claim for Python object identity/pytest.

## 9. Non-goals and claim boundaries

- No pandas/NumPy/Python object identity, Python exception inheritance or
  pytest execution.
- No claim that Plotly/Bokeh/PDF artifacts are byte/schema-identical to the
  Python backends; equivalence is data/visual/behavioural unless separately
  recorded.
- Live PyMC/provider integrations are optional external routes, not PASS.
- No Windows or hosted-CI claim without a runner.
