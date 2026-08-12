# Phase 34 Native VaR Excess Return Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to
> implement this plan task-by-task.

**Goal:** Add the smallest useful dependency-closed Plan A parity slice by
composing accepted native annual-return, annualization, and historical-VaR
leaves into `var_excess_return`.

**Architecture:** Add one scalar declaration to the existing risk leaf and one
definition that preserves source ordering after native DataFrame validation.
Reuse the existing risk and metrics-facade tests; add no source target, facade
include, or dependency.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, mandatory
Anaconda Python source oracle, Homebrew LLVM 22 ASan+UBSan, and optional local
ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 33 ends at
`fcfa5baa5abc4f58122ac2785128280f64af2dcb` with `439` headless tests and a
clean tracked worktree. Do not amend any prior design, plan, implementation,
test hardening, acceptance, trace, or ignored evidence.

## Global constraints

- Use `fincore::dataframe`, `fincore::input_error`, public
  `annualization_factor`, public `annual_return`, public
  `historical_value_at_risk`, and the C++ standard library only. Do not embed
  Python or add a dependency, source target, provider, network call, solver,
  plot source, report, package, or GUI runtime.
- Add exactly one native symbol in `metrics/risk.hpp`. Do not add a second
  overload, multi-column API, alias, or facade include.
- Reject embedded NUL before every DataFrame C-string boundary; validate the
  genuine-double selected column before the raw `<2` shortcut; then preserve
  source call order and call each accepted public numerical leaf exactly once.
- Preserve cutoff endpoints, raw-short behavior, invalid-frequency ordering,
  explicit annualization override, zero/NaN VaR, nonfinite returns, and direct
  IEEE risk-free/result arithmetic from the design.
- In valid multi-row calls, inherit accepted native `annual_return` behavior
  for zero, negative, NaN, and signed-infinity annualization overrides. Do not
  add a positivity or finite-only domain check to imitate Python's intermediate
  `num_years` division.
- Legal bounded non-NUL-terminated selectors must remain supported.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.
- Do not implement `mar_ratio`, adjusted/conditional Sharpe, weekly/monthly
  consecutive statistics, EVT, aliases, providers, reports, or plots.

### Task 144: Record Phase 34 design and executable plan

**Files:**

- Create: `docs/design/2026-08-13-phase-34-var-excess-return-design.md`
- Create: `docs/plans/2026-08-13-fincore-cpp-phase-34-var-excess-return.md`

1. Record the route audit, already-native `loss_rate`, exact API, accepted leaf
   composition, validation/call order, cutoff/annualization/IEEE behavior,
   oracle, mutation-resistant tests, gates, and exclusions.
2. Obtain a fresh independent plan review. Correct every Critical, Important,
   or Minor finding in the same two-file candidate, run diff checks, explicitly
   stage only these paths, and commit:

   ```sh
   git commit -m "docs: plan phase 34 var excess return"
   ```

### Task 145: Add native VaR excess return

**Files:**

- Modify: `include/fincore/metrics/risk.hpp`
- Modify: `src/metrics/risk.cpp`
- Modify: `tests/unit/extended_risk_test.cpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`

1. Create the ignored Phase 34 SDD brief, progress ledger, source oracle, RED
   evidence, and implementation report. Run this mandatory oracle from the
   Python source checkout:

   ```sh
   cd /Users/yunjinqi/Documents/new_projects/fincore
   /Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base python - <<'PY'
   import numpy as np
   import pandas as pd
   from fincore.metrics.risk import var_excess_return

   returns = pd.Series([.10, -.05, .02, -.01], dtype=float)
   endpoint_returns = pd.Series([.10, -.20, .03], dtype=float)
   cases = {
       "daily_default": lambda: var_excess_return(returns),
       "weekly": lambda: var_excess_return(returns, period="weekly"),
       "explicit_7": lambda: var_excess_return(
           returns, period="daily", annualization=7.0),
       "bad_period_explicit_7": lambda: var_excess_return(
           returns, period="bad", annualization=7.0),
       "cutoff_0": lambda: var_excess_return(endpoint_returns, cutoff=0.0),
       "cutoff_1": lambda: var_excess_return(endpoint_returns, cutoff=1.0),
       "zero_var": lambda: var_excess_return(pd.Series([0., 0.])),
       "nan_return": lambda: var_excess_return(pd.Series([-.1, np.nan])),
       "pos_inf_return": lambda: var_excess_return(pd.Series([-.1, np.inf])),
       "bad_cutoff": lambda: var_excess_return(returns, cutoff=-.1),
       "risk_free_pos_inf": lambda: var_excess_return(
           returns, risk_free=np.inf),
       "one_row_bad_everything": lambda: var_excess_return(
           pd.Series([.1]), cutoff=-.1, risk_free=np.inf,
           period="bad", annualization=-3.0),
   }
   for name, call in cases.items():
       print(name, repr(call()))

   for name, kwargs in {
       "bad_period": {"period": "bad"},
       "bad_period_bad_cutoff": {"period": "bad", "cutoff": -.1},
   }.items():
       try:
           value = var_excess_return(returns, **kwargs)
       except Exception as error:
           print(name, type(error).__name__, str(error))
       else:
           print(name, repr(value))

   for annualization in [0.0, -3.0, np.nan, np.inf, -np.inf]:
       try:
           value = var_excess_return(returns, annualization=annualization)
       except Exception as error:
           print("annualization", repr(annualization),
                 type(error).__name__, str(error))
       else:
           print("annualization", repr(annualization), repr(value))
   PY
   ```

2. Before adding the declaration or definition, add leaf and facade compile
   uses to the two registered tests. Remove the leaf test's direct
   `<fincore/error.hpp>` include so `<fincore/metrics/risk.hpp>` is its only
   fincore header. Build and retain the genuine missing-symbol dual-TU compile
   RED. Do not run CTest after the failed build.
3. Add the exact declaration and minimal definition:
   - reject `column.find('\0')` with `input_error` before any DataFrame call;
   - resolve `require_double_column(returns, column)` next;
   - return NaN for fewer than two raw selected values;
   - call `annualization_factor(period, annualization)` once;
   - call `annual_return(returns, factor, column)` once;
   - call `historical_value_at_risk(returns, cutoff, column)` once;
   - return NaN for zero/NaN VaR or NaN annual return; and
   - otherwise return the direct quotient without finite clamping.
4. Test exact signature/default use, oracle goldens, direct public-leaf parity,
   weekly/explicit annualization, invalid frequency with/without override,
   invalid-frequency-before-cutoff ordering, cutoff endpoints, zero/one raw
   row shortcut, zero/invalid VaR, NaN and signed-infinity returns, nonfinite
   risk-free arithmetic, preserved positive infinity, full-length native
   annualization overrides `0`, `-3`, NaN, `+infinity`, and `-infinity`,
   named/default columns, embedded NUL, bounded nonterminated selector,
   structural validation order, and facade visibility.
5. Run build; focused `ExtendedRisk|MetricsUmbrella`; related
   `Risk|Returns|Yearly|Basic|PerfStats`; full CTest; and diff checks. Explicitly
   stage and commit only the four paths:

   ```sh
   git commit -m "feat: add native var excess return"
   ```

6. Dispatch a fresh specification review and a separate fresh code-quality
   review. Correct every Critical, Important, or Minor finding in new scoped
   commits, rerun focused/full verification, and never amend accepted history.

### Task 146: Accept Phase 34

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-13-phase-34-var-excess-return-acceptance.md`

1. Document the route selection, already-native `loss_rate`, exact API/source
   mapping, public composition, validation/call order, cutoff/annualization/
   IEEE behavior, complete oracle, genuine dual RED, final implementation and
   review chain with exact scopes, and explicit exclusions.
2. Retain raw current focused/related/full and fresh Apple headless,
   registration-audited LLVM ASan+UBSan, ImPlot/demo compile/link-only/no-GUI,
   invalid-ImGui multiline diagnostic, and actual FetchContent/local dependency
   provenance evidence.
3. Keep both files unstaged for a fresh independent read-only document/gate
   review. Fix every Critical, Important, or Minor issue, then explicitly stage
   only the two files and commit:

   ```sh
   git commit -m "docs: accept phase 34 var excess return"
   ```

### Task 147: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-13-phase-34-var-excess-return-acceptance.md`

1. Record Task 145's complete implementation/fix chain and exact scopes plus
   Task 146's initial acceptance SHA/scope. State Task 147 is documentation-
   only, does not amend prior commits, and neither states nor anticipates its
   own SHA.
2. Run focused risk/facade CTest and diff checks; explicitly stage only the
   acceptance document and commit:

   ```sh
   git commit -m "docs: trace phase 34 var excess return"
   ```

### Final Phase 34 review

Audit all Phase 34 ancestry/scopes; exact API/defaults; one native symbol;
NUL/genuine-double/raw-length/call ordering; each public leaf exactly once;
cutoff endpoints and invalid cases; annualization override; raw-short,
zero/NaN/infinity branches; facade visibility; no CMake/dependency drift;
tests/provenance/no-GUI wording; and non-self-referential trace. Fresh-configure
Apple headless, rerun focused/full CTest and the mandatory Anaconda oracle, then
publish Spec/Task-Quality verdicts. Correct every Critical, Important, or Minor
finding in new scoped commits.
