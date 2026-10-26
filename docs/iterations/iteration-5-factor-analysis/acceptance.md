# Iteration 5 — Factor Analysis: Acceptance

**Status:** PASS for the selected current native contracts, with the explicit
exclusions, partial source models and historical variances below.


Primary close-out: 2026-10-24, Codex main agent. Candidate is dirty Git HEAD
`4859386ff8c260aa241553da4f0aca359d895517` plus the 305-file native snapshot
`ef05555c5e7e0b39e385944b7b2b40a25a8d4710f1e8351625f37f0578083e7f`. A short `ef05555c5e7e` below names that snapshot, not a commit.
The [candidate record][C] and [exact clause review][R] supply the execution
results and the interpretation of each clause. Checked boxes apply to those
explicit native contracts; they do not accept an excluded Python facade,
unexecuted backend, or unsupported whole-class parity. Raw FAIL/GAP evidence
retains its original status in [the resolution record][D].

## 1. Data and kernel checks

- [x] 77: dense/sparse layout comparison and memory budgets recorded;
      one panel schema covers identity, dates/assets, membership/missingness,
      groups, weights and horizons. Ownership survives input destruction.
- [x] 77–78: tied/all-equal quantiles, duplicate edges, NaN/inf, duplicate
      pairs, unsorted dates, absent assets, group/zero-aware behavior and
      forward-return boundaries match the pinned paths; no assumed qcut mode.
- [x] 78: every preparation entry and horizon/precomputed/PIT variant mapped;
      loss accounting, publication/as-of and calendar-gap fixtures covered.
      Future-data perturbations do not affect earlier eligible computations.
- [x] 80: performance and inference ledger reconciled; regression/error
      budgets fixed; intermediate values and independent identities checked.
- [x] 81: long/unbalanced universe, changing membership, lag/period and
      turnover denominators matched; present NaN differs from absent pair.
- [x] 82: portfolio models/constraints plus cost/capacity units and gross/net
      identities matched; empty/zero-liquidity/degenerate cases included.
- [x] 79: full model assembled from accepted kernels; invocation counts
      verify reuse, and consumer mutation cannot corrupt cached snapshots.

## 2. Research and rendering checks

- [x] 83: evaluation returns tables/manifest without creating files; explicit
      export covers CSV schema, deterministic ordering, unique directories,
      no overwrite and partial-failure cleanup/status. Chart export is
      independently tested or explicitly NOT_RUN, never silently skipped.
- [x] 84: shared ReportDocument/section metadata mapping complete; no second
      incompatible factor schema or expensive render-time computation.
- [x] 85: ImPlot compute/build/visual results tracked per Iteration 4;
      every selected panel and substituted primitive has evidence.
- [x] 86: ECharts source/hash/notices pinned; HTML loads with network disabled,
      no external fetch or JS error, and actually renders expected charts
      in zh/en. Values/sections, escaping, missing/NaN/inf and non-ASCII labels
      validated. Repeated native rendering gives identical declared bytes.
- [x] 86: labels containing HTML/script delimiters cannot escape their data
      context; no raw JSON NaN/Infinity tokens emitted as valid JSON numbers.
- [x] 87: instrumentation proves reuse, allocation/copy behavior and cache
      invalidation. Equivalent workload time/RSS/scaling reported against
      the frozen baseline; numerical output and snapshot isolation preserved.

## 3. Close-out and sign-off

- [x] Complete factor capability ledger and explicit deferrals, full
      candidate CTest/sanitizers, affected benchmark results, public consumer
      checks and durable manifests/logs. No moving-head parity claim.

| Phase | Candidate SHA | Manifest | Status | Durable evidence / reviewer |
|---|---|---|---|---|
| 77 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 78 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 80 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 81 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 82 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 79 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 83 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS tables/CSV; chart export deferred | [Clause evidence][R], [candidate][C]; Codex main |
| 84 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 85 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 86 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 87 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS execution/semantics; comparator FAIL dispositioned | [Clause evidence][R], [candidate][C]; Codex main |
| Iteration close | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |

Named DEFERRED/NON_PORT routes and partial source classes remain visible in
the [master capability ledger](../capability-ledger.tsv) and its primary
disposition input. This is a current native-scope close-out; historical phase
records keep their original candidate identities and statuses. No hosted CI,
Windows support, full Python-package parity or external publication is claimed.

[C]: ../../acceptance/2026-10-23-revised-plan-candidate-acceptance.md
[R]: ../../acceptance/2026-10-23-revised-plan-checklist-review.md
[D]: ../../acceptance/2026-10-23-revised-plan-evidence-resolutions.md
[M]: ../../evidence/revised-plan-20261023/candidate/native-source-manifest.json
[O]: ../../evidence/revised-plan-20261023/historical-oracles/phase-manifest.csv
