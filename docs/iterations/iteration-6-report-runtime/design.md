# Iteration 6 — Report and Runtime: Design Document

**Iteration ID:** iteration-6-report-runtime
**Status:** Draft
**Depends on:** Iterations 1–5 (full compute surface; Iteration 5 only for
the factor-report boundary determination in Phase 88).

## 1. Architecture Overview

```
include/fincore/report/{document,html,xlsx,portfolio,risk_validation}.hpp
src/report/*.cpp + tests/unit/report_*_test.cpp
include/fincore/runtime/{catalog,plan,session,bundle}.hpp
src/runtime/*.cpp + tests/unit/runtime_*_test.cpp
```

No new dependencies by default; the XLSX decision (D2) is the only opening
for one.

## 2. Key Design Decisions

### D1 — ReportDocument is a frozen value type with typed sections
`report_document` holds: ordered sections; tables as
`{name, column_names, rows<vector<string or double>>}`; series referencing
chart value types from Iteration 4; KPI entries `{label, value, class}`.
Renderers are pure functions `document → bytes`. Compute functions produce
documents; nothing downstream recomputes — the same discipline as
`analysis_context` (Phase 5a) and the factor models (Phase 79).

### D2 — XLSX: minimal OOXML emitter first
Default decision: emit minimal valid OOXML (one sheet per table, inline
strings, no styling) with a hand-written emitter — no vendored dependency.
The Phase 90 design must demonstrate the output opens in Excel/LibreOffice
and carries correct values; if fidelity to openpyxl features (styles,
column widths) turns out to be required by the oracle tests, the design doc
amends this decision with a vendoring plan. Rationale: styling parity is not
a Python-oracle functional requirement; values and structure are.

### D3 — Runtime: catalog of function refs, plan as data
`OperationSpec` declares name + input/output shape metadata;
`builtin_catalog()` registers the ported compute functions as a
`std::function`-free table of plain function pointers where signatures
match, or thin adapters where they don't. `plan()` validates a request DAG
against the catalog (unknown operation → `input_error`), `run()` executes in
topological order with explicit per-operation error propagation, `batch()`
runs multiple plans sharing one session snapshot. `AnalysisSession` owns
frames by value; `AnalysisSnapshot` is an immutable deep view;
`ArtifactBundle` collects document/chart bytes.

Mirrors the Python semantics deliberately: the C++ implementation should be
recognizable to a reader of `fincore/runtime/`.

### D4 — HTML renderer reuses the Phase 86 serializer patterns
Escaping rules, table emission, and deterministic ordering reuse the
patterns proven by the factor HTML report (Phase 86): static label
dictionaries, sorted/ordered iteration, no locale dependence, no timestamps
unless the oracle emits them.

## 3. Component Plan

As in requirements §3 (phases ↔ files 1:1). Phase 91 composes existing
compute leaves into builders; Phase 92 wires the catalog over the full
ported surface.

## 4. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Report surface drift (Python report layer is active) | Enumerate surfaces at Phase 88 design; drift policy applies; re-pin checkpoint at iteration start. |
| OOXML emitter correctness | Phase 90 validates with a real spreadsheet application opening the file + value-by-value structural comparison vs openpyxl output. |
| Runtime scope creep (backend abstraction) | D3 forbids a backend abstraction layer; the native path is the only path. Deviations need a design amendment. |
| HTML byte-determinism broken by unordered containers | Deterministic serializer rules from Phase 86 applied; byte-determinism test in every renderer phase. |

## 5. Sequencing

88 → 89 → 90 ordered (document → HTML → XLSX). 91 after 89 (HTML rendering
of portfolio/risk reports) and Iteration 3 (risk models). 92 last, after the
full compute surface is frozen; its catalog registration is the natural
final audit of the whole port.
