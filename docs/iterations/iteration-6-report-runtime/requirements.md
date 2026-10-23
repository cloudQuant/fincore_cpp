# Iteration 6 — Report and Runtime: Requirements Specification

**Iteration ID:** iteration-6-report-runtime
**Phase range:** Phases 88–92
**Source of truth:** fincore Python `dev@4b8f31b`: `fincore/report/`, `fincore/runtime/`
**Status:** Draft

> Roadmap amendment 2026-10-23: former Iteration 7 (report + runtime)
> renumbered to 6 after the risk/optimization merge.

## 1. Purpose and Scope

Port the "compute once, render many" spine: the `ReportDocument` compute-once
model and its renderers (HTML, XLSX; matplotlib/plotly equivalents are
covered by the ImPlot adapter), the portfolio and risk validation report
builders that close the loop from Iterations 1–5, and the runtime
orchestration core (`OperationCatalog`, `OperationSpec`, `plan/run/batch`,
`AnalysisSession`).

This iteration also formalizes the explicit non-port list (§6) as a
requirements decision, revisitable only by dated roadmap amendment.

## 2. Entry Conditions

- Iterations 1–3 accepted (metrics, performance/portfolio, risk/optimization)
  — the portfolio report needs the full compute surface.
- Iteration 5 accepted if factor report rendering is in scope for a phase;
  otherwise factor reports are excluded (see 3.2).

## 3. Functional Requirements

### 3.1 Report phases

| Phase | Public API | Python source | Requirement summary |
|---|---|---|---|
| 88 | `ReportDocument` compute-once model | `fincore/report/` (document core) | Frozen document value type: sections, tables (name → column/row data), series (name → points), KPI entries; compute-once semantics (renderers never recompute). Consumes chart value types from Iteration 4. |
| 89 | HTML renderer | `report/renderers/html.py` | Escaped HTML tables + document assembly; byte-deterministic output; matches the oracle's table structure and escaping rules exactly. |
| 90 | XLSX renderer | `report/renderers/xlsx.py` | Minimal XLSX writer producing sheets per table. Design-time decision: vendored writer library vs. minimal OOXML emitter (see design D2). |
| 91 | portfolio + risk validation report builders | `report/portfolio/compute.py`, risk validation report | `build_portfolio_report`-equivalent (returns, positions, rolling window inputs) and the risk validation report rendering that closes Phase 67's deferred item. |
| 92 | runtime core | `fincore/runtime/` | `OperationCatalog`, `builtin_catalog`, `OperationSpec`/`OperationRequest`, `plan()`, `run()`, `batch()`, `AnalysisSession`, `AnalysisSnapshot`, `Result`, `ArtifactBundle`. The native compute path *is* the backend — no NumPyBackend analogue. |

### 3.2 Scope boundary on factor reports
The bilingual factor HTML report is already delivered by Iteration 5
(Phase 86). Iteration 6 renders only if the oracle's portfolio/risk report
surfaces require shared renderer infrastructure that the factor report
predates; that determination is made in Phase 88's design and recorded.

### 3.3 Cross-cutting requirements

- **Renderer parity basis:** HTML/XLSX output is validated structurally
  (same tables, sections, values within parity policy) plus byte-determinism
  (two runs → identical bytes). Byte-parity with the Python renderer is not
  required where formatting differs (fonts, CSS), but table structure and
  cell values must match on the frozen fixture.
- **Runtime semantics:** `plan`/`run`/`batch` mirror the Python operation
  catalog semantics — deterministic ordering, explicit error propagation per
  operation, snapshot immutability. The C++ catalog registers exactly the
  ported operations; unported operations are absent, not stubbed.
- `AnalysisSession` owns frames by value (the established ownership rule);
  no shared mutable state.

## 4. Non-Functional Requirements

- Standard SDD loop and gates; oracle runs against a clean detached checkout.
- Byte-determinism tests are mandatory for both renderers.
- No new heavyweight dependency without a design-time decision recorded in
  the phase design doc (XLSX is the only candidate).

## 5. Exit Criteria

- Phases 88–92 accepted.
- `report` and `runtime` public surfaces at parity for the ported subset;
  audit tables in the final acceptance record.
- The non-port list §6 remains accurate or is amended by dated entry.

## 6. Explicit Non-Ports (requirements decision, 2026-10-23)

| Item | Reason | Reversible by |
|---|---|---|
| `metrics/bayesian.py` (PyMC) | No C++ equivalent; Bayesian return cone (Phase 28) is the extent of Bayesian support. | Dated roadmap amendment |
| `data/providers.py` network SDKs (yfinance/tushare/akshare/AlphaVantage) | Heavy SDK/network surface, low C++ value. `DataSnapshot` content-addressing *model* may be ported if a phase needs it. | Dated roadmap amendment |
| `viz` plotly/bokeh backends | ImPlot is the interactive target; HTML backend covered by Phase 89. | Dated roadmap amendment |
| PDF renderer (Playwright/Chromium) | External browser dependency; HTML output is the portable equivalent. | Dated roadmap amendment |
| `extensions/` entry-point discovery | Python packaging mechanism. | Dated roadmap amendment |
