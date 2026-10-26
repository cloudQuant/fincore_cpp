# Iteration 6 — Report, Runtime and Native Delivery: Requirements

**Phases:** 88a early contract, 88b completion, 89–93.
**Status:** Revised plan; NOT_RUN.
**Oracle:** Python `4b8f31b1b59707480d5db9603704d83bbf7e6946`.
The [shared delivery contract](../delivery-contract.md) applies.

## 1. Scope and entry

Deliver shared report/artifact models, report renderers/builders, native
runtime orchestration and an installable native package. Start the narrow
88a schema after Phase 39 before its consumers (67/84/86); do not wait for
all compute domains. Later report/runtime slices require only their named
accepted operations, with final reconciliation over the complete selected
scope. Phase 93 gates release readiness without publishing anything.

## 2. Functional scope

| Phase | Source / delivery | Requirements |
|---|---|---|
| 88a | `report/models.py`, runtime artifact/result contracts | Minimal common `ReportDocument`/`ReportSection`, typed tables/series/metrics, metadata, units, legends/notes, offline assets and `ArtifactBundle` boundary. Owning immutable data, versioned schemas and serializer contracts before consumers. Domain calculation and full runtime are not required here. |
| 88b | Shared model completion/integration | Complete fields and integration mapping across factor, portfolio and risk projections; preserve the 88a schema or explicitly migrate consumers. No second report framework. |
| 89 | `report/renderers/html.py` | Structural parity in sections/tables/values and safe escaping, deterministic native bytes under declared metadata; use common asset/artifact handling. |
| 90 | `report/renderers/xlsx.py` | One sheet per ReportSection, stacking metrics/tables/series as the pinned writer does; mapped names, order, labels and cell types. Choose writer/ZIP dependency by evidence, not an assumed hand-written OOXML default. |
| 91 | `report/portfolio/compute.py`, `report/risk.py` | Portfolio builder and projection/rendering of Phase 67 risk validation results. Sections, missing optional inputs and builder failures from the ledger; no render-time recalculation. |
| 92 | `runtime/` | Catalog/spec/request, `ExecutionPlan`, `plan/run/batch`, session/snapshot/result/bundle and validation helpers. Register the source catalog's selected native equivalents; classify utilities and unported operations explicitly. |
| 93 | Native package and consumer validation | Install/export/config/version rules, optional GUI separation, dependency pins/notices/assets, supported-platform CI, relocatable downstream consumer, benchmark regression and migration/usage notes. |

The pinned Python `plan()` stores a validated **ordered request sequence**,
not a dependency DAG. `batch()` checks the catalog digest and replays order;
`run()` invokes the registered callable using the request snapshot. Preserve
input/config/provenance/error semantics, including config that is recorded
but not forwarded into the domain function. Do not invent graph scheduling,
cycle rejection or concurrency guarantees as parity requirements.

## 3. Ownership, serialization and artifacts

Own returned models/snapshots or retain immutable backing ownership. Freeze
snapshot isolation, copying and cache invalidation. Model schemas distinguish
missing/null, finite values, NaN/±inf, strings, booleans, dates and labels;
HTML/JSON/XLSX conversions have explicit policies, never silent numeric loss.
Version canonical bytes and digest domain. Runtime catalog fingerprints are
language-specific implementation identities unless a shared cross-language
schema is intentionally established; they are not output-value hashes.

Separate pure compute/render-to-bytes from explicit artifact writing.
Specify output root, path normalization, overwrite policy, failure cleanup
and bundle metadata. Test untrusted label text in HTML/script contexts;
XLSX and CSV text must not accidentally become formulas. Byte determinism
is a native repeatability promise under normalized metadata, not byte equality
with Python ZIP/HTML files. Structural/cell parity remains independently tested.

## 4. Carried-forward scope dispositions

| Capability | Disposition for this native plan | Required mapping |
|---|---|---|
| PyMC-dependent Bayesian APIs | NON_PORT | Keep accepted Bayesian cone; do not exclude that native capability with the rest of the module |
| Network provider SDK adapters | NON_PORT | File/in-memory inputs still supported; any consumed snapshot/digest model is mapped separately |
| Plotly/Bokeh-specific backends | NON_PORT | Selected visual capabilities mapped to ImPlot/HTML with explicit fidelity differences |
| Browser-driven PDF renderer | NON_PORT | HTML delivery does not imply PDF feature parity |
| Python entry-point discovery | NON_PORT | Native explicit catalog registration only |
| NumPyBackend implementation | Native equivalent | Native execution is the backend; map supported dispatch and validators, do not discard their observable semantics |
| Python bindings/wheels | Outside current native delivery target | No drop-in Python-package claim; separate scope decision required for a binding iteration |

Do not add factor inference/calendar, custom renderer or other discovered
public capabilities to this table by omission. The master ledger records
individual mappings/deferrals and their effect on completion.

## 5. Exit

88a–93 accepted for the frozen selected scope; cross-domain integration,
report/runtime behavior, native installed consumer and supported platform
matrix demonstrated; all exclusions and remaining gaps explicit. Full native
port closure is distinct from Python package replacement or actual release.
