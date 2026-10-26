# Iteration 5 Close-out — Factor Analysis

## Delivered

| phase | accepted slice | evidence |
| --- | --- | --- |
| 77/78 | Panel preparation, forward returns, quantization, PIT ledger and PIT preparation | [factor core](2026-10-23-iteration-5-factor-core-acceptance.md), [PIT/portfolio](2026-10-23-iteration-5-pit-portfolio-acceptance.md) |
| 79 | Compute-once analysis model (quantile stats, by-date tables, spreads, cumulative/growth curves, positions, monthly IC, portfolio inputs) | [analysis model](2026-10-23-phase-79-analysis-model-acceptance.md) |
| 80 | IC inference, Fama-MacBeth, model inference, event windows and quantile event returns | [inference](2026-10-23-phase-80-inference-acceptance.md), [event windows](2026-10-23-phase-80-event-windows-acceptance.md) |
| 81 | Turnover, rank autocorrelation, stability | [factor core](2026-10-23-iteration-5-factor-core-acceptance.md) |
| 82 | Costs/capacity ledger, overlapping portfolio constructor, business-day holdings, cash/benchmark | [costs](2026-10-23-iteration-5-factor-costs-analysis-acceptance.md), [PIT/portfolio](2026-10-23-iteration-5-pit-portfolio-acceptance.md) |
| 83 | Research evaluation, capabilities, CSV/manifest export, correlations/comparisons, segment provenance, precomputed/next-open labels incl. calendar naming, universe masks, signals/groups, multi-factor assembly | [research/tears](2026-10-23-iteration-5-research-tears-acceptance.md), [export](2026-10-23-phase-83-research-export-acceptance.md), [correlations](2026-10-23-phase-83-research-correlations-acceptance.md), [segments](2026-10-23-phase-83-research-segments-acceptance.md), [precomputed labels](2026-10-23-phase-83-precomputed-labels-acceptance.md), [universe](2026-10-23-phase-83-research-universe-acceptance.md), [next-open](2026-10-23-phase-83-next-open-labels-acceptance.md), [signals/groups](2026-10-23-phase-83-research-signals-acceptance.md), [multi-factor](2026-10-23-phase-83-multifactor-assembly-acceptance.md), [calendar labels](2026-10-23-phase-83-calendar-labels-acceptance.md) |
| 84 | Four-section factor report projection | [factor report](2026-10-23-phase-84-factor-report-acceptance.md) |
| 85/86 | ImPlot tear chart data (including grouped panels); factor HTML through the shared renderer | [research/tears](2026-10-23-iteration-5-research-tears-acceptance.md), [group tears](2026-10-23-phase-85-group-tears-acceptance.md) |
| 87 | Native pipeline benchmark and measurements | [performance study](2026-10-23-phase-87-factor-performance-acceptance.md) |

The parity ledger lists **zero unresolved factor-analysis function gaps**
after the Phase 80 closures; the remaining factor routes are deferred plotting,
calendar utilities, or pandas/plugin plumbing.

## Evidence and gates

- Oracle fixtures at pinned Python `4b8f31b1` with recorded hashes for data,
  performance, analysis, portfolio, research and event windows.
- Full headless CTest **721/721** passed at the final HEAD.
- Full ASan+UBSan suite **734/734 passed** at the final HEAD (Homebrew LLVM
  22.1.8, Ninja Debug).
- HTML artifact visually verified headlessly (four SVG charts, eleven tables);
  ImPlot windows remain **NOT_RUN**.

## Open items

- Group-aware model results and grouped tear chart data are accepted; on-screen
  rendering and the source duplicate aggregate-versus-group snapshots are not
  modelled. The optional event model is accepted.
- Matplotlib tear-sheet functions are deferred in favour of ImPlot chart data;
  grouped chart data is accepted, but visual rendering is unverified.
- Calendar utility functions (`infer_trading_calendar`, timedelta helpers)
  are deferred; business-day handling exists internally.
- Weighted factor combinations and intraday label formatting remain open; all
  other bounded research routes (preparation, calendar/precomputed/next-open
  labels, universe, signals/groups, segments, correlations, comparisons,
  multi-factor assembly and export) are accepted.
