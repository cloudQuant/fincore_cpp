# Phase 36 Native R-Cubed Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to
> implement this plan task-by-task.

**Goal:** Add the smallest remaining Empyrical-registered, non-alias metric
calculation whose numerical leaf is already native by composing
`safe_correlation` into signed R-cubed.

**Architecture:** Add one scalar declaration to the existing stats leaf and
one definition that rejects embedded NUL selectors, invokes the public
`safe_correlation` exactly once, and returns its signed cube. Reuse the
registered stats and metrics-facade tests; add no source target, facade include,
helper, result type, or dependency.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, mandatory
Anaconda Python source oracle, Homebrew LLVM 22 ASan+UBSan, and optional local
ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 35 ends at
`879a5eb21b17ad3a432875e804d00ac339d8e43d` with `457` headless tests and a
clean tracked worktree. Do not amend any prior plan, implementation,
acceptance, trace, review fix, or ignored evidence.

## Global constraints

- Use `fincore::dataframe`, `fincore::input_error`, public
  `fincore::safe_correlation`, and the C++ standard library only. Do not embed
  Python or add a dependency, source target, helper, result struct, provider,
  network call, solver, report, plot source, package, or GUI runtime.
- Add exactly one native symbol in `metrics/stats.hpp`. Do not add an overload,
  market-correlation alias, automatic-input API, array API, or facade include.
- Reject embedded NUL in both selectors, in parameter order, before every
  DataFrame call. Then call public `safe_correlation` exactly once and inherit
  all of its structure, timestamp alignment, finite-pair, sample-count,
  variance-threshold, and correlation semantics.
- Return NaN when the public leaf returns NaN. Otherwise use the direct signed
  product `correlation * correlation * correlation`.
- Do not copy alignment/correlation formulas, delegate through
  `capm_r_squared`, take an absolute value, square, clamp, or use `std::pow`.
- Preserve legal bounded non-NUL-terminated selectors and scalar ownership.
  Do not alter `safe_correlation`, `align_on_common_index`, or duplicate-index
  policy in this phase.
- Accept the native signed-infinity divergence: the public leaf removes every
  nonfinite pair, while Python removes only NaN pairs. Do not add Python's
  infinity behavior on top of the native leaf.
- Treat NumPy warnings as Python oracle evidence, not a new C++ diagnostic
  channel. Accept existing native reduction order within explicit tolerance.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.
- Do not implement compatibility aliases, standalone
  `simulation.base.annualize`, `kappa_three_ratio`, adjusted or conditional
  Sharpe, `mar_ratio`, calendar-resampled consecutive statistics, EVT,
  providers, reports, or plots.

### Task 152: Record Phase 36 design and executable plan

**Files:**

- Create: `docs/design/2026-08-13-phase-36-r-cubed-design.md`
- Create: `docs/plans/2026-08-13-fincore-cpp-phase-36-r-cubed.md`

1. Record the narrowly scoped route claim, compatibility aliases and smaller
   standalone utility, exact Python export/registry mapping, signed formula,
   exact native API, public-leaf composition, selector validation, alignment,
   finite-value divergence, oracle, mutation-resistant tests, gates, and
   exclusions.
2. Obtain a fresh independent specification review and a separate plan-quality
   review. Correct every Critical, Important, or Minor finding in the same
   two-file candidate, rerun scoped reviews and diff checks, explicitly stage
   only these paths, and commit without amending history:

   ```sh
   git commit -m "docs: plan phase 36 r cubed"
   ```

### Task 153: Add native R-cubed

**Files:**

- Modify: `include/fincore/metrics/stats.hpp`
- Modify: `src/metrics/stats.cpp`
- Modify: `tests/unit/stats_test.cpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`

1. Create the ignored Phase 36 SDD brief, progress ledger, source oracle, RED
   evidence, and implementation report. Run this mandatory oracle from the
   Python source checkout:

   ```sh
   cd /Users/yunjinqi/Documents/new_projects/fincore
   PYTHONDONTWRITEBYTECODE=1 \
   /Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base \
     python - <<'PY'
   import warnings
   import numpy as np
   import pandas as pd
   from fincore.metrics.stats import r_cubed

   cases = [
       ("ordinary_positive",
        np.array([1., 2., 4., 8.]),
        np.array([1., 3., 4., 7.])),
       ("ordinary_negative",
        np.array([1., 2., 4., 8.]),
        -np.array([1., 3., 4., 7.])),
       ("perfect_positive",
        np.array([1., 2., 3., 4.]),
        np.array([2., 4., 6., 8.])),
       ("perfect_negative",
        np.array([1., 2., 3., 4.]),
        np.array([4., 3., 2., 1.])),
       ("pairwise_nan",
        np.array([1., np.nan, 3., 4.]),
        np.array([2., 5., 6., 8.])),
       ("constant",
        np.array([1., 1., 1.]),
        np.array([1., 2., 3.])),
       ("tiny_variation",
        np.array([0., 5e-16]),
        np.array([.01, -.02])),
       ("empty",
        np.array([], dtype=float),
        np.array([], dtype=float)),
       ("one_pair", np.array([1.]), np.array([2.])),
       ("positive_inf_pair",
        np.array([.01, np.inf, -.02, .03]),
        np.array([.005, .1, -.01, -.01])),
       ("negative_inf_pair",
        np.array([.01, -np.inf, -.02, .03]),
        np.array([.005, .1, -.01, -.01])),
       ("huge_pair",
        np.array([np.finfo(float).max, -np.finfo(float).max]),
        np.array([1., -1.])),
       ("misaligned_series",
        pd.Series(
            [.1, .2, np.nan, .4],
            index=pd.to_datetime([
                "2020-01-01", "2020-01-02",
                "2020-01-03", "2020-01-05"])),
        pd.Series(
            [.3, .2, .1, -.1],
            index=pd.to_datetime([
                "2020-01-02", "2020-01-03",
                "2020-01-04", "2020-01-05"]))),
       ("one_common_series",
        pd.Series(
            [.1, .2],
            index=pd.to_datetime(["2020-01-01", "2020-01-02"])),
        pd.Series(
            [.3, .4],
            index=pd.to_datetime(["2020-01-02", "2020-01-03"]))),
   ]
   for name, returns, factors in cases:
       with warnings.catch_warnings(record=True) as caught:
           warnings.simplefilter("always")
           result = r_cubed(returns, factors)
       messages = " | ".join(str(item.message) for item in caught) or "-"
       print(f"{name}={result!r}; warnings={len(caught)}; "
             f"messages={messages}")
   PY
   ```

2. Before adding the declaration or definition, remove the leaf consumer's
   direct `<fincore/error.hpp>` include and add the exact
   `double (*)(const dataframe&, const dataframe&, std::string_view,
   std::string_view)` function-pointer `static_assert` plus a two-argument
   default-column call to both registered test translation units. Keep
   `<fincore/metrics/stats.hpp>` as the leaf consumer's only fincore include.
   Force Ninja to continue through both consumer compilations and retain a
   zsh-safe genuine missing-symbol dual-TU RED:

   ```zsh
   red_log=.superpowers/sdd/2026-08-13-fincore-cpp-phase-36-r-cubed/task-153-red-build.log
   set +e
   cmake --build build --target fincore_tests --parallel 4 -- -k 0 \
     >"$red_log" 2>&1
   red_exit=$?
   set -e
   test "$red_exit" -ne 0
   rg -n 'stats_test\.cpp' "$red_log"
   rg -n 'metrics_umbrella_test\.cpp' "$red_log"
   rg -n 'r_cubed' "$red_log"
   ```

   The log must show the symbol absent in both translation units and no other
   root cause. Record `red_exit` explicitly; do not use zsh's reserved
   `status` name or infer pipeline status. Do not run CTest after this failed
   build.
3. Add the exact declaration and minimal definition:
   - reject returns-selector embedded NUL;
   - reject benchmark-selector embedded NUL before any DataFrame call;
   - call public `safe_correlation` exactly once with both exact selectors;
   - return NaN when that result is NaN;
   - otherwise return the direct signed triple product.
4. Test exact signature/default use, ordinary positive/negative Python
   goldens, perfect `+1/-1`, direct public-leaf cube parity, timestamp
   intersection/nonmonotonic order, pairwise NaN, no common index, one finite
   pair, valid typed-empty inputs, constant and tiny-variation boundaries,
   symmetric maximum-finite inputs whose variance overflows to a NaN result,
   and accepted signed-infinity filtering divergence. Add the finite identical
   two-point `{-10.0, -0.3}` endpoint-rounding fixture: assert the public
   correlation and R-cubed are each strictly greater than `1.0`, and assert
   exact triple-product parity, so a `[-1, 1]` clamp mutant cannot pass. Also
   test default/named/bounded selectors, both embedded-NUL pre-DataFrame
   branches, both sides' empty-selector/missing/wrong-type/malformed structure
   order, scalar ownership, and metrics-facade visibility.
5. Run these exact current-tree gates and diff checks. Explicitly stage and
   commit only the four paths:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -N
   ctest --test-dir build -R 'Stats|MetricsUmbrella' \
     --output-on-failure --parallel 4
   ctest --test-dir build -R \
     'Dataframe|Stats|AlphaBeta|Timing|Rolling|Yearly' \
     --output-on-failure --parallel 4
   ctest --test-dir build --output-on-failure --parallel 4
   git diff --check
   git diff --cached --check
   ```

   ```sh
   git commit -m "feat: add native r cubed"
   ```

6. Dispatch a fresh specification review and a separate fresh code-quality
   review. Correct every Critical, Important, or Minor finding in new scoped
   commits, rerun focused/full verification, and never amend accepted history.

### Task 154: Accept Phase 36

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-13-phase-36-r-cubed-acceptance.md`

1. Document the narrow route claim and excluded aliases/utility, Python export
   mapping, signed formula, exact native API, public-leaf control flow,
   selector validation, timestamp/finite-value policies, full oracle including
   warnings, signed-infinity divergence, genuine dual RED, final
   implementation/review chain with exact scopes, and exclusions.
2. Retain raw current build/registration/focused/related/full and fresh Apple
   headless, registration-audited LLVM ASan+UBSan, ImPlot/demo
   compile-link-only/no-GUI, invalid-ImGui multiline diagnostic, and actual
   FetchContent/local dependency provenance evidence. Record actual counts,
   versions, warning families, commands, roots, and statuses; do not copy prior
   phase numbers.
3. Keep both files unstaged for a fresh independent read-only document/gate
   review. Fix every Critical, Important, or Minor issue, run a scoped
   re-review, then explicitly stage only the two files and commit:

   ```sh
   git commit -m "docs: accept phase 36 r cubed"
   ```

### Task 155: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-13-phase-36-r-cubed-acceptance.md`

1. Record Task 153's complete implementation/fix chain and exact scopes plus
   Task 154's initial acceptance SHA/scope. State Task 155 is documentation-
   only, does not amend prior commits, and neither states nor anticipates its
   own SHA.
2. Run the exact focused command and diff checks; explicitly stage only the
   acceptance document and commit:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'Stats|MetricsUmbrella' \
     --output-on-failure --parallel 4
   git diff --check
   ```

   ```sh
   git commit -m "docs: trace phase 36 r cubed"
   ```

### Final Phase 36 review

Audit all Phase 36 ancestry/scopes; the narrow route claim; exact API/defaults;
one native symbol; two pre-DataFrame NUL checks; exactly one public
`safe_correlation` call; signed triple product; timestamp alignment and native
finite-pair inheritance; negative-correlation and infinity-divergence tests;
facade visibility; no CMake/dependency drift; oracle/provenance/no-GUI wording;
and non-self-referential trace. Configure one unique fresh AppleClang headless
root, build it completely, audit registration with `ctest -N`, then run the
exact focused `-R 'Stats|MetricsUmbrella'` selection and full CTest from that
root. Rerun the mandatory Anaconda source oracle, then publish separate Spec
and Task-Quality verdicts. Correct every Critical, Important, or Minor finding
in new scoped commits.
