# Iteration 3 — Risk and Optimization: Acceptance

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

## 1. Spike and numerical checks

- [x] 60: every required problem class/constraint mapped to the chosen
      solver strategy; representative, ill-conditioned, infeasible and
      nonconvergent proofs recorded. Build/sanitizer/dependency evidence is
      real; no production API acceptance inferred from the spike.
- [x] Numeric budgets and failure mappings fixed before dependent code;
      existing exact contracts preserved. Only relevant comparison classes
      apply to closed-form phases such as 64.
- [x] 61–62: fixed-parameter likelihood/variance/gradient checks, forecast
      and result mappings, constants/short inputs and optimizer failures;
      distinct GARCH/EGARCH/GJRGARCH recursions independently verified.
- [x] 63: Hill/GPD/GEV and tail APIs reconciled; distribution support,
      shape boundaries, extreme quantiles and failed fits covered.
- [x] 64: beta fragility and any aligned/native-equivalent mapping proven.
- [x] 65: counts, transition tables and traffic-light boundaries exact;
      p-values/statistics use declared budgets; zero/all exceptions and
      degenerate samples covered.
- [x] 66: exact training/forecast schedule and estimator dispatch; a
      future-data perturbation fixture proves no leakage into earlier fits.
- [x] 67: spec/report/metadata fields and input-change invalidation tested;
      digest recipe and cross-language/native-version policy recorded;
      Phase 88a projection works without rendering or recomputation.
- [x] 68–71: bounds, weight sum, target return/risk budget and objective
      residuals tested; infeasible and max-iteration outcomes explicit.
      Nonunique optimum comparisons use objective/feasibility; risk parity
      meets a declared ERC residual budget rather than assumed machine zero.
- [x] Native repeatability tested under the fixed executable/thread settings;
      platform comparisons use declared budgets and include diagnostics.

## 2. Iteration close

- [x] Ledger covers risk submodules, optimization and metric routes owned
      by 63–64, including forecast/calibration/GEV paths.
- [x] Public consumer/ownership checks, complete candidate CTest and
      ASan+UBSan pass; benchmark impact recorded and evidence retained.
- [x] Phase records and README trace agree; rendering remains assigned to
      91 and no blocked solver capability is labeled accepted.

## 3. Sign-off

| Phase | Candidate SHA | Manifest | Status | Durable evidence / reviewer |
|---|---|---|---|---|
| 60 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS spike; API gates separate | [Clause evidence][R], [candidate][C]; Codex main |
| 61 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 62 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 63 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 64 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 65 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 66 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 67 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 68 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 69 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 70 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
| 71 | `ef05555c5e7e` | [Source][M], [fixtures][O] | PASS selected native scope | [Clause evidence][R], [candidate][C]; Codex main |
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
