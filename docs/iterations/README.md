# Iteration Roadmap — Document Index

The post-Phase-38 port is organized into **six delivery iterations**, each
with its own folder containing three documents:

- `requirements.md` — requirements specification (scope, API inventory
  against the Python oracle, standing constraints, exit criteria,
  non-goals/non-ports)
- `design.md` — iteration-level design (architecture, key decisions,
  component plan, risks, sequencing); superseded per phase by the individual
  `docs/plans/<date>-phase-N-*.md` and
  `docs/design/<date>-phase-N-*-design.md` documents at implementation time
- `acceptance.md` — acceptance document (standard per-phase gate, iteration-
  specific items, definition of done, sign-off record to be filled as phases
  complete)

| Iteration | Folder | Phases | Domain |
|---|---|---|---|
| 1 | `iteration-1-metrics-completion/` | 38 (close-out), 39 (re-pin), 40–52 | metrics kernel completion |
| 2 | `iteration-2-performance-portfolio/` | 53–59 | GIPS performance + portfolio capacity/liquidity |
| 3 | `iteration-3-risk-optimization/` | 60–71 | solver spike, GARCH/EVT/backtests, frontier/objectives/risk parity |
| 4 | `iteration-4-visualization/` | 72–76 | ImPlot adapter completion (non-factor VizBackend surface) |
| 5 | `iteration-5-factor-analysis/` | 77–87 | factor analysis + tear sheets + bilingual ECharts report |
| 6 | `iteration-6-report-runtime/` | 88–92 | ReportDocument + renderers + runtime orchestration |

Top-level roadmap (sequencing, parity policy, risk register):
[`docs/plans/2026-10-23-fincore-cpp-iteration-roadmap.md`](../plans/2026-10-23-fincore-cpp-iteration-roadmap.md)

## Amendment 2026-10-23 — Reorganization into six delivery iterations

The roadmap originally numbered eight iterations (0–7). They are reorganized
into six delivery iterations: the former Iteration 0 (close-out and baseline
re-pin) is folded into Iteration 1 as its preparatory phases (Phase 38
close-out, Phase 39 re-pin) plus entry conditions; the former Iterations 3
(risk) and 4 (optimization) are merged into Iteration 3 because they share
the Phase 60 solver decision and the same tolerance-parity regime. Phase
numbering from the original roadmap is unchanged. Each iteration carries its
own requirements/design/acceptance documents in this directory; the
per-phase SDD loop (plan → design → feat → test-harden → accept → trace) is
unchanged.
