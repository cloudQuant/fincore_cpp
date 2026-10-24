# Iteration 1 — Metrics Completion: Acceptance

**Status:** PASS for the selected current native contracts, with the explicit
exclusions, partial source models and historical variances below.
The [shared gates](../delivery-contract.md#5-execution-and-evidence-gates)
apply to native implementation; Phase 39 separately supplies baseline evidence.


Primary close-out: 2026-10-24, Codex main agent. Candidate is dirty Git HEAD
`4859386ff8c260aa241553da4f0aca359d895517` plus the 305-file native snapshot
`ef05555c5e7e0b39e385944b7b2b40a25a8d4710f1e8351625f37f0578083e7f`. A short `ef05555c5e7e` below names that snapshot, not a commit.
The [candidate record][C] and [exact clause review][R] supply the execution
results and the interpretation of each clause. Checked boxes apply to those
explicit native contracts; they do not accept an excluded Python facade,
unexecuted backend, or unsupported whole-class parity. Raw FAIL/GAP evidence
retains its original status in [the resolution record][D].

## 1. Preparatory acceptance

- [x] Phase 38 uses the existing exact declaration and original pin; tests
      cover raw/clean n, n=19/20/49/50, constant/near-constant and NaN/±inf
      returns/risk-free, selector precedence, input order and ownership.
- [x] Fresh Phase 38 full suite/sanitizers, acceptance record and README entry
      exist. A committed implementation alone does not pass this gate.
- [x] Phase 39 records full old/new source/environment identities and import
      paths, paired outputs before replacement, classified drift and zero
      unresolved dependency drift; script/fixture/output hashes retained.
- [x] Phase 39 capability ledger reconciles exports, catalog, direct modules
      and consumers. Benchmark baseline and supported-platform decision exist.

## 2. Behavior acceptance

- [x] 40–47: source-correct canonical names and formulas; fixtures distinguish
      arithmetic MAR from CAGR and left-tail conditional Sharpe from an
      above-cutoff implementation. Alias dispositions are explicit.
- [x] 42/48: special-function/regression budgets declared numerically before
      implementation; exact structural and nonfinite classes preserved.
- [x] 49: objective evaluation on fixed θ values, boundary/nonconvergence
      cases and final optimizer result satisfy the declared budget; repeated
      native runs satisfy the stated reproducibility contract.
- [x] 50: row order, every field, timestamps and unrecovered episodes match
      oracle mapping; underwater series matches the accepted native kernel;
      owned results survive input destruction.
- [x] 51: names, inclusive/exclusive date boundaries, empty windows and
      historical-data pin asserted.
- [x] 52: all bootstrap/distribution routes mapped; identical resampling
      indices exercise numeric parity; seeded native runs repeat; no NumPy
      stream equality claimed without explicit matching evidence.
- [x] 52b: weekly/monthly consecutive, residual rolling/aligned, turtle and
      remaining discovered capabilities have implemented mappings or dated
      explicit dispositions. No unexplained `GAP` is closed by omission.

## 3. Iteration close

- [x] Every assigned phase has a public route/test/oracle/acceptance mapping.
- [x] Full registered CTest and ASan+UBSan pass on the recorded candidate;
      public consumer compilation and ownership checks pass; durable evidence
      is retrievable. Identical candidate evidence may be reused.
- [x] Metric domain coverage explicitly lists outstanding Phases 63–64 and
      other deferred rows; no premature full-domain parity claim.
- [x] README trace is updated; Git delivery status is recorded separately.

## 4. Current-candidate sign-off

| Phase | Candidate SHA | Oracle/fixture manifest | Status | Durable evidence / reviewer |
|---|---|---|---|---|
| 38 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 39 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS current evidence; historical ordering variance | [Clause evidence][R], [candidate][C]; Codex main |
| 40 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 41 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 42 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 43 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 44 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 45 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 46 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 47 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 48 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 49 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 50 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 51 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 52 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 52b | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
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
