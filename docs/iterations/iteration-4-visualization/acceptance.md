# Iteration 4 — Visualization: Acceptance

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

## 1. Behavior and isolation

- [x] 72: monthly compounding/pivot values, years/months/order, missing-month
      mask and date boundaries match the oracle; normalization and color
      substitutions documented rather than assumed.
- [x] 73: returns and rolling-Sharpe protocol mappings include benchmark,
      alignment and metadata behavior; malformed/mismatched series rejected.
- [x] 74: filled and line drawdown plots consume identical computed values.
- [x] 75: edges, counts/densities, labels, empty/constant/NaN cases tested;
      any unsupported violin/QQ capability has a named disposition.
- [x] 76: correlation shape/labels tested; frontier consumes Phase 69's
      result without adapter-side optimization or moment recomputation.
- [x] All chart outputs survive input mutation/destruction; missing values
      remain distinct from zero; public chart headers compile headlessly.
- [x] Draw/render calls do not invoke financial kernels (instrumented call
      counts or a dependency test, not a search for numeric literals).

## 2. Build and runtime gates

- [x] Clean headless core configure/build/full CTest and relevant sanitizers.
- [x] Adapter ON build/link and `ImPlotAdapterCompile` pass. Separate demo
      ON configure/build result recorded with actual dependency revisions.
- [x] Invalid-ImGui check reports the expected failure when wiring changes.
- [x] Deterministic demo fixtures actually rendered; screenshots and visual
      checks cover all selected chart families, labels, scales, legends and
      documented substitutions. Otherwise VISUAL stays NOT_RUN/INCOMPLETE.
- [x] Four-method VizBackend mapping plus selected extras is complete; no
      unsupported backend-specific feature is counted as passed.

## 3. Sign-off

| Phase | Candidate SHA | COMPUTE | BUILD | VISUAL | Durable evidence / reviewer |
|---|---|---|---|---|---|
| 72 | `ef05555c5e7e` | PASS selected | PASS macOS adapter | PASS selected macOS captures | [Clause evidence][R], [candidate][C]; Codex main |
| 73 | `ef05555c5e7e` | PASS selected | PASS macOS adapter | PASS selected macOS captures | [Clause evidence][R], [candidate][C]; Codex main |
| 74 | `ef05555c5e7e` | PASS selected | PASS macOS adapter | PASS selected macOS captures | [Clause evidence][R], [candidate][C]; Codex main |
| 75 | `ef05555c5e7e` | PASS selected | PASS macOS adapter | PASS selected macOS captures | [Clause evidence][R], [candidate][C]; Codex main |
| 76 | `ef05555c5e7e` | PASS selected | PASS macOS adapter | PASS selected macOS captures | [Clause evidence][R], [candidate][C]; Codex main |
| Iteration close | `ef05555c5e7e` | PASS selected | PASS macOS adapter | PASS selected macOS captures | [Clause evidence][R], [candidate][C]; Codex main |

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
