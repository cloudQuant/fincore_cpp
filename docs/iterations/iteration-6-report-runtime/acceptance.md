# Iteration 6 — Report, Runtime and Native Delivery: Acceptance

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

## 1. Schema and rendering

- [ ] 88a precedes 67/84/86 consumers; shared section/table/series/artifact
      schema and all required metadata have a source mapping and public
      consumer test, with no GUI/solver dependency or dangling views.
      **Historical GAP:** the old execution order cannot be repaired
      retrospectively. Current schema/consumer behavior passes; this ordering
      variance is explicitly retained and dispositioned in [D].
- [x] 88b reconciles factor/portfolio/risk projection fields and migrations;
      special numeric/missing/date/cell types are losslessly represented or
      have an explicit output policy.
- [x] 89: structural parity, native determinism, Unicode/HTML/script escaping
      and actual offline browser rendering; no unexpected network fetch/JS
      error. Re-rendering has zero expensive compute calls.
- [x] 90: writer decision evidence; independent reopen confirms one sheet
      per section with metrics/tables/series, names/order/types/values and
      missing/nonfinite policy. Formula-like text is handled explicitly.
- [x] 90: real application opens the workbook; ZIP metadata/order and byte-
      or canonical-content determinism meet the declared contract. A valid
      ZIP alone does not prove XLSX correctness.
- [x] 91: complete portfolio/risk section mapping on ordinary and missing-
      optional-input fixtures; Phase 67 result is rendered without refitting.
- [x] Explicit writers validate output paths, no-overwrite semantics and
      partial failures; bundle media/provenance match written files.

## 2. Runtime and integration

- [x] 92: source-catalog reconciliation maps every selected operation and
      aliases; utilities have explicit native mappings; excluded operations
      are absent with recorded dispositions, not failing placeholders.
- [x] Ordered flat plan/batch behavior, stale catalog digest, unknown ID,
      malformed inputs and failure propagation match the pinned source.
      Provenance/config is not mistakenly forwarded to compute functions.
- [x] Session/snapshot/input/output isolation and deterministic request order
      tested; native fingerprint scope/version explicit.
- [x] End-to-end portfolio and factor or risk scenarios exercise catalog →
      plan/run/batch → computed model → multiple renderers → artifact bundle;
      instrumentation verifies compute reuse and changed-input invalidation.

## 3. Native delivery and program close

- [x] 93: clean build/test/sanitizer evidence on the declared matrix and
      exact candidate; CI job outcomes and skips/blockers separately reported.
- [x] Install/export/package contains headers, libraries, config/version,
      required report assets and notices; dependency versions pinned.
- [x] Independent relocated installed-prefix consumer succeeds with
      `find_package(fincore CONFIG REQUIRED)` while source/build roots are
      unavailable. Headless consumer has no GUI dependency; optional adapter
      consumer checked on platforms where it is claimed supported.
- [x] Offline configure/build proven with dependencies prepopulated; no
      unconditional source-checkout paths remain in installed metadata.
- [x] Equivalent-workload Release benchmarks report median/spread, RSS,
      conversion/compute/render/I/O and scaling against Phase 39; regressions
      resolved or explicitly dispositioned before any performance claim.
- [x] Master capability ledger reconciles all six iterations, historical
      mappings, 52b/63/64 cross-owners, exclusions and deferred rows. No
      unexplained GAP; no full-Python-package or untested-platform claim.
- [x] Full candidate regression, public consumer/ownership checks, durable
      manifests/scripts/logs, README trace and native usage/migration notes
      complete. Publish/tag/push status remains a separate authorized action.

## 4. Sign-off

| Phase | Candidate SHA | Manifest | Status | Durable evidence / reviewer |
|---|---|---|---|---|
| 88a early schema | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS schema; historical order GAP dispositioned | [Clause evidence][R], [candidate][C]; Codex main |
| 88b integration | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 89 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 90 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 91 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 92 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 93 native delivery | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS local gates; benchmark tradeoffs retained | [Clause evidence][R], [candidate][C]; Codex main |
| Iteration close | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| Program close | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |

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
