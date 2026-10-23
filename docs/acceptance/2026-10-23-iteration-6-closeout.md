# Iteration 6 Close-out — Report and Runtime

## Delivered

| phase | accepted slice | evidence |
| --- | --- | --- |
| 88a | Shared `report_document`/section/table/series/metric schema and deterministic JSON | [report schema](2026-10-23-phase-88a-report-schema-acceptance.md) |
| 88b | Factor, portfolio and risk projections onto the shared schema (partial field completion) | [factor report](2026-10-23-phase-84-factor-report-acceptance.md), [portfolio](2026-10-23-phase-91-portfolio-report-acceptance.md), [risk](2026-10-23-phase-67-risk-report-acceptance.md) |
| 89 | Offline HTML renderer (escaped semantic HTML, inline SVG, en/zh, deterministic bytes) | [HTML/risk/runtime](2026-10-23-iteration-6-html-risk-runtime-acceptance.md) |
| 90 | Dependency-free XLSX writer (stored ZIP/OOXML, one sheet per section) | [XLSX/packaging](2026-10-23-iteration-6-xlsx-packaging-acceptance.md) |
| 91 | Portfolio performance/benchmark series, drawdowns, rolling beta, exposure and typed transaction sections | [portfolio completion](2026-10-23-phase-91-portfolio-report-completion-acceptance.md), [transactions](2026-10-23-phase-91-transactions-acceptance.md) |
| 92 | Runtime catalog/spec/request/plan/batch/session/snapshot/artifact boundary; 20 selected operations under source IDs | [runtime catalog](2026-10-23-phase-92-runtime-catalog-acceptance.md) |
| 93 | Install/export component, downstream consumer and native usage/migration notes | [usage/packaging](2026-10-23-phase-93-usage-packaging-acceptance.md) |

The program parity ledger covers 351 public Python functions: 238 exact-name
native declarations, 36 verified renames, 51 deferred, 21 non-port and three
unresolved (one pandas-stat-function round-trip aggregation, two attribution
helpers).
The runtime catalog registers 20 of 273 source operations with explicit
domain classification. See [the program parity audit](2026-10-23-program-parity-audit.md)
and [ledger](2026-10-23-program-parity-ledger.tsv).

## Evidence and gates

- Fresh component install at this revision and a downstream consumer printing
  `version=0.1.0 sharpe=6.928203230275507`.
- Full headless CTest **718/718** passed at the final HEAD.
- Sanitizer **NOT_RUN** for Iterations 4–6 (last complete 661/661 at
  Iteration 3 close).
- Supported-platform CI matrix **NOT_RUN**; only macOS arm64 is evidenced.
- Visual rendering **NOT_RUN**; HTML is structurally tested, not
  pixel-reviewed, and ECharts is replaced by inline SVG.

## Open items

- 88b remains a partial integration: event/group factor fields and some
  optional domain fields are absent from the shared document.
- Runtime operations for factor/portfolio/performance/simulation/
  attribution/report domains need typed input contracts; data utilities are
  explicitly excluded.
- Three unresolved capability gaps and the deferred/non-port ledger rows.
- Transaction turnover (beyond the accepted count/notional series), automated
  benchmark regression gating and portable CI.
