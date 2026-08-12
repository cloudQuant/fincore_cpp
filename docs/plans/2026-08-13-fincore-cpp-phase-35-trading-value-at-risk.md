# Phase 35 Native Trading Value at Risk Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to
> implement this plan task-by-task.

**Goal:** Add the smallest genuine missing public Plan A metric by implementing
the raw mean-minus-sample-standard-deviation `trading_value_at_risk` leaf.

**Architecture:** Add one scalar declaration to the existing risk leaf and one
definition that validates the selected DataFrame column, preserves the raw
length shortcut, and composes accepted internal mean/sample-variance helpers.
Reuse the registered risk and metrics-facade tests; add no source target,
facade include, or dependency.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, mandatory
Anaconda Python source oracle, Homebrew LLVM 22 ASan+UBSan, and optional local
ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 34 ends at
`ad621ca09454f1e7071ec227bc8694ab749ad7bb` with `449` headless tests and a
clean tracked worktree. Do not amend any prior plan, implementation, test
hardening, acceptance, trace, or ignored evidence.

## Global constraints

- Use `fincore::dataframe`, `fincore::input_error`,
  `detail::mean`, `detail::sample_variance`, and the C++ standard library only.
  Do not embed Python or add a dependency, source target, provider, network
  call, solver, plot source, report, package, or GUI runtime.
- Add exactly one native symbol in `metrics/risk.hpp`. Do not add a second
  overload, alias, multi-column API, automatic-returns API, or facade include.
- The native signature omits Python's completely unused `period` parameter.
  Do not add `frequency`, optional annualization, or hidden period validation.
- Reject embedded NUL before every DataFrame C-string boundary; validate the
  genuine-double selected column before the raw `<2` shortcut; then preserve
  the mean/sample-variance/direct-arithmetic order from the design.
- Do not use `finite_values`, `annual_volatility`, historical VaR, population
  variance, annualization, a zero-standard-deviation guard, a sigma domain
  guard, or a nonfinite-result clamp.
- Preserve raw NaN/signed-infinity propagation, valid finite negative/zero
  sigma, NaN/signed-infinity sigma, `zero * infinity`, and legitimate signed
  infinity from direct IEEE arithmetic.
- Accept legal bounded non-NUL-terminated selectors. Structural validation
  remains observable before the raw-short shortcut.
- Treat NumPy warnings as Python oracle evidence, not a new C++ diagnostic
  channel. Accept existing native reduction order for extreme values.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.
- Do not implement `mar_ratio`, `r_cubed`, `kappa_three_ratio`, adjusted or
  conditional Sharpe, calendar-resampled consecutive statistics, EVT, aliases,
  providers, reports, or plots.

### Task 148: Record Phase 35 design and executable plan

**Files:**

- Create: `docs/design/2026-08-13-phase-35-trading-value-at-risk-design.md`
- Create: `docs/plans/2026-08-13-fincore-cpp-phase-35-trading-value-at-risk.md`

1. Record the route challenge from MAR, already-native `loss_rate`, Python
   export mapping, exact native API, unused-period omission, native
   ndarray/DataFrame mapping, validation/control flow, raw NaN/infinity and
   arithmetic boundaries, oracle, mutation-resistant tests, gates, and
   exclusions.
2. Obtain a fresh independent plan review. Correct every Critical, Important,
   or Minor finding in the same two-file candidate, run diff checks, explicitly
   stage only these paths, and commit:

   ```sh
   git commit -m "docs: plan phase 35 trading value at risk"
   ```

### Task 149: Add native trading value at risk

**Files:**

- Modify: `include/fincore/metrics/risk.hpp`
- Modify: `src/metrics/risk.cpp`
- Modify: `tests/unit/extended_risk_test.cpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`

1. Create the ignored Phase 35 SDD brief, progress ledger, source oracle, RED
   evidence, and implementation report. Run this mandatory oracle from the
   Python source checkout:

   ```sh
   cd /Users/yunjinqi/Documents/new_projects/fincore
   PYTHONDONTWRITEBYTECODE=1 \
   /Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base \
     python - <<'PY'
   import warnings
   import numpy as np
   from fincore.metrics.risk import trading_value_at_risk

   cases = [
       ("ordinary_default", np.array([.01, -.02, .03]), None, 2.),
       ("ordinary_sigma_1", np.array([.01, -.02, .03]), None, 1.),
       ("ordinary_sigma_0", np.array([.01, -.02, .03]), None, 0.),
       ("ordinary_sigma_neg3", np.array([.01, -.02, .03]), None, -3.),
       ("ordinary_sigma_nan", np.array([.01, -.02, .03]), None, np.nan),
       ("ordinary_sigma_pos_inf", np.array([.01, -.02, .03]), None, np.inf),
       ("ordinary_sigma_neg_inf", np.array([.01, -.02, .03]), None, -np.inf),
       ("bad_period_ignored", np.array([.01, -.02, .03]), object(), 1.),
       ("empty_bad_sigma", np.array([], dtype=float), "bad", object()),
       ("single_bad_sigma", np.array([.1]), "bad", object()),
       ("flat_positive", np.array([.1, .1]), None, 2.),
       ("flat_positive_sigma_pos_inf", np.array([.1, .1]), None, np.inf),
       ("flat_zero_sigma_pos_inf", np.array([0., 0.]), None, np.inf),
       ("nan_mixed", np.array([.1, -.1, np.nan]), None, 2.),
       ("pos_inf_mixed", np.array([.1, -.1, np.inf]), None, 2.),
       ("neg_inf_mixed", np.array([.1, -.1, -np.inf]), None, 2.),
       ("both_inf", np.array([.1, -.1, np.inf, -np.inf]), None, 2.),
       ("max_symmetric",
        np.array([np.finfo(float).max, -np.finfo(float).max]), None, 2.),
       ("max_symmetric_sigma_neg2",
        np.array([np.finfo(float).max, -np.finfo(float).max]), None, -2.),
   ]
   for name, values, period, sigma in cases:
       with warnings.catch_warnings(record=True) as caught:
           warnings.simplefilter("always")
           result = trading_value_at_risk(
               values, period=period, sigma=sigma)
       messages = " | ".join(str(item.message) for item in caught) or "-"
       print(f"{name}={result!r}; warnings={len(caught)}; "
             f"messages={messages}")
   PY
   ```

2. Before adding the declaration or definition, add an exact
   `double (*)(const dataframe&, double, std::string_view)` function-pointer
   `static_assert` and a one-argument default call to both registered test
   translation units. Keep the leaf consumer's only fincore include as
   `<fincore/metrics/risk.hpp>`. Force Ninja to continue through both consumer
   compilations and retain a zsh-safe genuine missing-symbol dual-TU RED:

   ```zsh
   red_log=.superpowers/sdd/2026-08-13-fincore-cpp-phase-35-trading-value-at-risk/task-149-red-build.log
   set +e
   cmake --build build --target fincore_tests --parallel 4 -- -k 0 \
     >"$red_log" 2>&1
   red_exit=$?
   set -e
   test "$red_exit" -ne 0
   rg -n 'extended_risk_test\.cpp' "$red_log"
   rg -n 'metrics_umbrella_test\.cpp' "$red_log"
   rg -n 'trading_value_at_risk' "$red_log"
   ```

   The log must show the symbol absent in both translation units and no other
   root cause. Record `red_exit` explicitly; do not use zsh's reserved
   `status` name or infer a pipeline status. Do not run CTest after this failed
   build.
3. Add the exact declaration and minimal definition:
   - reject `column.find('\0')` with `input_error` before any DataFrame call;
   - resolve `require_double_column(returns, column)` next;
   - return NaN for fewer than two raw selected values;
   - compute `detail::mean(values)` once for the output mean stage;
   - compute `detail::sample_variance(values)` once and take its square root;
   - return `average - sigma * standard_deviation` directly.
4. Test exact signature/default use, ordinary Python goldens, independent
   sample-standard-deviation parity, `sigma` values `0`, `-3`, NaN, positive
   infinity, and negative infinity, typed zero/one-row shortcuts, constant
   positive/zero series, `zero * infinity`, raw NaN, raw signed infinity, and
   nonfinite fixtures with at least two finite survivors to kill filtering,
   symmetric maximum finite inputs and signed-infinity preservation,
   default/named columns, embedded NUL, legal bounded nonterminated selector,
   structural validation order, and metrics-facade visibility.
5. Run the following exact current-tree gates, then diff checks. Explicitly
   stage and commit only the four paths:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -N
   ctest --test-dir build -R 'ExtendedRisk|MetricsUmbrella' \
     --output-on-failure --parallel 4
   ctest --test-dir build -R 'Risk|Stats|Dataframe' \
     --output-on-failure --parallel 4
   ctest --test-dir build --output-on-failure --parallel 4
   git diff --check
   git diff --cached --check
   ```

   ```sh
   git commit -m "feat: add native trading value at risk"
   ```

6. Dispatch a fresh specification review and a separate fresh code-quality
   review. Correct every Critical, Important, or Minor finding in new scoped
   commits, rerun focused/full verification, and never amend accepted history.

### Task 150: Accept Phase 35

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-13-phase-35-trading-value-at-risk-acceptance.md`

1. Document the route selection, already-native `loss_rate`, Python export
   mapping, exact native API and unused-period divergence, control flow,
   validation, raw NaN/infinity and native arithmetic boundaries, complete
   oracle including warnings, genuine dual RED, final implementation/review
   chain with exact scopes, and explicit exclusions.
2. Retain raw current build/registration/focused/related/full and fresh Apple
   headless, registration-audited LLVM ASan+UBSan, ImPlot/demo
   compile-link-only/no-GUI, invalid-ImGui multiline diagnostic, and actual
   FetchContent/local dependency provenance evidence. Record actual counts,
   versions, warning families, commands, roots, and statuses; do not copy prior
   phase numbers.
3. Keep both files unstaged for a fresh independent read-only document/gate
   review. Fix every Critical, Important, or Minor issue, then explicitly stage
   only the two files and commit:

   ```sh
   git commit -m "docs: accept phase 35 trading value at risk"
   ```

### Task 151: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-13-phase-35-trading-value-at-risk-acceptance.md`

1. Record Task 149's complete implementation/fix chain and exact scopes plus
   Task 150's initial acceptance SHA/scope. State Task 151 is documentation-
   only, does not amend prior commits, and neither states nor anticipates its
   own SHA.
2. Run the exact focused command and diff checks; explicitly stage only the
   acceptance document and commit:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'ExtendedRisk|MetricsUmbrella' \
     --output-on-failure --parallel 4
   git diff --check
   ```

   ```sh
   git commit -m "docs: trace phase 35 trading value at risk"
   ```

### Final Phase 35 review

Audit all Phase 35 ancestry/scopes; exact API/defaults; one native symbol;
unused-period omission; NUL/genuine-double/raw-length ordering; sample rather
than population variance; no filtering/annualization/clamp; sigma and raw
NaN/infinity/overflow branches; facade visibility; no CMake/dependency drift;
tests/provenance/no-GUI wording; and non-self-referential trace. Configure one
unique fresh AppleClang headless root, build it completely, audit registration
with `ctest -N`, then run the exact focused
`-R 'ExtendedRisk|MetricsUmbrella'` selection and full CTest from that root.
Rerun the mandatory Anaconda source oracle, then publish separate Spec and
Task-Quality verdicts. Correct every Critical, Important, or Minor finding in
new scoped commits.
