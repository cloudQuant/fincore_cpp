# Phase 31 Native Rolling Style Tilts Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to
> implement this plan task-by-task.

**Goal:** Add a strict DataFrame-native counterpart to Python
`fincore.attribution.style.calculate_style_tilts`.

**Architecture:** Extend the accepted style leaf with one typed rolling API.
The implementation validates and owns the tabular boundary, then calculates
exclusionary positional size, momentum, value, and volatility flags while
copying the trailing raw timestamp index.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, standard
library checked arithmetic, mandatory Anaconda Python oracle, Homebrew LLVM 22
ASan+UBSan, and optional local ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 30 ends at
`6e17d7bc6140ac7a362e3303f123fa1167eafde0` with `407` headless tests. Do not
amend any Phase 30 plan, implementation, acceptance, trace, correction, or
ignored evidence.

## Global constraints

- Use `fincore::dataframe`, `fincore::input_error`, existing private style
  patterns, and the C++ standard library only. Do not embed Python or add
  NumPy/SciPy, a solver, provider, network call, cache, plot source, or new
  dependency.
- Add exactly `calculate_style_tilts(const dataframe&, std::size_t = 252U)`.
  Do not expose or silently accept Python's unused `factor_returns` parameter.
- Treat the current input row as excluded. Preserve exact average-tie rank,
  momentum, sample-volatility, median, threshold, label-direction, and
  four-block output-column semantics from the design.
- Copy the raw timestamp suffix in positional order. Preserve duplicate and
  nonmonotonic labels, but never reproduce pandas `.loc` broadcast overwrite.
- Empty-shaped input, `window < 2`, or fewer than three observations returns a
  canonical `0x0` frame before nonempty validation.
- Strict nonempty computation requires exact genuine-double/finite structure,
  safe names and derived names, finite checked arithmetic, representable
  output, and value ownership. All public failures translate to `input_error`.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.
- Do not implement regression attribution, factor fetching/providers, Python
  mapping conveniences, reports, packaging/release work, or GUI runtime.

### Task 132: Record Phase 31 design and executable plan

**Files:**

- Create: `docs/design/2026-08-12-phase-31-style-tilts-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-31-style-tilts.md`

1. Record source alternatives, exact API, dead-parameter exclusion, window and
   history semantics, rank ties, compounding, sample volatility, raw index and
   column layout, deliberate duplicate-label/empty/nonfinite native policies,
   oracle and its public-observability limits, TDD matrix, gates, and
   exclusions.
2. Obtain a fresh independent plan review, correct every finding, run diff
   checks, explicitly stage only these two documents, and commit:

   ```sh
   git commit -m "docs: plan phase 31 style tilts"
   ```

### Task 133: Add native rolling style tilts

**Files:**

- Modify: `include/fincore/attribution/style.hpp`
- Modify: `src/attribution/style.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/style_tilts_test.cpp`
- Create: `tests/unit/attribution_style_tilts_umbrella_test.cpp`

1. Create the ignored Phase 31 SDD brief, ledger, oracle, RED log, and report.
   Run this mandatory oracle from the Python source checkout:

   ```sh
   cd /Users/yunjinqi/Documents/new_projects/fincore
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import pandas as pd
   from fincore.attribution.style import calculate_style_tilts
   returns = pd.DataFrame({
       "A": [0., .25, -.5, 0., .75],
       "B": [0., 0., -.5, -.5, .75],
       "C": [0., -.5, 1., .25, .75],
   }, index=pd.Index([42, -7, 100, 3, 55], name="raw"))
   result = calculate_style_tilts(returns, window=3)
   print("INPUT", returns.to_dict("list"))
   print("INDEX", result.index.tolist())
   print("COLUMNS", result.columns.tolist())
   print("VALUES", result.values.tolist())
   print("WINDOW_0", calculate_style_tilts(returns, window=0).shape)
   print("WINDOW_1", calculate_style_tilts(returns, window=1).shape)
   print("SHORT", calculate_style_tilts(returns.iloc[:2]).shape)
   print("EMPTY", calculate_style_tilts(pd.DataFrame()).shape)
   '
   ```

2. Register both consumers before adding production declarations. The leaf
   test includes only `<fincore/attribution/style.hpp>` among fincore headers;
   the facade test includes only `<fincore/attribution.hpp>`. Build and retain
   the genuine missing-declaration compile RED; do not run stale CTest.
3. Add the exact declaration and minimal implementation:
   - safely derive `effective_window` and documented empty short-circuits;
   - reuse accepted whole-frame/name/finite validation patterns;
   - preflight eight phase-major derived names per asset and output capacity;
   - compute stable one-based average ranks for exact ties;
   - compute exclusionary checked trailing compounding and checked Welford
     `ddof=1` volatility with linear cross-sectional median;
   - append binary double columns in exact four-block order and copy the raw
     timestamp suffix positionally;
   - translate allocation, length, malformed, and arithmetic failures to
     `input_error`.
4. Test exact signature/default call and absence of the dead parameter; the
   full Python oracle and column order; current-row exclusion; average ties and
   exact N/3, N/2, 2N/3 boundaries; trailing momentum and zero-as-loser;
   last-60 volatility ordering/median equality plus implementation-level
   review of the source-mandated checked `ddof=1` formula, whose binary public
   output cannot distinguish a common positive rescaling; `window=0/1`,
   short/default and
   empty-shaped surfaces; duplicate/nonmonotonic raw labels with distinct
   positional results; exact total schema/types/count; malformed/mixed/short,
   name/capacity, nonfinite/arithmetic and legally testable representability
   boundaries; source mutation/destruction ownership; facade visibility.
5. Run build; focused `StyleTilts|AttributionStyleTiltsUmbrella`; related
   `StyleAnalysis|StylePerformance|StyleTilts|Attribution|Dataframe`; full
   CTest; and diff checks. Explicitly stage and commit only the five paths:

   ```sh
   git commit -m "feat: add native rolling style tilts"
   ```

6. Dispatch fresh specification review followed by fresh code-quality review.
   Correct every Critical, Important, or Minor finding in new scoped commits,
   rerun focused/full verification, and never amend accepted history.

### Task 134: Accept Phase 31

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-31-style-tilts-acceptance.md`

1. Document exact API/source mapping, dead-parameter exclusion, positional
   history/index policy, formulas/schema/ownership, native divergences,
   complete oracle, genuine dual-consumer RED, final implementation/review
   chain and exact scopes, and exclusions.
2. Retain raw current focused/full and fresh Apple headless, registration-
   audited LLVM ASan+UBSan, ImPlot/demo compile/link-only/no-GUI, invalid-ImGui
   multiline diagnostic, and actual dependency provenance evidence.
3. Keep both files unstaged for a fresh independent read-only document/gate
   review. Fix every Critical, Important, or Minor issue, then explicitly stage
   only the two files and commit:

   ```sh
   git commit -m "docs: accept phase 31 style tilts"
   ```

### Task 135: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-31-style-tilts-acceptance.md`

1. Record Task 133's complete implementation/fix chain and exact scopes plus
   Task 134's initial acceptance SHA/scope. State Task 135 is documentation-
   only, does not amend prior commits, and neither states nor anticipates its
   own SHA.
2. Run focused Style Tilts/facade CTest and diff checks; explicitly stage only
   the acceptance document and commit:

   ```sh
   git commit -m "docs: trace phase 31 style tilts"
   ```

### Final Phase 31 review

Audit all Phase 31 ancestry/scopes; API and default; dead parameter exclusion;
empty/window/history behavior; exact average ranks, momentum, sample
volatility and labels; raw suffix/duplicate positional semantics; finite/name/
capacity/error boundaries; exact schema/order/index/ownership; leaf/facade
proof; raw gates/provenance/no-GUI wording; and non-self-referential trace.
Fresh-configure Apple headless, rerun focused/full CTest and the mandatory
Anaconda oracle, then publish Spec/Task-Quality verdicts. Correct every
Critical, Important, or Minor finding in new scoped commits.
