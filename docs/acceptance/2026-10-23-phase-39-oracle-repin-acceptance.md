# Phase 39 Oracle Re-Pin Acceptance

## Scope

Documentation-and-evidence-only phase. Re-pins the C++ parity baseline from
Python `2354bb560788f372502151a8b1b53accf536a6c0` to `dev@4b8f31b`. No C++
source, test, target, dependency, or CMake change.

Chain: plan `docs/plans/2026-10-23-phase-39-oracle-repin.md` (this phase's
first commit), design `docs/design/2026-10-23-phase-39-oracle-repin-design.md`,
then this record and the README ledger entry.

## Measured results

- **Drift census:** nine changed paths in the 19-commit range, all under
  `fincore/factor_analysis/` and `fincore/report/factor/`; **0** changed files
  across the nine ported domains (`metrics`, `attribution`, `simulation`,
  `portfolio`, `risk`, `optimization`, `performance`, `runtime`, `viz`).
- **Positive oracle re-run at `4b8f31b`:** clean detached worktree, module
  path asserted under the detached root, exit `0`, zero stderr bytes,
  `FAILURES=0`. Value-domain output byte-identical to the old-pin run
  (SHA-256 `b651d86e88311b2f6e97c2981f1d760b601d8ee47a2086068e7cf8d255224809`
  at both pins).
- **Full-suite gate:** 490/490 headless CTest cases passed at the
  documentation head.

## Outcome

No goldens required re-baselining; no re-baseline phase is opened. The new
pin `4b8f31b` governs all subsequent phases. Iterations 5 and 6 must run
their own re-pin checkpoint before starting, because their source domains are
exactly the ones that moved in this range.

## Limits

This phase makes no parity claim beyond the ported domains. It does not
authorize touching the factor-analysis or report source, nor any release
action.
