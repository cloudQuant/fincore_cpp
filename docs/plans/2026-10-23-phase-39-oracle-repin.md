# Phase 39 Oracle Re-Pin Plan

**Goal:** Re-pin the C++ parity baseline from fincore Python
`2354bb560788f372502151a8b1b53accf536a6c0` to `dev@4b8f31b`
(v0.5.2.dev0), demonstrating by measurement — not assumption — that the
ported metric/attribution/simulation/portfolio surface is unchanged and its
goldens remain valid.

**Scope:** Documentation and evidence only. This phase changes no C++ source,
test, target, dependency, or CMake file. It is the first phase of Iteration 1
after the Phase 38 close-out.

## Constraints

- Do not re-run or tune any C++ golden. If a drift is found, stop and open a
  re-baseline phase instead of editing tests here.
- Oracle runs use only a clean detached worktree of the Python repository and
  the mandatory Anaconda interpreter
  (`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`) with
  `PYTHONPATH`/`FINCORE_PHASE39_PYTHON_SOURCE` bound to it and
  `PYTHONDONTWRITEBYTECODE=1`.
- Evidence roots are unique `/tmp/fincore-phase39-*` directories.
- No commit outside `docs/plans`, `docs/design`, `docs/acceptance`, and the
  README ledger.

## Steps

1. **Drift census (measured).** In the Python checkout, enumerate
   `git diff --name-status 2354bb5..4b8f31b -- fincore/` and count changed
   files per directory. The ported domains are: `metrics/`, `attribution/`,
   `simulation/`, `portfolio/`, `risk/`, `optimization/`, `performance/`,
   `runtime/`, `viz/`. Requirement: the changed-file count in those domains
   is zero; any nonzero count triggers per-file inspection and an oracle
   re-run for every affected public function.
2. **Positive re-run at the new pin.** Create a clean detached worktree at
   `4b8f31b`, run the retained Phase 38 oracle script unchanged, assert the
   imported module resolves under the detached root, require exit `0` and
   zero stderr bytes, and byte-compare the raw output with the Phase 38
   run at the old pin (`39c640cf7d2a68c98bad661bd6d8fa3ddd1933d898c3f221248c8caead7c085b`).
   Identical output proves the newest metric's goldens survive the re-pin.
3. **Full-suite gate.** Re-run the complete headless CTest suite at the
   current `dev` head to confirm 490/490 at the re-pin documentation head.
4. **Records.** Commit this plan; then commit the design document (old/new
   SHAs, census table, oracle hashes and diff result) and the acceptance
   record plus README ledger entry.

## Acceptance criteria

- Census shows zero changed files across all nine ported domains.
- New-pin oracle: exit `0`, zero stderr, module path under the detached root,
  raw-output SHA-256 identical to the old-pin run, `FAILURES=0`.
- Full CTest green at the documentation head.
- No C++ source, test, or build file changed in the phase's commit set.
