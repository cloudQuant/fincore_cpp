# Iteration 2 — Performance and Portfolio: Acceptance

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

## 1. Domain checks

- [x] 53: geometric identity, NaN/empty behavior and returns at/below -1;
      tests distinguish return chaining from cashflow adjustment.
- [x] 55 then 54: same-period/date aggregation, sign-transition rejection,
      extreme rates, finite/nonfinite inputs, absent bracket and solver
      failure match the fixed oracle. Numeric rate and scaled-NPV residual
      budgets are declared and satisfied.
- [x] 55: positional versus labeled alignment, duplicate/unsorted dates,
      mismatched labels, same-local-day aggregation, timezone handling and
      day-count /365 are covered. Any narrower native input representation
      is declared with explicit rejections and coverage impact.
- [x] 56: deposits/withdrawals, start/end timing, missing valuations and
      valuation-date boundaries; adjusted TWR composes the same returns.
- [x] 57: all three inference exports mapped; ddof/sample gates, constants,
      confidence levels and normal-quantile budgets tested.
- [x] 57b: context fields and `render_disclosure` text behavior tested;
      applicability/missing fields/Unicode and deterministic output covered.
- [x] 59: liquidation/liquidity primitives have matching rows/labels/units;
      zero/negative/missing volume, cash/asset handling and multi-column
      structural validation follow the declared oracle contract.
- [x] 58: capacity config/assessment/`assess_liquidity` composition matches
      the oracle; existing native helper reuse and any deviations recorded.
- [x] Returned models survive caller mutation/destruction; public-only
      consumers compile without private headers or GUI dependencies.

## 2. Close-out

- [x] Performance/portfolio capability ledger has no unexplained gaps and
      includes 57b plus historical native mappings.
- [x] Candidate full CTest and sanitizer pass; affected benchmark checks have
      executed and their regression triggers are explicitly dispositioned;
      reproducible scripts, manifest and logs are durably retained.
- [x] Phase records and README trace link evidence. Native capability parity
      is not reported as GIPS compliance, Python binding or publication.

## 3. Sign-off

| Phase | Candidate SHA | Manifest | Status | Durable evidence / reviewer |
|---|---|---|---|---|
| 53 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 54 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 55 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 56 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 57 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 57b | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 58 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 59 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
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
