# Phase 37 Native Kappa-Three Ratio Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to
> implement this plan task-by-task.

**Goal:** Add the next smallest valuable, non-alias, dependency-closed public
calculation by implementing native Kappa-three ratio with its exact NaN-only,
lower-partial-moment, strict-threshold, infinity, and signed-zero semantics.

**Architecture:** Add one scalar declaration to the existing ratios leaf and
one definition that validates the selector, reuses the private NaN-only value
filter, computes third-order lower partial moment before the mean, and preserves
the source result classification without a finite clamp. Reuse the registered
ratios and metrics-facade tests; add no source target, facade include, helper,
result type, compatibility parameter, or dependency.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, mandatory
Anaconda Python source oracle, Homebrew LLVM 22 ASan+UBSan, and optional local
ImPlot compile/link acceptance.

---

**Baseline:** Accepted Phase 36 ends at
`f460860f5a40711b3adf1de23e36c8c8f738a046` with `467` headless tests and a
clean tracked worktree. Do not amend any Phase 36 or earlier plan,
implementation, acceptance, trace, correction, or ignored evidence.

## Global constraints

- Use `fincore::dataframe`, `fincore::input_error`, the existing private
  NaN-only `valid_values` helper in `src/metrics/ratios.cpp`, and the C++
  standard library only. Do not embed Python or add NumPy/SciPy, a dependency,
  source target, helper, result type, provider, network call, solver, report
  source, plot source, package, or GUI runtime.
- Add exactly
  `double kappa_three_ratio(const dataframe&, double = 0.0,
  std::string_view = returns_column)` to `metrics/ratios.hpp`. Add no overload,
  alias, automatic-input API, ndarray/Series API, or facade include.
- Omit Python's unused `risk_free`, `period`, and `annualization` parameters.
  Record that a future Python adapter may accept and discard them before
  calling this native leaf. Do not expose or validate ignored native state.
- Preserve exact order: NUL check, `require_double_column`, raw-size guard,
  NaN-only cleaning, clean-size guard, NaN-MAR propagation, downside LPM3,
  mean, strict threshold branch, direct powered quotient.
- Retain signed infinity in the cleaned vector. Do not substitute
  `detail::finite_values`, call another ratio, or silently adopt another
  metric's finite-value policy.
- Explicitly propagate NaN from `mar - value`, including same-signed infinity
  subtraction. Use strict `lpm3 < 1e-30`, return `+inf` only when `mu > mar`,
  and otherwise use `(mu - mar) / std::pow(lpm3, 1.0 / 3.0)`.
- Do not use `std::cbrt`, `finite_quotient`, absolute value, epsilon addition,
  range clamp, finite-result replacement, signed-zero normalization, or a C++
  warning channel.
- Preserve raw row order, duplicate/nonmonotonic timestamp labels, legal
  bounded non-NUL-terminated selectors, and scalar ownership.
- Treat NumPy warnings as source evidence only. Use explicit numeric tolerance
  only for ordinary finite power/reduction differences; classification,
  strict-threshold, infinity, and negative-zero checks remain exact.
- Every Python command must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.
- Do not implement literal aliases, `simulation.base.annualize`, adjusted or
  conditional Sharpe, `mar_ratio`, `r_cubed_turtle`, calendar resampling, EVT,
  providers, reports as a native subsystem, plots, or packages.

### Task 156: Record Phase 37 design and executable plan

**Files:**

- Create: `docs/design/2026-08-13-phase-37-kappa-three-ratio-design.md`
- Create: `docs/plans/2026-08-13-fincore-cpp-phase-37-kappa-three-ratio.md`

1. Record the narrowed route claim and alias/utility exclusions, exact Python
   formula and call order, export and registry-versus-explicit-Empyrical
   mapping, report consumer, native-first API, dead-parameter boundary,
   validation, index/ownership policy, NaN/infinity/threshold/overflow
   semantics, full oracle, mutation matrix, exact file scope, gates, and
   exclusions.
2. Obtain a fresh independent specification review and a separate plan-quality
   review. Correct every Critical, Important, or Minor finding in the same
   two-file candidate, rerun scoped reviews and diff checks, explicitly stage
   only these paths, and commit without amending history:

   ```sh
   git add \
     docs/design/2026-08-13-phase-37-kappa-three-ratio-design.md \
     docs/plans/2026-08-13-fincore-cpp-phase-37-kappa-three-ratio.md
   git diff --cached --check
   git commit -m "docs: plan phase 37 kappa three ratio"
   ```

### Task 157: Add native Kappa-three ratio

**Files:**

- Modify: `include/fincore/metrics/ratios.hpp`
- Modify: `src/metrics/ratios.cpp`
- Modify: `tests/unit/ratios_test.cpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`

1. Create the ignored evidence directory
   `.superpowers/sdd/2026-08-13-fincore-cpp-phase-37-kappa-three-ratio/` with a
   brief, progress ledger, exact source oracle, RED evidence, and
   implementation report. Do not stage ignored evidence.
2. Run this mandatory oracle from the pinned Python checkout and retain the
   complete output, warning categories/messages, status, Python HEAD, and
   environment provenance:

   ```sh
   cd /Users/yunjinqi/Documents/new_projects/fincore
   PYTHONDONTWRITEBYTECODE=1 \
   /Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base \
     python - <<'PY'
   import warnings
   import numpy as np
   import pandas as pd
   from fincore.metrics.ratios import kappa_three_ratio

   ordinary = np.array([
       0.01, 0.02, -0.05, 0.03, 0.01,
       -0.02, 0.04, 0.01, 0.02, -0.03,
   ])
   below = np.nextafter(1e-10, 0.0)
   duplicate_index = pd.to_datetime([
       "2024-01-03", "2024-01-01", "2024-01-03", "2024-01-02",
   ])
   maximum = np.finfo(float).max

   cases = [
       ("ordinary", ordinary, {}),
       ("ordinary_mar_0.1", ordinary, {"mar": 0.1}),
       ("dead_params", ordinary, {
           "risk_free": np.nan,
           "period": object(),
           "annualization": -np.inf,
       }),
       ("empty", np.array([], dtype=float), {}),
       ("singleton", np.array([0.01]), {}),
       ("nan_mixed", np.array([np.nan, 0.01, -0.02, np.nan, 0.03]), {}),
       ("one_clean", np.array([np.nan, 0.01]), {}),
       ("all_nan", np.array([np.nan, np.nan]), {}),
       ("all_positive", np.array([0.01, 0.02, 0.03]), {}),
       ("equal_mar", np.array([0.0, 0.0]), {"mar": 0.0}),
       ("strict_below_negative", np.array([-below, -below]), {}),
       ("strict_below_positive_mu", np.array([-below, 1.0]), {}),
       ("nominal_boundary", np.array([-1e-10, -1e-10]), {}),
       ("positive_inf", np.array([np.inf, 0.01, -0.02]), {}),
       ("negative_inf", np.array([-np.inf, 0.01, -0.02]), {}),
       ("both_inf", np.array([np.inf, -np.inf, 0.01]), {}),
       ("mar_nan", ordinary, {"mar": np.nan}),
       ("mar_pos_inf", ordinary, {"mar": np.inf}),
       ("mar_neg_inf", ordinary, {"mar": -np.inf}),
       ("same_pos_inf_mar", np.array([np.inf, 0.0]), {"mar": np.inf}),
       ("same_neg_inf_mar", np.array([-np.inf, 0.0]), {"mar": -np.inf}),
       ("max_overflow", np.array([-maximum, 0.0]), {}),
       ("duplicate_nonmonotonic", pd.Series(
           [0.01, -0.02, 0.03, -0.01], index=duplicate_index), {}),
   ]

   for name, returns, kwargs in cases:
       with warnings.catch_warnings(record=True) as caught:
           warnings.simplefilter("always")
           result = kappa_three_ratio(returns, **kwargs)
       messages = " | ".join(str(item.message) for item in caught) or "-"
       print(f"{name}={result!r}; warnings={len(caught)}; "
             f"messages={messages}")

   for name, value in [
       ("below_lpm3", np.mean(np.array([below, below]) ** 3)),
       ("exact_lpm3", np.mean(np.array([1e-10, 1e-10]) ** 3)),
   ]:
       print(f"{name}={value!r}; hex={float(value).hex()}; "
             f"below={bool(value < 1e-30)}")
   PY
   ```

   Require all 23 metric-result lines plus both threshold-diagnostic lines, 25
   output lines in total, with the exact scalar classifications and warning
   observations recorded in the design. The dead-parameter result must exactly
   equal the ordinary result. Do not turn warning text into a native diagnostic
   requirement.
3. Before adding the declaration or definition, add to both registered test
   translation units:
   - the exact
     `double (*)(const dataframe&, double, std::string_view)` signature
     `static_assert`; and
   - a one-argument default-column call.

   Keep `<fincore/metrics/ratios.hpp>` as `ratios_test.cpp`'s only fincore
   include. Add only required standard-library test includes. Force Ninja to
   continue through both consumers and retain a genuine missing-symbol RED:

   ```zsh
   evidence=.superpowers/sdd/2026-08-13-fincore-cpp-phase-37-kappa-three-ratio
   red_log="$evidence/task-157-red-build.log"
   set +e
   cmake --build build --target fincore_tests --parallel 4 -- -k 0 \
     >"$red_log" 2>&1
   red_exit=$?
   set -e
   test "$red_exit" -ne 0
   rg -n 'ratios_test\.cpp' "$red_log"
   rg -n 'metrics_umbrella_test\.cpp' "$red_log"
   rg -n 'kappa_three_ratio' "$red_log"
   ```

   Record `red_exit` explicitly and prove both translation units were
   attempted. The only root cause may be the absent Phase 37 symbol. Do not use
   zsh's reserved `status` variable and do not run or cite CTest after this
   failed build.
4. Add the exact declaration and minimal definition in the two production
   paths. Implement the design's literal order:
   - reject embedded NUL before DataFrame access;
   - require the genuine-double column;
   - return NaN for fewer than two raw values;
   - call existing NaN-only `valid_values` exactly once;
   - return NaN for fewer than two cleaned values;
   - return NaN for NaN MAR;
   - compute downside deviations and cubed LPM in original order before mean,
     explicitly returning NaN for a NaN deviation;
   - compute the arithmetic mean in original order;
   - apply strict `< 1e-30` and `mu > mar`; and
   - otherwise return the direct `std::pow` quotient without a clamp.
5. Add mutation-resistant tests for exact signature/default use, ordinary and
   MAR goldens, raw/clean sample guards, NaN-only filtering, zero-risk branch,
   strict-below/equal threshold, signed infinities, every nonfinite MAR class,
   both `[+inf, 0.0]`/`mar=+inf` and `[-inf, 0.0]`/`mar=-inf`
   same-signed-subtraction NaN cases, maximum-finite cube overflow and negative
   zero, original value order, duplicate/nonmonotonic timestamps,
   default/named/bounded selectors, NUL precedence, empty/missing/wrong-type/
   malformed structure, ownership, and metrics-facade visibility. Assert both
   `result == 0.0` and `std::signbit(result)` for the overflow negative zero.
6. Run the exact current-tree gates and record actual test counts rather than
   copying Phase 36 values:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -N
   ctest --test-dir build -R 'Ratios|MetricsUmbrella' \
     --output-on-failure --parallel 4
   ctest --test-dir build -R \
     'Dataframe|Basic|Returns|Drawdown|Ratios|Stats|PerfStats|AnalysisContext|Rolling' \
     --output-on-failure --parallel 4
   ctest --test-dir build --output-on-failure --parallel 4
   git diff --check
   git diff --cached --check
   ```

   Explicitly stage only the four Task 157 paths, re-run the cached diff check,
   inspect the index scope, and commit:

   ```sh
   git add include/fincore/metrics/ratios.hpp \
     src/metrics/ratios.cpp \
     tests/unit/ratios_test.cpp \
     tests/unit/metrics_umbrella_test.cpp
   git diff --cached --name-only
   git diff --cached --check
   git commit -m "feat: add native kappa three ratio"
   ```
7. Dispatch a fresh specification review and a separate fresh code-quality
   review over the committed Task 157 chain and ignored evidence. Correct every
   Critical, Important, or Minor finding in new explicitly scoped commits,
   rerun focused/related/full verification, and never amend accepted history.

### Task 158: Accept Phase 37

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-13-phase-37-kappa-three-ratio-acceptance.md`

1. After Task 157's final review SHA is fixed, document the narrow route claim
   and excluded aliases/utility, Python export/registry-versus-explicit facade
   mapping, report route, dead parameters and future-adapter boundary, exact
   native API/control flow, validation/index/ownership behavior, full source
   oracle with warnings, threshold/infinity/negative-zero policies, genuine
   dual RED, implementation/review chain and exact scopes, and exclusions.
2. From the committed Task 157 candidate, retain raw evidence for:
   - current build, registration, focused, related, full CTest, and both diff
     checks;
   - a unique fresh AppleClang Debug headless configure/build/registration,
     focused selection, full CTest, and warning scan;
   - a distinct fresh Homebrew LLVM 22 Debug ASan+UBSan headless configure,
     build, registration, complete strict-runtime CTest, sanitizer diagnostic
     scan, compile/link flag counts, `ratios.cpp` and final-link flag anchors,
     `otool -L` runtime count, a focused DYLD loaded-runtime observation, and
     UBSan symbol audit;
   - a unique fresh local DataFrame/ImGui/ImPlot Debug configuration with
     testing, adapter, and demo build ON, complete compile/link, verbose
     registration, full CTest, exact-once adapter evidence, artifact/linkage
     audit, warning-family audit, and proof that the demo was never executed
     and no GUI launched; and
   - a distinct absent-path invalid-ImGui configuration that exits exactly
     `1`, does not create the target, and matches the full diagnostic and exact
     dynamic path after whitespace normalization across physical line wraps.
3. Record actual commands, roots, SHAs, versions, statuses, test/Ninja counts,
   warning families, and dependency provenance. Verify local DataFrame, ImGui,
   and ImPlot checkouts plus each positive root's actual GoogleTest
   FetchContent checkout; do not copy Phase 36 counts or roots.
4. Keep both documentation files entirely unstaged for a fresh independent
   read-only document/gate review. Fix every Critical, Important, or Minor
   issue, run a scoped re-review and diff checks, explicitly stage only the two
   files, and commit:

   ```sh
   git add README.md \
     docs/acceptance/2026-08-13-phase-37-kappa-three-ratio-acceptance.md
   git diff --cached --name-only
   git diff --cached --check
   git commit -m "docs: accept phase 37 kappa three ratio"
   ```

### Task 159: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-13-phase-37-kappa-three-ratio-acceptance.md`

1. Record Task 157's complete implementation/fix chain and exact scopes plus
   Task 158's initial acceptance SHA/scope. State that Task 159 is
   documentation-only and does not amend prior commits. Neither state nor
   anticipate Task 159's own SHA.
2. Run the exact focused command and diff checks; explicitly stage only the
   acceptance document and commit:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'Ratios|MetricsUmbrella' \
     --output-on-failure --parallel 4
   git diff --check
   git add \
     docs/acceptance/2026-08-13-phase-37-kappa-three-ratio-acceptance.md
   git diff --cached --name-only
   git diff --cached --check
   git commit -m "docs: trace phase 37 kappa three ratio"
   ```

### Final Phase 37 review

Audit Phase 37 ancestry and exact scopes; the narrowed route claim; Python
export, facade, and report evidence; dead-parameter exclusion; exact native
API/defaults; NUL-before-DataFrame order; raw versus cleaned sample guards;
NaN-only cleaning; MAR/deviation NaN propagation; LPM-before-mean order;
strict threshold and `mu > mar`; direct `std::pow` quotient; infinity and
negative-zero preservation; selector/index/ownership tests; facade visibility;
no CMake/dependency drift; oracle/provenance/no-GUI wording; and
non-self-referential traceability.

Configure one unique fresh AppleClang headless root from the final committed
HEAD, build it completely, audit registration with `ctest -N`, then run exact
focused `-R 'Ratios|MetricsUmbrella'` and full CTest from that root. Rerun the
mandatory Anaconda source oracle. Do not substitute Task 158's old root for
this final audit and do not rerun GUI runtime. Publish separate Spec and
Task-Quality verdicts. Correct every Critical, Important, or Minor finding in
new scoped commits and repeat the affected final gates; never amend history.
