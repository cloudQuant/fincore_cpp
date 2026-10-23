# Iteration 5 Close-out — Factor Analysis

## Delivered

| phase | accepted slice | evidence |
| --- | --- | --- |
| 77/78 | Panel preparation, forward returns, quantization, PIT ledger and PIT preparation | [factor core](2026-10-23-iteration-5-factor-core-acceptance.md), [PIT/portfolio](2026-10-23-iteration-5-pit-portfolio-acceptance.md) |
| 79 | Compute-once analysis model (quantile stats, by-date tables, spreads, cumulative/growth curves, positions, monthly IC, portfolio inputs) | [analysis model](2026-10-23-phase-79-analysis-model-acceptance.md) |
| 80 | IC inference, Fama-MacBeth, model inference, event windows and quantile event returns | [inference](2026-10-23-phase-80-inference-acceptance.md), [event windows](2026-10-23-phase-80-event-windows-acceptance.md) |
| 81 | Turnover, rank autocorrelation, stability | [factor core](2026-10-23-iteration-5-factor-core-acceptance.md) |
| 82 | Costs/capacity ledger, overlapping portfolio constructor, business-day holdings, cash/benchmark | [costs](2026-10-23-iteration-5-factor-costs-analysis-acceptance.md), [PIT/portfolio](2026-10-23-iteration-5-pit-portfolio-acceptance.md) |
| 83 | Research evaluation, capabilities, CSV/manifest export | [research/tears](2026-10-23-iteration-5-research-tears-acceptance.md), [export](2026-10-23-phase-83-research-export-acceptance.md) |
| 84 | Four-section factor report projection | [factor report](2026-10-23-phase-84-factor-report-acceptance.md) |
| 85/86 | ImPlot tear chart data; factor HTML through the shared renderer | [research/tears](2026-10-23-iteration-5-research-tears-acceptance.md) |
| 87 | Native pipeline benchmark and measurements | [performance study](2026-10-23-phase-87-factor-performance-acceptance.md) |

The parity ledger lists **zero unresolved factor-analysis function gaps**
after the Phase 80 closures; the remaining factor routes are deferred plotting,
calendar utilities, or pandas/plugin plumbing.

## Evidence and gates

- Oracle fixtures at pinned Python `4b8f31b1` with recorded hashes for data,
  performance, analysis, portfolio, research and event windows.
- Full headless CTest **721/721** passed at the final HEAD.
- Sanitizer **NOT_RUN** for every Iteration 5 slice (last complete run
  661/661 at Iteration 3 close).
- Visual **NOT_RUN** (tear-sheet plotting deferred; HTML is structurally
  accepted but not pixel-reviewed).

## Open items

- Group-aware factor analysis (`by_group`, group-neutral aggregate snapshots)
  and event/group tear models are not modelled.
- Matplotlib tear-sheet functions are deferred in favour of ImPlot chart data;
  visual rendering is unverified.
- Calendar utility functions (`infer_trading_calendar`, timedelta helpers)
  are deferred; business-day handling exists internally.
- Research grid/next-open bases, universe/group inputs, paired
  comparisons/segments and factor correlations are not part of the bounded
  evaluator.
