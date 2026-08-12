# Phase 33 Native Common-Sense Ratio Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to
> implement this plan task-by-task.

**Goal:** Add the smallest useful dependency-closed Plan A parity slice by
composing the accepted native `tail_ratio` and `win_rate` APIs into
`common_sense_ratio`.

**Architecture:** Add one scalar declaration to the existing ratio leaf and
one definition that preserves the Python raw-length/branch structure while
delegating the numerical leaves. Reuse the existing ratio and metrics-facade
test binaries; add no new source target, facade include, or dependency.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, mandatory
Anaconda Python source oracle, Homebrew LLVM 22 ASan+UBSan, and optional local
ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 32 ends at
`19af87c205cca20c678517d7458b779e1a3f440f` with `434` headless tests and a
clean tracked worktree. Do not amend any prior plan, implementation,
acceptance, correction, trace, or ignored evidence.

## Global constraints

- Use `fincore::dataframe`, `fincore::input_error`, public `tail_ratio`, public
  `win_rate`, and the C++ standard library only. Do not embed Python or add a
  dependency, source target, provider, network call, solver, plot source,
  report, package, or GUI runtime.
- Add exactly one native symbol in `metrics/ratios.hpp`; do not duplicate the
  Python compatibility alias in `metrics/stats.hpp`.
- Reject an embedded NUL before every DataFrame C-string boundary, validate the
  genuine-double selected column before the raw `<2` shortcut, then call each
  accepted public leaf exactly once. Legal bounded non-NUL-terminated views
  must remain supported.
- Preserve raw-count, NaN, zero-win, all-win positive-infinity, default-column,
  named-column, structural-error, and value-lifetime behavior from the design.
- Compose the accepted native finite-filter semantics and explicitly document
  the signed-infinity divergence from Python; do not add a third cleaning
  policy.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.
- Do not implement `mar_ratio`, `var_excess_return`, conditional ratios,
  weekly/monthly consecutive statistics, EVT, market aliases, providers,
  reports, or plots in this phase.

### Task 140: Record Phase 33 design and executable plan

**Files:**

- Create: `docs/design/2026-08-13-phase-33-common-sense-ratio-design.md`
- Create: `docs/plans/2026-08-13-fincore-cpp-phase-33-common-sense-ratio.md`

1. Record the global route audit, exact API, duplicate Python alias decision,
   public composition, validation order, raw versus finite count, branch
   semantics, signed-infinity divergence, oracle, mutant-resistant tests,
   gates, and exclusions.
2. Obtain a fresh independent plan review. Correct every Critical, Important,
   or Minor finding, run diff checks, explicitly stage only these two files,
   and commit:

   ```sh
   git commit -m "docs: plan phase 33 common sense ratio"
   ```

### Task 141: Add native common-sense ratio

**Files:**

- Modify: `include/fincore/metrics/ratios.hpp`
- Modify: `src/metrics/ratios.cpp`
- Modify: `tests/unit/extended_ratios_test.cpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`

1. Create the ignored Phase 33 SDD brief, progress ledger, oracle, RED log, and
   implementation report. Run this mandatory oracle from the Python source
   checkout:

   ```sh
   cd /Users/yunjinqi/Documents/new_projects/fincore
   /Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base python - <<'PY'
   import numpy as np
   import pandas as pd
   from fincore.metrics.ratios import common_sense_ratio
   from fincore.metrics.risk import tail_ratio
   from fincore.metrics.stats import win_rate

   cases = {
       "mixed": [.01, -.02, .03, -.01],
       "zeros_mixed": [.01, 0., -.02, .03, 0.],
       "all_win": [.01, .02, .03],
       "no_win": [-.01, 0., -.02],
       "single": [.01],
       "one_finite_two_raw": [.01, np.nan],
       "nan_mixed": [.01, np.nan, -.02, .03],
       "all_nan": [np.nan, np.nan],
       "zero_tail": [0., .01],
       "positive_infinity": [.01, np.inf, -.02, .03],
       "negative_infinity": [.01, -np.inf, -.02, .03],
   }
   for name, values in cases.items():
       series = pd.Series(values, dtype=float)
       print(name, len(series), tail_ratio(series), win_rate(series),
             common_sense_ratio(series))
   PY
   ```

2. Before adding production declarations, add leaf and facade compile uses to
   the two already registered test files. The leaf test must include only
   `<fincore/metrics/ratios.hpp>` among fincore headers. Build and retain the
   genuine missing-symbol compile RED; do not run stale CTest after failure.
3. Add the exact declaration and minimal implementation:
   - reject `column.find('\0')` with `input_error` before any DataFrame call;
   - resolve `require_double_column(returns, column)` next;
   - return NaN when the selected raw span has fewer than two rows;
   - call public `tail_ratio(returns, column)` once and public
     `win_rate(returns, column)` once;
   - return NaN for a NaN delegate or zero win rate;
   - return positive infinity for win rate exactly one; and
   - otherwise return the direct source quotient without clamping.
4. Test the exact signature/default call, Python finite goldens, public-leaf
   composition parity, zeros, raw-count versus finite-count, NaN branches,
   all-win infinity, named columns, embedded-NUL rejection, a bounded
   non-NUL-terminated selector, structural validation ordering, both signed-
   infinity native divergences, and metrics-facade visibility.
5. Run build; focused `ExtendedRatios|MetricsUmbrella`; related
   `Ratios|Risk|Stats|PerfStats`; full CTest; and diff checks. Explicitly stage
   and commit only the four paths:

   ```sh
   git commit -m "feat: add native common sense ratio"
   ```

6. Dispatch a fresh specification review followed by a fresh code-quality
   review. Correct every Critical, Important, or Minor finding in new scoped
   commits, rerun focused/full verification, and never amend accepted history.

### Task 142: Accept Phase 33

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-13-phase-33-common-sense-ratio-acceptance.md`

1. Document the global route selection, style-route closure, exact API and
   source mapping, public composition, validation/branch semantics, signed-
   infinity divergence, complete oracle, genuine dual RED, final
   implementation/review chain and exact scopes, and explicit exclusions.
2. Retain raw current focused/full and fresh Apple headless,
   registration-audited LLVM ASan+UBSan, ImPlot/demo compile/link-only/no-GUI,
   invalid-ImGui multiline diagnostic, and actual FetchContent/local
   dependency provenance evidence.
3. Keep both files unstaged for a fresh independent read-only document/gate
   review. Fix every Critical, Important, or Minor issue, then explicitly
   stage only the two files and commit:

   ```sh
   git commit -m "docs: accept phase 33 common sense ratio"
   ```

### Task 143: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-13-phase-33-common-sense-ratio-acceptance.md`

1. Record Task 141's complete implementation/fix chain and exact scopes plus
   Task 142's initial acceptance SHA/scope. State Task 143 is documentation-
   only, does not amend prior commits, and neither states nor anticipates its
   own SHA.
2. Run focused ratio/facade CTest and diff checks; explicitly stage only the
   acceptance document and commit:

   ```sh
   git commit -m "docs: trace phase 33 common sense ratio"
   ```

### Final Phase 33 review

Audit all Phase 33 ancestry/scopes; exact API/default; one native symbol;
genuine-double validation before raw length; one-time public delegation;
raw/finite count and every branch; finite oracle and signed-infinity native
divergence; facade visibility; no CMake/dependency drift; tests/provenance/
no-GUI wording; and non-self-referential trace. Fresh-configure Apple
headless, rerun focused/full CTest and the mandatory Anaconda oracle, then
publish Spec/Task-Quality verdicts. Correct every Critical, Important, or
Minor finding in new scoped commits.
