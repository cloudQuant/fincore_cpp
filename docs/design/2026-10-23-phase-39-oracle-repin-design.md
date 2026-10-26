# Phase 39 Oracle Re-Pin Design

**Old pin:** `2354bb560788f372502151a8b1b53accf536a6c0`
**New pin:** `4b8f31b` (`fix(report): render unavailable metrics for empty
factor quantiles`, v0.5.2.dev0)
**Range:** 19 commits (`git rev-list --count 2354bb5..4b8f31b`)

## 1. Drift census

`git -C /Users/yunjinqi/Documents/new_projects/fincore diff --name-status
2354bb5..4b8f31b -- fincore/` reports exactly nine changed paths:

| Status | Path | Domain |
|---|---|---|
| M | `fincore/factor_analysis/analysis.py` | factor analysis |
| M | `fincore/factor_analysis/models.py` | factor analysis |
| M | `fincore/factor_analysis/performance.py` | factor analysis |
| M | `fincore/factor_analysis/portfolio.py` | factor analysis |
| M | `fincore/factor_analysis/render_matplotlib.py` | factor analysis |
| A | `fincore/factor_analysis/research.py` | factor analysis (new) |
| M | `fincore/report/factor/__init__.py` | report |
| M | `fincore/report/factor/compute.py` | report |
| A | `fincore/report/factor/html.py` | report (new) |

Changed-file count across the nine ported domains — `metrics/`,
`attribution/`, `simulation/`, `portfolio/`, `risk/`, `optimization/`,
`performance/`, `runtime/`, `viz/` — is **0**. Every ported public function's
Python source is byte-identical between the two pins, so no golden requires
re-derivation. The drift is confined to the two domains scheduled for
Iterations 5 and 6, which will be pinned at their own start dates.

## 2. Positive re-run at the new pin

A clean detached worktree was created under
`/tmp/fincore-phase39-python.lm1A2Z/source` at `4b8f31b`. The Phase 38 oracle
script was copied unchanged and executed with
`FINCORE_PHASE38_PYTHON_SOURCE`/`PYTHONPATH` bound to the detached root,
`PYTHONDONTWRITEBYTECODE=1`, and the mandatory Anaconda interpreter:

- imported module asserted under the detached root:
  `fincore_file=/private/tmp/fincore-phase39-python.lm1A2Z/source/fincore/__init__.py`;
- exit status `0`; stderr size `0` bytes;
- `FAILURES=0`;
- raw output contains three environment-dependent lines (`fincore_file`,
  `fincore_source_root`, `sys_path_head`). Excluding those, the complete
  value domain is byte-identical to the Phase 38 old-pin run:
  SHA-256 `b651d86e88311b2f6e97c2981f1d760b601d8ee47a2086068e7cf8d255224809`
  at **both** pins.

This is the required positive control: the newest metric's goldens were
re-produced, not inferred, at the new pin.

## 3. Full-suite gate

Full headless CTest at the documentation head: **490/490 passed**. No source,
test, or build file changed in this phase.

## 4. Conclusion

The parity baseline is re-pinned to `4b8f31b`. No golden edits, no re-baseline
phase, and no C++ changes are required. Earlier phase acceptance records that
cite `2354bb5` remain historically accurate; Phase 39 is the authoritative
pin statement for all subsequent phases until the next re-pin.
