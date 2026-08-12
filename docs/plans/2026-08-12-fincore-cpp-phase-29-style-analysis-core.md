# Phase 29 Native Style-Analysis Core Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a DataFrame-native, value-owning counterpart to Python's
deterministic `fincore.attribution.style.style_analysis` leaf.

**Architecture:** Add one attribution leaf that treats DataFrame rows as time
and genuine double columns as named asset-return series. It materializes three
independent DataFrames: ordinal/name keyed style exposures, ordinal/name keyed
style summary returns, and a raw-index-preserving equal-weighted return series.
The attribution façade exports the leaf only after its implementation passes
independent review.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, accepted
`detail::linear_percentile`, mandatory Anaconda Python source oracle, Homebrew
LLVM 22 ASan+UBSan, and optional local ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 28 ends at
`983ce00697115515428f6dc412aed0ae4fa6a2f9` with 388 headless CTest cases.
Do not amend its leaf, acceptance, trace, or ignored review records.

## Global constraints

- Use only `fincore::dataframe`, `fincore::input_error`, accepted
  `detail::linear_percentile`, cloudQuant/DataFrame, and the C++ standard
  library. Do not add Python/NumPy/SciPy embedding, an optimizer, a model
  class, cache, remote provider, callback, random engine, plot source, or a
  non-local dependency.
- The public leaf uses only the exact `style_analysis_options`,
  `style_analysis_result`, and `style_analysis` API in the design. Optional
  characteristics are bounded non-owning input spans consumed during the call;
  all result DataFrames must own their values.
- Input is time-by-asset. Require a nonempty raw index, at least one and only
  genuine double asset column, exact column/index lengths, finite asset
  returns and supplied characteristic spans, valid `[0,1]` quantiles, and
  NUL-free asset names. Translate allocation/length failures to `input_error`.
- Preserve source algorithm order: size (`large,mid,small` or
  `equal_weight`), momentum (`winner,loser`), value (`value,growth`), then
  volatility (`high,low`). Source-generated empty-style summary returns remain
  NaN; nonfinite input is rejected rather than silently skipped.
- Map pandas Series characteristics positionally to the stable input double
  column order; `value_scores` takes precedence over `book_to_price`.
- Use ordinal timestamp indexes for string-keyed exposure/summary outputs, and
  preserve raw input index exactly for `overall_returns`. Do not sort,
  deduplicate, align, or retain input DataFrame views.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`, never bare
  `python` or `python3`.
- Do not implement style tilts, regression attribution, performance-by-style,
  factor fetching, optimization, EVT/GARCH, reports, or GUI runtime behavior.

### Task 124: Record Phase 29 design and executable plan

**Files:**

- Create: `docs/design/2026-08-12-phase-29-style-analysis-core-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-29-style-analysis-core.md`

1. Record the deterministic-leaf scope decision, rejected solver/fitting and
   provider alternatives, exact API, positional-characteristic mapping,
   DataFrame schemas/indexes, source ordering/rolling-volatility semantics,
   finite boundary, source oracle, test matrix, gates, and exclusions.
2. Run `git diff --check`; stage exactly these two documents; run cached diff
   check; commit:

   ```sh
   git commit -m "docs: plan phase 29 style analysis core"
   ```

### Task 125: Add the native style-analysis leaf

**Files:**

- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `include/fincore/attribution/style.hpp`
- Create: `src/attribution/style.cpp`
- Create: `tests/unit/style_analysis_test.cpp`

1. Create the ignored Phase 29 SDD workspace, brief, ledger, and task report.
   Run and retain this source oracle from
   `/Users/yunjinqi/Documents/new_projects/fincore`:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import pandas as pd
   from fincore.attribution.style import style_analysis
   n = 60
   returns = pd.DataFrame({
       "ALPHA": [0.010 if i % 3 == 0 else -0.004 for i in range(n)],
       "BETA": [-0.006 if i % 4 == 0 else 0.003 for i in range(n)],
       "GAMMA": [0.002 + 0.0005 * ((i % 5) - 2) for i in range(n)],
   })
   result = style_analysis(
       returns,
       market_caps=pd.Series({"ALPHA": 100., "BETA": 40., "GAMMA": 10.}),
       value_scores=pd.Series({"ALPHA": .2, "BETA": .8, "GAMMA": .5}),
       momentum_window=5,
       size_quantiles=[.3, .7],
   )
   print("EXPOSURES", result.exposures.to_dict())
   print("STYLE_RETURNS", result.returns_by_style["return"].to_dict())
   print("OVERALL", result.overall_returns.tolist())
   '
   ```

2. Before adding any production declaration, create/register a leaf-only
   public consumer with only `<fincore/attribution/style.hpp>` among fincore
   headers. It names the planned types/function and constructs a typed frame.
   Run `cmake --build build --parallel 4`; retain the genuine missing-header
   RED and do not run CTest after this failed build.
3. Add exact public column-name constants, `style_analysis_options`,
   `style_analysis_result`, and the planned function declaration. The header
   must be self-contained and expose no Python types or opaque mutable state.
4. Implement the full deterministic source leaf:

   - obtain stable input double-column names/order, preflight structure and
     finite inputs/options before output allocation, and copy bounded selector
     values when materialization needs them;
   - build normalized size, momentum, value, and rolling-volatility masks in
     the exact documented order; use `detail::linear_percentile` for size and
     cross-sectional medians, and rolling width 60/minimum 20/population
     standard deviation for volatility;
   - build ordinal/indexed exposure and style-summary tables with explicit
     `asset`, `style`, and `return` strings, retaining NaN only for zero-weight
     style summary rows;
   - build the raw-index-preserving equal-weight return DataFrame; do not
     mutate inputs or return a span/view into them;
   - translate only allocation/length failures to `input_error`.

5. Add public tests without private production helpers:

   - exact API/defaults/constants and source oracle: full exposure schema/order
     and weights, style return values, and raw-index overall values;
   - no-caps/no-score default path, book-to-price path, and value-score
     precedence over book-to-price;
   - duplicate/nonmonotonic raw indexes, ordinal string-table indexes,
     short-history source-compatible zero-weight/NaN style summaries, and
     value ownership after input mutation/destruction;
   - no rows/columns, mixed/non-double/malformed input, NaN/+/-infinite return
     or optional characteristics, mismatched spans, invalid quantiles, and
     embedded-NUL asset names as `input_error` boundaries.

6. Run:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R StyleAnalysis --output-on-failure
   ctest --test-dir build -R 'StyleAnalysis|FactorAttribution|Dataframe|Attribution' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

7. Commit exactly the five owned paths:

   ```sh
   git add CMakeLists.txt tests/CMakeLists.txt \
           include/fincore/attribution/style.hpp \
           src/attribution/style.cpp tests/unit/style_analysis_test.cpp
   git commit -m "feat: add native style analysis core"
   ```

8. Dispatch fresh independent read-only spec and code-quality reviews. Correct
   every Critical, Important, or Minor finding in new scoped commits and rerun
   focused/full CTest; never amend accepted history.

### Task 126: Export and accept the attribution-façade leaf

**Files:**

- Modify: `README.md`
- Modify: `include/fincore/attribution.hpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/attribution_style_analysis_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-12-phase-29-style-analysis-core-acceptance.md`

1. Create/register a consumer with only `<fincore/attribution.hpp>` among
   fincore headers. Before adding the one style leaf include, build it and
   retain the genuine missing-export RED; do not run stale CTest.
2. Add only `#include <fincore/attribution/style.hpp>` to the façade. The
   consumer proves exact types/defaults, ordinal string-table schemas, raw
   index retention, representative source endpoints, and no hidden extra
   columns through the façade.
3. Document source mapping, positional spans, output schema, short-history
   NaN policy, oracle, RED/GREEN evidence, ownership, reviews, and exclusions.
   Keep the acceptance document free of Task 126's SHA until Task 127.
4. Retain raw `tee` logs for current focused/full CTest, fresh Apple headless,
   registration-audited Homebrew LLVM ASan+UBSan, ImPlot/demo compile/link-only
   with build-root executable checked but never launched, invalid-ImGui
   multiline diagnostic, and actual GoogleTest FetchContent provenance. Use a
   unique retry if an attempted gate has corrupt/time-sliced CTest evidence.
5. Dispatch a fresh independent read-only document/gate review. Correct every
   Critical, Important, or Minor finding before committing exactly:

   ```sh
   git add README.md include/fincore/attribution.hpp tests/CMakeLists.txt \
           tests/unit/attribution_style_analysis_umbrella_test.cpp \
           docs/acceptance/2026-08-12-phase-29-style-analysis-core-acceptance.md
   git commit -m "docs: accept phase 29 style analysis core"
   ```

### Task 127: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-29-style-analysis-core-acceptance.md`

1. Record Task 125's complete implementation/fix chain, Task 126's direct
   acceptance SHA, and exact scopes. State Task 127 is documentation-only and
   does not amend earlier commits; do not mention or predict its own SHA.
2. Run focused `StyleAnalysis|AttributionStyleAnalysisUmbrella` CTest and
   diff checks, then commit only this document:

   ```sh
   git commit -m "docs: trace phase 29 style analysis core"
   ```

### Final Phase 29 review

Audit Task 125/126/127 scope and ancestry; public API/defaults/constants;
exact source-to-DataFrame positional mapping; output indexes/schema/order;
size/momentum/value/volatility semantics; short-history NaN summaries;
ownership; finite/overflow error boundaries; façade proof; raw gates,
FetchContent provenance, no-GUI claims, and trace wording. Fresh-configure
Apple headless, rerun focused/full CTest and the mandatory Anaconda oracle,
then publish Spec/Task-Quality verdicts. Correct every Critical, Important, or
Minor finding in new scoped commits; do not amend accepted history.
