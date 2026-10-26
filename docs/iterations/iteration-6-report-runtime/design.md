# Iteration 6 — Report, Runtime and Native Delivery: Design

**Status:** Revised plan. [Shared contract](../delivery-contract.md) applies.

## 1. Early shared schema without a dependency cycle

Phase 88a establishes `include/fincore/report/document.hpp` and the minimal
shared artifact/result boundary needed by report consumers. Use owning
primitive section/table/series payloads and metadata independent of GUI or
solver types. Reference neutral chart semantics through values; complete
chart builders can land later in 72–75. Assign shared-header ownership to
88a and require consumer phases to use it. Schema changes need affected
consumer tests rather than a parallel local model.

Phase 88b reconciles full pinned fields and domain adapters after 67/84.
The source includes metrics/tables/series, units, legends, notes, document
metadata and offline assets; a string-or-double table alone is insufficient.
Document and artifact types precede HTML/XLSX and factor rendering; full
session/catalog implementation remains in 92.

## 2. Rendering and output design

Shared serialization handles deterministic ordering, locale-independent
numbers, missing/nonfinite values, Unicode and escaping in each context.
Do not insert unescaped JSON strings inside HTML scripts. Keep ECharts
assets/version/notices with the package and verify an offline browser run.
Renderers consume computed models; instrument a second render to prove it
performs no analysis. Searching the diff for numeric literals is not proof.

Phase 90 compares a maintained XLSX writer with a narrowly scoped emitter
only if the latter covers ZIP/container, relationships, content types, sheet
names, cell types and deterministic metadata reliably. Decide using measured
fidelity, licensing, dependency/build cost and maintenance. Match one sheet
per section with stacked metrics/tables/series, not a sheet per table.
Validate by independent reopen and cell-by-cell comparison, plus a real
spreadsheet application's opening check. Freeze sheet-name truncation/
collision handling and formula-like text behavior from source or a recorded
native adaptation. Normalize ZIP order/timestamps when promising identical
native bytes; otherwise explicitly limit the promise to canonical content.

Explicit writers validate the output root and bundle paths, refuse/handle
overwrite according to the mapped operation and report partial failure.
Pure render-to-bytes has no filesystem side effects. Shared artifact
contracts describe actual bytes/media types/paths and provenance, not a
placeholder assumed to be filled by future runtime work.

## 3. Runtime follows the source execution model

Inventory `runtime/__init__.py` and catalog/spec/snapshot/engine modules at
the pinned object. Define operation IDs, accepted argument shapes, result
schemas, configuration/provenance and error mapping before registering
callables. Existing utilities need native mappings but are not automatically
catalog operations; reconcile the Python operation catalog separately.

`ExecutionPlan` stores immutable validated requests in input order plus
catalog identity. `batch()` detects a changed catalog, runs the ordered
requests with the specified failure behavior and produces the corresponding
results. `run()` invokes one registered function; the native adapter must
not forward metadata/config that Python does not pass to that function.
Unknown operation, malformed snapshot, configuration and validation failures
follow the oracle's order. No DAG or arbitrary backend framework is added.

Snapshots retain owned/immutable inputs and stable provenance. Test original
input mutation, returned value mutation, stale-catalog detection and repeated
execution. If native implementation fingerprints differ from Python hashes,
version their identity scheme explicitly; do not fake byte equality.

## 4. Distribution architecture and sequence

93 adds install/exported targets and CMake config/version files without
source-checkout absolute paths. Account for public DataFrame dependencies,
C++23/compiler ABI constraints, optional ImPlot target/assets and license
notices. A headless consumer needs no ImGui/GLFW. Test relocation in a clean
consumer using `find_package(fincore CONFIG REQUIRED)` after source/build
roots are unavailable. Configure test dependencies explicitly for offline
runs; current GoogleTest FetchContent is not inherently offline.

39 freezes platform/compiler targets and baseline protocol; build/packaging
proofs may start early to expose dependency problems, with 93 final evidence
at the candidate. Default proposed targets are macOS arm64 and Linux x86_64,
subject to an explicit supported-matrix decision and actual tests. Add CI
for that declared matrix and record blocked/skipped jobs accurately.

Sequence: 88a after 39; 67/84/86 consume it. 88b after domain projections;
89/90 can proceed independently once schema/serializers are stable; 91 needs
its compute domains and renderer; 92 integrates the selected catalog; 93
closes installed-consumer/platform/performance evidence and migration notes.
