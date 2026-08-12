# Phase 30 Native Performance-by-Style Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a DataFrame-native, value-owning counterpart to Python
`fincore.attribution.style.analyze_performance_by_style`.

**Architecture:** Extend the accepted Style Analysis leaf with explicit and
automatic overloads. The explicit path aligns Phase 29's ordinal/string
exposure table to named return columns, normalizes each style, and materializes
an ordinal per-period return table; the automatic path composes public
`style_analysis` once and delegates.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, accepted
Style Analysis public API, mandatory Anaconda Python oracle, Homebrew LLVM 22
ASan+UBSan, and optional local ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 29 ends at
`5650313a7cd2da0ffc8f4f60f1576b975b9e3d51` with `397` headless tests.
Do not amend Phase 29 implementation, acceptance, trace, or ignored evidence.

## Global constraints

- Use `fincore::dataframe`, `fincore::input_error`, the accepted public
  `style_analysis`, and the C++ standard library only. Do not embed Python or
  add NumPy/SciPy, a solver, provider, network call, cache, plot source, or new
  dependency.
- Add exactly the two overloads in the design. Do not add an options object or
  inherit Python's unused arguments.
- Treat returns as time-by-asset and explicit exposures as Phase 29's
  `asset` string identity plus stable double style columns. Align by exact
  asset name; missing assets get zero and extras are ignored.
- Preserve finite negative exposure weights when their aligned total is
  positive. Normalize only positive totals; zero/negative totals emit zeros.
- Emit ordinal output index and ordered `{style}_return` genuine-double
  columns. Do not retain input views, raw output indexes, or hidden columns.
- Empty-shaped returns or an exposure surface with no rows/styles returns an
  empty `0x0` DataFrame before nonempty calculation. All nonempty structures,
  identities, weights, returns, arithmetic, indexes, names, and allocations
  obey the design's strict `input_error` boundary.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.
- Do not implement rolling style tilts, regression attribution, factor
  fetching/providers, Python `StyleResult` conveniences, SciPy, reports,
  packaging/release work, or GUI runtime behavior.

### Task 128: Record Phase 30 design and executable plan

**Files:**

- Create: `docs/design/2026-08-12-phase-30-style-performance-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-30-style-performance.md`

1. Record alternatives, exact overloads, Phase 29 exposure mapping, alignment,
   normalization, empty/finite semantics, output schema/index, oracle, test
   matrix, gates, and exclusions.
2. Run diff checks; stage exactly these two documents; commit:

   ```sh
   git commit -m "docs: plan phase 30 style performance"
   ```

### Task 129: Add native performance-by-style overloads

**Files:**

- Modify: `include/fincore/attribution/style.hpp`
- Modify: `src/attribution/style.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/style_performance_test.cpp`
- Create: `tests/unit/attribution_style_performance_umbrella_test.cpp`

1. Create the ignored Phase 30 SDD brief, ledger, oracle, RED log, and report.
   Run from `/Users/yunjinqi/Documents/new_projects/fincore` and retain this
   exact source oracle:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import pandas as pd
   from fincore.attribution.style import analyze_performance_by_style
   returns = pd.DataFrame({
       "ALPHA": [.01, -.02, .03, 0., -.01],
       "BETA": [.02, .01, -.01, .04, 0.],
       "GAMMA": [-.01, .03, .02, -.02, .05],
   }, index=pd.Index([42, -7, 42, 100, 3], name="raw"))
   exposures = pd.DataFrame({
       "blend": [2., 1., 0.], "hedged": [1., -.5, .5],
       "zero": [0., 0., 0.], "negative_total": [-1., 0., 0.],
   }, index=["ALPHA", "BETA", "GAMMA"])
   explicit = analyze_performance_by_style(returns, exposures)
   automatic = analyze_performance_by_style(returns)
   print("EXPLICIT_INDEX", explicit.index.tolist())
   print("EXPLICIT_COLUMNS", explicit.columns.tolist())
   print("EXPLICIT_VALUES", explicit.values.tolist())
   print("AUTO_INDEX", automatic.index.tolist())
   print("AUTO_COLUMNS", automatic.columns.tolist())
   print("AUTO_VALUES", automatic.values.tolist())
   print("REORDERED", analyze_performance_by_style(
       returns, exposures.loc[["GAMMA", "ALPHA", "BETA"]]).values.tolist())
   print("EMPTY_RETURNS", analyze_performance_by_style(pd.DataFrame()).shape)
   print("EMPTY_EXPOSURES", analyze_performance_by_style(
       returns, pd.DataFrame()).shape)
   '
   ```

2. Register both new public consumers before adding production declarations.
   The leaf test includes only `<fincore/attribution/style.hpp>` among fincore
   headers; the façade test includes only `<fincore/attribution.hpp>`. Build
   and retain the genuine missing-overload RED; do not run stale CTest.
3. Add the exact two declarations and minimal implementation:
   - short-circuit the documented empty surfaces;
   - validate/copy bounded return and exposure names and finite columns;
   - construct a distinct exposure-asset row map;
   - align each style to stable return-asset order, sum and normalize only a
     positive total, then compute finite ordered row dot products;
   - materialize ordinal output with stable `{style}_return` columns;
   - have the one-argument overload call public `style_analysis` once and
     delegate; translate allocation/length failures to `input_error`.
4. Add tests for exact overload types, full explicit and automatic oracles,
   reordered rows, missing/extra assets, negative hedge weights, zero/negative
   totals, empty returns/exposures/styles, duplicate/nonmonotonic raw indexes,
   exact total column type/order/count, default façade visibility, malformed or
   mixed types, missing/duplicate/NUL identities, nonfinite input/arithmetic,
   representability, and ownership after source mutation/destruction.
5. Run build, focused `StylePerformance|AttributionStylePerformanceUmbrella`,
   related `StyleAnalysis|StylePerformance|Attribution|Dataframe`, full CTest,
   and diff checks. Commit exactly the five owned paths:

   ```sh
   git commit -m "feat: add native performance by style"
   ```

6. Dispatch fresh independent specification and task-quality reviews. Correct
   every Critical, Important, or Minor finding in new scoped commits and rerun
   focused/full verification; never amend accepted history.

### Task 130: Accept Phase 30

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-30-style-performance-acceptance.md`

1. Document API/source mapping, alignment and normalization, empty/finite
   policy, output ownership/schema, oracle, true dual-consumer RED, final leaf
   reviews, exact implementation SHA/scope, and exclusions.
2. Retain raw current focused/full logs and fresh Apple headless, registration-
   audited LLVM ASan+UBSan, ImPlot/demo compile/link-only/no-GUI, invalid-ImGui
   multiline diagnostic, and actual FetchContent provenance evidence.
3. Dispatch a fresh independent read-only document/gate review; fix every
   Critical, Important, or Minor issue before committing exactly the two
   documents:

   ```sh
   git commit -m "docs: accept phase 30 style performance"
   ```

### Task 131: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-30-style-performance-acceptance.md`

1. Record Task 129's complete implementation/fix chain and exact scopes plus
   Task 130's initial acceptance SHA/scope. State Task 131 is documentation-
   only, does not amend prior commits, and does not state or anticipate its own
   SHA.
2. Run focused Style Performance/façade CTest and diff checks; commit only the
   acceptance document:

   ```sh
   git commit -m "docs: trace phase 30 style performance"
   ```

### Final Phase 30 review

Audit all Phase 30 ancestry/scopes; API and default composition; exposure
identity mapping; missing/extra assets; negative and nonpositive weights;
empty/finite boundaries; exact output schema/order/index and ownership;
leaf/façade proof; raw gates, provenance, no-GUI wording, and non-self-
referential trace. Fresh-configure Apple headless, rerun focused/full CTest and
the mandatory Anaconda oracle, then publish Spec/Task-Quality verdicts. Correct
every Critical, Important, or Minor finding in new scoped commits.
