# Program-Level Parity Audit — Phase 92 Close

**Pin:** Python `4b8f31b1b59707480d5db9603704d83bbf7e6946`
**Machine ledger:** [2026-10-23-program-parity-ledger.tsv](2026-10-23-program-parity-ledger.tsv)
**Generator:** `tools/parity_ledger.py`
**Native tests:** 718/718 Release CTest

## Method

1. Every non-private top-level function in the pinned `fincore/<package>/*.py`
   modules was extracted with an AST walk (classes, methods and operation
   specs are counted separately below).
2. Every native public header under `include/fincore/**/*.hpp` was scanned for
   a declaration of each name (`\bname\s*\(`). A name found anywhere counts as
   an exact-name native route.
3. Names without an exact declaration were assigned in this order: a verified
   rename whose target is declared; a non-port (pandas/plugin plumbing that
   has no native analogue); a deferred capability (plotting, network,
   Bayesian posterior helpers, calendar public utilities); otherwise an
   unresolved gap.
4. The ledger records per family the public count, each classification and
   the unresolved names. Deferred and non-port rows are scope decisions, not
   parity claims.

Limits: this is a **name-level floor**, not a behavioral proof. A matching
name can still differ in contract, and a rename is credited only when the
target declaration exists; acceptance evidence for behaviour remains the
per-phase records. Methods, dataclasses and the 273-operation runtime catalog
are outside the function ledger (the catalog audit is in the Phase 92
record).

## Measured surface

| family | public | exact | renamed | deferred | non-port | unresolved |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| metrics | 157 | 123 | 18 | 16 | 0 | 0 |
| performance | 9 | 9 | 0 | 0 | 0 | 0 |
| portfolio | 30 | 25 | 2 | 0 | 2 | 1 |
| factor_analysis | 79 | 34 | 7 | 30 | 8 | 0 |
| simulation | 14 | 9 | 4 | 0 | 1 | 0 |
| optimization | 3 | 2 | 1 | 0 | 0 | 0 |
| data | 3 | 0 | 0 | 3 | 0 | 0 |
| risk | 20 | 20 | 0 | 0 | 0 | 0 |
| attribution | 18 | 12 | 1 | 2 | 1 | 2 |
| report | 1 | 1 | 0 | 0 | 0 | 0 |
| runtime | 13 | 4 | 2 | 0 | 7 | 0 |
| extensions | 3 | 1 | 0 | 0 | 2 | 0 |
| viz | 1 | 0 | 1 | 0 | 0 | 0 |
| **total** | **351** | **240** | **36** | **51** | **21** | **3** |

Exact-or-renamed coverage is 276/351 (78.6%); 51 deferred and 21 non-port
rows bring classified coverage to 348/351 (99.1%). No full-domain parity is
claimed.

## Unresolved gaps (3)

Gap closure during this audit: `simulation.annualize` is now a native public
helper (`annualize(value, period)` with the source period factors, tested in
`SimulationParameters.AnnualizesWithSourcePeriodFactors`), and
`fama_macbeth` plus `factor_model_inference` are ported with a clean oracle
fixture (`docs/acceptance/2026-10-23-phase-80-inference-acceptance.md`), and
the event-window routes (`common_start_returns`,
`average_cumulative_return_by_quantile`) are ported with their own oracle
(`docs/acceptance/2026-10-23-phase-80-event-windows-acceptance.md`), and the
round-trip helpers `apply_sector_mappings_to_round_trips` and
`add_closing_transactions` are ported with typed ledgers
(`docs/acceptance/2026-10-23-phase-59-round-trip-helpers-acceptance.md`).
These seven closures reduce the initial ten gaps to three.

- `portfolio.agg_all_long_short` — arbitrary caller-supplied statistic
  functions over a pandas round-trip frame have no typed native analogue;
  the native round-trip stats cover grouping/extraction/standard summaries.
- `attribution.align_and_warn` — the four-panel attribution alignment helper
  with warning policy is not ported.
- `attribution.calculate_idiosyncratic_risk` — Fama-French idiosyncratic
  variance is not ported.

## Deferred and non-port scope decisions

- 51 deferred: matplotlib tear-sheet/plot functions (native ImPlot chart data
  instead), network factor/provider fetchers (`data` domain; excluded by the
  runtime scope), Bayesian/bootstrap posterior helpers, public calendar
  utilities (business-day handling exists internally), and pandas-only
  utility helpers.
- 21 non-port: pandas ownership/inspection plumbing (`fingerprint_value`,
  `frozen_mapping`, `snapshot_pandas`, ...), plugin discovery/loading,
  Python lazy-import catalog composition, timezone-normalization helpers
  (native timestamps are UTC), and internal validation helpers replaced by
  typed native validation.
- Runtime catalog: 20 of 273 source operations are registered under exact
  source IDs; the other domains are classified in the Phase 92 acceptance
  record (typed inputs pending, data utilities excluded, plot-only deferred).

## Iteration status

| iteration | accepted scope | open |
| --- | --- | --- |
| 1 — metrics completion | Phases 39–52b accepted with oracle evidence | 3 function-level gaps as above (metrics itself has 0) |
| 2 — performance/portfolio | Phases 53–59 accepted | none at function level |
| 3 — risk/optimization | Phases 60–71 accepted; 661/661 CTest+ASan+UBSan at close | none at function level |
| 4 — visualization | chart data and ImPlot adapter accepted for compute/build | visual rendering **NOT_RUN**; sanitizer not re-run after the changes |
| 5 — factor analysis | 77–87 slices accepted: prep/PIT, performance/inference/event windows, costs, analysis model, research, export, tears, four-section report, portfolio constructor, performance study | group analysis absent; tear-sheet plotting deferred |
| 6 — report/runtime | 88a/89/90/91/92/93 accepted: schema, HTML, XLSX, risk/portfolio/transactions projections, runtime catalog, install/export consumer, usage/migration notes | 88b integration fields partial; transaction turnover metric open; 93 CI matrix NOT_RUN |

## Gate status

- Release CTest: **718/718 passed** (this audit).
- Sanitizer: last complete ASan+UBSan run **661/661** (Iteration 3 close and
  layout alignment). No later slice claims a sanitizer run; the gate remains
  **NOT_RUN** for Iterations 4–6.
- Visual rendering: **NOT_RUN** (no screenshots; ImPlot adapter build not
  re-exercised).
- Supported-platform CI matrix: **NOT_RUN**; only the local macOS arm64
  install/consumer path is evidenced.
- XLSX byte parity with the Python writer: not claimed; openpyxl load
  validation only.
- Python-side timing: not measured; Phase 87 numbers are native-only.

## Verdict

The native program covers the accepted compute, model, report, runtime and
packaging paths with 276 exact/renamed public routes and 72 explicitly
deferred or non-port routes. Three named gaps remain, plus the iteration-level
items above (visual, sanitizer re-run, CI matrix, 88b fields, transaction
turnover). Program-level parity is therefore **partial and explicitly
bounded**; this record is the authoritative close-out of Phase 92 and must be
updated if any listed gap is closed.
