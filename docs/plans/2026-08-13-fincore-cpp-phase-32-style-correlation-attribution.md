# Phase 32 Native Style Correlation Attribution Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to
> implement this plan task-by-task.

**Goal:** Add a strict DataFrame-native counterpart to Python
`fincore.attribution.style.calculate_regression_attribution` while preserving
its Pearson-correlation formula rather than inventing an OLS fit.

**Architecture:** Extend the accepted style leaf with an explicit
timestamp-aligned core and a one-argument automatic composer. Replace Python's
unread exposure frame with a bounded style-name selector, materialize a stable
two-column attribution result, and keep all validation, arithmetic, and
ownership native.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, standard
library checked arithmetic, mandatory Anaconda Python oracle, Homebrew LLVM 22
ASan+UBSan, and optional local ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 31 ends at
`536660b0708d36a98c2527c2a6cc02bf6f6faff4` with `419` headless tests. Do not
amend any prior plan, implementation, acceptance, correction, trace, or
ignored evidence.

## Global constraints

- Use `fincore::dataframe`, `fincore::input_error`, accepted public style
  composition, existing private style patterns, and the C++ standard library
  only. Do not embed Python or add NumPy/SciPy, a solver, provider, network
  call, cache, plot source, or new dependency.
- Add exactly the two overloads and two constants in the design. The explicit
  overload averages all portfolio double columns row-wise and accepts only a
  style-name selector, not Python's unread exposure values.
- Preserve the source Pearson-correlation contribution formula, stable style
  order, at-least-three rule, strict population-std `<1e-15` boundary,
  common-only style mean, full-portfolio residual mean, and residual-last
  output.
- Explicit mode requires unique timestamps and stable left-order intersection;
  automatic mode composes public style analysis/performance once each and
  pairs rows positionally, retaining duplicate/nonmonotonic raw input labels.
- Apply the documented canonical empty, residual-only, strict finite/name/
  structure/capacity/arithmetic/error, and value-ownership policies.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.
- Do not implement factor regression, factor fetching/providers, Python
  mapping conveniences, unrelated scalar parity debt, reports,
  packaging/release work, or GUI runtime.

### Task 136: Record Phase 32 design and executable plan

**Files:**

- Create: `docs/design/2026-08-13-phase-32-style-correlation-attribution-design.md`
- Create: `docs/plans/2026-08-13-fincore-cpp-phase-32-style-correlation-attribution.md`

1. Record the route decision and its non-global use of “smallest”; source
   misnaming versus exact correlation formula; both APIs; selector/dead-data
   decision; explicit and automatic alignment; schema/order; empty, finite,
   duplicate-label, capacity, error and ownership boundaries; source oracle;
   mutant-resistant TDD matrix; gates; and exclusions.
2. Obtain a fresh independent plan review, correct every Critical, Important,
   or Minor finding, run diff checks, explicitly stage only these two
   documents, and commit:

   ```sh
   git commit -m "docs: plan phase 32 style correlation attribution"
   ```

### Task 137: Add native style correlation attribution

**Files:**

- Modify: `include/fincore/attribution/style.hpp`
- Modify: `src/attribution/style.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/style_regression_attribution_test.cpp`
- Create: `tests/unit/attribution_style_regression_attribution_umbrella_test.cpp`

1. Create the ignored Phase 32 SDD brief, ledger, oracle, RED log, and report.
   Run this mandatory oracle from the Python source checkout:

   ```sh
   cd /Users/yunjinqi/Documents/new_projects/fincore
   /Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base python - <<'PY'
   import numpy as np
   import pandas as pd
   from fincore.attribution.style import calculate_regression_attribution

   portfolio = pd.Series([.03, .01, .05, .02, .04],
                         index=[30, 10, 50, 20, 40])
   styles = pd.DataFrame({
       "growth": [.08, .04, .10, .02, .06],
       "value": [.02, .04, .01, .05, .03],
       "unselected": [9., 8., 7., 6., 5.],
   }, index=[40, 20, 50, 10, 30])
   exposures = pd.DataFrame({"value": [np.nan], "growth": [np.inf]},
                            index=["ignored"])
   portfolio_frame = pd.DataFrame({
       "left": [.02, .04, .06, .08, .10],
       "right": [.04, .06, .08, .10, .12],
   }, index=portfolio.index)
   assets = pd.DataFrame({
       "ALPHA": [.01, -.02, .03, 0., -.01],
       "BETA": [.02, .01, -.01, .04, 0.],
       "GAMMA": [-.01, .03, .02, -.02, .05],
   }, index=[42, -7, 42, 100, 3])

   print("EXPLICIT", calculate_regression_attribution(
       portfolio, styles, exposures))
   print("EXPLICIT_FRAME", calculate_regression_attribution(
       portfolio_frame, styles, exposures))
   print("AUTOMATIC", calculate_regression_attribution(assets))
   print("SHORT", calculate_regression_attribution(
       portfolio,
       pd.DataFrame({"growth": [.5, .7]}, index=[30, 10]),
       pd.DataFrame({"growth": [1.]})))
   PY
   ```

2. Register both consumers before adding production declarations. The leaf
   test includes only `<fincore/attribution/style.hpp>` among fincore headers;
   the facade test includes only `<fincore/attribution.hpp>`. Build and retain
   the genuine missing-declaration/constants compile RED; do not run stale
   CTest.
3. Add the exact constants/declarations and minimal implementation:
   - copy/validate selector views before any C-string DataFrame boundary;
   - canonical-empty explicit/automatic portfolio surfaces;
   - validate and row-average the complete explicit portfolio frame;
   - require unique explicit indexes and collect common pairs in portfolio
     left order without changing style-return column order;
   - implement checked centered population moments/Pearson correlation,
     at-least-three and strict threshold branches, common style mean,
     contributions, full portfolio mean and residual;
   - automatically call public `style_analysis` once and explicit public
     `analyze_performance_by_style` once, mapping suffixed input names back to
     unsuffixed styles and pairing positionally;
   - materialize exact value-owning ordinal/string/double output and translate
     allocation, length, malformed, capacity, duplicate and arithmetic
     failures to `input_error`.
4. Test exact overload signatures and one-argument call; absence of an exposure
   DataFrame overload; explicit Series-shaped and multi-column oracles;
   centered Pearson versus uncentered dot product and OLS slope; reordered and
   partial intersections; fewer-than-three, constant, strict threshold,
   common-style-mean and full-residual-mean branches; selector filter versus
   style-return order, missing/empty/reserved/duplicate/NUL selectors;
   selected versus unselected malformed columns; duplicate index, wrong type,
   short column, nonfinite and checked-overflow failures; empty/residual-only
   surfaces; exact schema/order/types/index; selector/input mutation and scope
   ownership; automatic full oracle, public composition, raw duplicate labels,
   and facade visibility.
5. Run build; focused
   `StyleRegressionAttribution|AttributionStyleRegressionAttributionUmbrella`;
   related
   `StyleAnalysis|StylePerformance|StyleTilts|StyleRegressionAttribution|Attribution|Stats|Dataframe`;
   full CTest; and diff checks. Explicitly stage and commit only the five paths:

   ```sh
   git commit -m "feat: add native style correlation attribution"
   ```

6. Dispatch fresh specification review followed by fresh code-quality review.
   Correct every Critical, Important, or Minor finding in new scoped commits,
   rerun focused/full verification, and never amend accepted history.

### Task 138: Accept Phase 32

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-13-phase-32-style-correlation-attribution-acceptance.md`

1. Document route selection, exact APIs/source mapping, correlation-not-OLS
   formula, selector replacement, explicit/automatic alignment, formulas,
   schema/order/ownership, native divergences, complete oracle, genuine dual
   RED, final implementation/review chain and exact scopes, and exclusions.
2. Retain raw current focused/full and fresh Apple headless,
   registration-audited LLVM ASan+UBSan, ImPlot/demo compile/link-only/no-GUI,
   invalid-ImGui multiline diagnostic, and actual FetchContent/local
   dependency provenance evidence.
3. Keep both files unstaged for a fresh independent read-only document/gate
   review. Fix every Critical, Important, or Minor issue, then explicitly stage
   only the two files and commit:

   ```sh
   git commit -m "docs: accept phase 32 style correlation attribution"
   ```

### Task 139: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-13-phase-32-style-correlation-attribution-acceptance.md`

1. Record Task 137's complete implementation/fix chain and exact scopes plus
   Task 138's initial acceptance SHA/scope. State Task 139 is documentation-
   only, does not amend prior commits, and neither states nor anticipates its
   own SHA.
2. Run focused style-correlation/facade CTest and diff checks; explicitly stage
   only the acceptance document and commit:

   ```sh
   git commit -m "docs: trace phase 32 style correlation attribution"
   ```

### Final Phase 32 review

Audit all Phase 32 ancestry/scopes; exact overloads/constants; selector and
dead exposure-data decision; explicit multi-column portfolio and stable
alignment; automatic one-time public composition; Pearson-not-OLS formula;
at-least-three, threshold, common mean and full residual; empty/finite/index/
name/capacity/error boundaries; schema/order/index/ownership; leaf/facade
proof; raw gates/provenance/no-GUI wording; and non-self-referential trace.
Fresh-configure Apple headless, rerun focused/full CTest and the mandatory
Anaconda oracle, then publish Spec/Task-Quality verdicts. Correct every
Critical, Important, or Minor finding in new scoped commits.
