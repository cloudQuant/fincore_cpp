# fincore C++ Phase 10 Brinson Attribution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the deterministic, DataFrame-native BHB Brinson attribution core of Python `fincore.attribution.brinson`, with strict table contracts and no pandas-style implicit alignment.

**Architecture:** Add one leaf module at `fincore/attribution/brinson.hpp` and `src/attribution/brinson.cpp`. Four same-index wide DataFrames plus explicit selected sector names drive all arithmetic. `brinson_results` returns a timestamp-indexed typed summary table; single-period and cumulative operations return value-owning C++ structs. A later public `fincore/attribution.hpp` umbrella exports this one deterministic core only.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local Python oracle via `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot compile/link verification.

## Global Constraints

- Use only the accepted `fincore::dataframe` alias (`hmdf::StdDataFrame<fincore::timestamp>`) and the already configured local dependencies. Do not add pandas, NumPy, a Python runtime, a synthetic MultiIndex, a warning/logging dependency, an optimizer, or a new date-time library.
- Create only the Phase 10 Brinson leaf module and `fincore/attribution.hpp` umbrella. Do not change existing scalar metrics, `dataframe.hpp`, Phase 9 attribution behavior, calendar behavior, DataFrame dependency configuration, ImPlot source, or accepted public semantics.
- Every operation receives four DataFrames and an explicit nonempty `std::span<const std::string_view>` of sector column names. Never infer sectors from every double column; nonselected metadata, including numeric metadata, is ignored.
- All four indexes must be duplicate-free and exactly identical in raw timestamp sequence and order. Preserve the portfolio-return index exactly in `brinson_results`; never sort, floor dates, reindex, or silently intersect/drop rows.
- Selected sector names must be nonempty and distinct. Every selected column must be a genuine `double`, exist on all four frames, and have vector length equal to its owning index. Violations throw `fincore::input_error` before output construction.
- Use the BHB equations exactly and preserve ordinary IEEE NaN/signed-infinity arithmetic. Do not skip NaNs, normalize weights, enforce sum-to-one, clamp infinities, or reproduce Python/NumPy warning emission.
- `brinson_results` has exactly six ordered genuine-`double` columns: `allocation`, `selection`, `interaction`, `total`, `portfolio_return`, and `benchmark_return`. Single-period/cumulative result structs own their scalar values. Empty valid multi-period input yields the typed-empty schema; cumulative empty sums/products yield zero effects and zero cumulative returns; single-period input requires exactly one row.
- Phase 10 excludes Python's stateful `BrinsonAttribution` class, sector mapping/equal-weight fallbacks, `brinson_hood`, style/Fama-French attribution, providers, warning/drop alignment, reports/tearsheets, packaging/release, and GUI-runtime behavior.
- All Python oracle commands must use `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare `python` or `python3`.

---

### Task 54: Add native BHB Brinson attribution results and summaries

**Files:**

- Create: `include/fincore/attribution/brinson.hpp`
- Create: `src/attribution/brinson.cpp`
- Create: `tests/unit/brinson_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** `fincore::dataframe`, `timestamp`, `require_double_column`, `input_error`, and the accepted C++23 cloudQuant/DataFrame boundary.

**Produces:**

~~~cpp
inline constexpr std::string_view brinson_allocation_column{"allocation"};
inline constexpr std::string_view brinson_selection_column{"selection"};
inline constexpr std::string_view brinson_interaction_column{"interaction"};
inline constexpr std::string_view brinson_total_column{"total"};
inline constexpr std::string_view brinson_portfolio_return_column{
    "portfolio_return"};
inline constexpr std::string_view brinson_benchmark_return_column{
    "benchmark_return"};

struct brinson_period_result {
    double allocation;
    double selection;
    double interaction;
    double total;
    double portfolio_return;
    double benchmark_return;
};

struct brinson_cumulative_result {
    double allocation;
    double selection;
    double interaction;
    double total;
    double portfolio_cumulative;
    double benchmark_cumulative;
};

brinson_period_result brinson_attribution(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

dataframe brinson_results(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

brinson_cumulative_result brinson_cumulative(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);
~~~

- [ ] **Step 1: Record the Python oracle and write the public-header RED test**

Create `tests/unit/brinson_test.cpp`, with only
`<fincore/attribution/brinson.hpp>` among fincore headers, and register it in
`tests/CMakeLists.txt`. Do not register `src/attribution/brinson.cpp` in the
library until after the RED build: the first failure must be absence of the
public header/API, not a link failure.

Run this exact Anaconda oracle and record its output in the ignored Task 54
report:

~~~sh
cd /Users/yunjinqi/Documents/new_projects/fincore
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
import numpy as np
from fincore.attribution.brinson import brinson_attribution, brinson_results, brinson_cumulative
rp = np.array([[.10, .05, -.02], [.03, -.01, .04]], dtype=float)
rb = np.array([[.08, .04, -.01], [.02, .00, .03]], dtype=float)
wp = np.array([[.50, .30, .20], [.45, .35, .20]], dtype=float)
wb = np.array([[.40, .40, .20], [.40, .40, .20]], dtype=float)
for row in range(2): print(brinson_attribution(rp[row], rb[row], wp[row], wb[row]))
print(brinson_results(rp, rb, wp, wb, periods=["2024-01-02", "2024-01-03"]).to_csv(index=False, float_format="%.17g"), end="")
print(brinson_cumulative(rp, rb, wp, wb))
'
~~~

Use two raw timestamps for 2024-01-02/03 and three selected sector names
`tech`, `health`, `finance`. Build the four frames with these rows:

~~~text
portfolio returns: tech [.10, .03], health [.05, -.01], finance [-.02, .04]
benchmark returns: tech [.08, .02], health [.04,  .00], finance [-.01, .03]
portfolio weights: tech [.50, .45], health [.30,  .35], finance [ .20, .20]
benchmark weights: tech [.40, .40], health [.40,  .40], finance [ .20, .20]
~~~

Assert these tolerance-bounded Python goldens in the new public test:

~~~text
allocation        = [.004, .001]
selection         = [.010, .002]
interaction       = [.001, .001]
total             = [.015, .004]
portfolio_return  = [.061, .018]
benchmark_return  = [.046, .014]

cumulative allocation=.005, selection=.012, interaction=.002, total=.019,
portfolio_cumulative=.080098, benchmark_cumulative=.06064400000000014
~~~

The test must also require exactly the ordered six output columns, genuine
`double` types, the raw left timestamp index, single-period result equality
with the first output row, and BHB total-to-active-return identity. Add a
non-double `metadata` column to one or more inputs and show that explicit
sector selection ignores it.

- [ ] **Step 2: Run the RED build before production edits**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R Brinson --output-on-failure
~~~

Expected: compilation fails at the public include/API boundary because
`fincore/attribution/brinson.hpp` does not yet exist; any CTest result from an
older test executable is not RED evidence.

- [ ] **Step 3: Implement strict table validation and BHB arithmetic**

Create the public header with the exact declarations above. In the source,
keep all schema inspection private. First validate the caller sector span:
copy its names to owned strings, reject an empty span, empty name, or duplicate
name. For each input, validate its selected double spans and every vector
length; require unique timestamps by inserting the raw index values into a
private `std::unordered_set<timestamp>`; require the other three index vectors
to equal the portfolio-return index exactly.

Construct one new result DataFrame with an owned copy of the portfolio-return
index and these six output vectors in exact order. For each row/sector, use
direct BHB arithmetic only:

~~~cpp
allocation += (portfolio_weight - benchmark_weight) * benchmark_return;
selection += benchmark_weight * (portfolio_return - benchmark_return);
interaction += (portfolio_weight - benchmark_weight) *
               (portfolio_return - benchmark_return);
portfolio += portfolio_weight * portfolio_return;
benchmark += benchmark_weight * benchmark_return;
~~~

Then set `total = allocation + selection + interaction`. Do not use a
skip-NaN helper, a weight allocation helper, or a Python-style alignment path.
`brinson_attribution` must validate through `brinson_results`, require exactly
one result row, and return that row's six values. `brinson_cumulative` must
validate through `brinson_results` once, arithmetically accumulate its four
effect columns, and compound its portfolio/benchmark columns from an initial
wealth of `1.0`; thus an empty valid result produces zero fields.

Register the new source in `CMakeLists.txt`. Do not change `metrics.hpp`,
Phase 9 headers/source, or generic DataFrame helpers.

- [ ] **Step 4: Add boundary regressions and run GREEN**

Extend the same public-header test with these assertions:

- empty but correctly typed four-frame schemas yield a typed-empty six-column
  table and an all-zero cumulative record, while `brinson_attribution` throws;
- duplicate/empty selected sector names, absent selected sector, wrong selected
  type, malformed selected vector length, duplicate timestamp in any input,
  and mismatched index sequence throw `fincore::input_error`;
- unsorted-but-identical indexes preserve original left order and arithmetic;
- arbitrary non-unit/negative weights retain direct formula semantics;
- NaN and signed-infinity fixtures preserve direct IEEE propagation rather
  than filtering/dropping values.

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Brinson|Dataframe|PerfAttrib|Positions' --output-on-failure
ctest --test-dir build --output-on-failure
git diff --check
~~~

Expected: all goldens, schema contracts, and edge cases pass; the full suite
has no regression; the diff check has no output.

- [ ] **Step 5: Review and commit only Task 54 paths**

Review that every output is newly owned, no date order is changed, and the
source has no external/provider or sector-mapping behavior. Stage only:

~~~sh
git add CMakeLists.txt tests/CMakeLists.txt include/fincore/attribution/brinson.hpp src/attribution/brinson.cpp tests/unit/brinson_test.cpp
git commit -m "feat: add native Brinson attribution"
~~~

Expected: one task-scoped implementation commit and an ignored Task 54 report
recording RED, Python goldens, GREEN, full CTest, and scope audit.

### Task 55: Export the attribution umbrella and accept Phase 10

**Files:**

- Create: `include/fincore/attribution.hpp`
- Create: `tests/unit/attribution_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-11-phase-10-brinson-attribution-acceptance.md`

**Consumes:** Task 54's public Brinson API and the established build/test gate
conventions.

- [ ] **Step 1: Add an attribution-umbrella-only RED consumer**

Create `tests/unit/attribution_umbrella_test.cpp`, include only
`<fincore/attribution.hpp>` among fincore headers, and register it in
`tests/CMakeLists.txt`. Use a one-date/two-sector fixture to call
`brinson_attribution`, `brinson_results`, and `brinson_cumulative`; assert an
allocation field, a summary-table field, and one cumulative field. Do not
include the Brinson leaf header or any private source header.

- [ ] **Step 2: Run the umbrella RED test**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R AttributionUmbrella --output-on-failure
~~~

Expected: compilation fails only because `<fincore/attribution.hpp>` does not
yet exist; a stale previous CTest executable is not treated as green or red
evidence.

- [ ] **Step 3: Export the leaf header and document the bounded public surface**

Create `include/fincore/attribution.hpp` with exactly:

~~~cpp
#pragma once

#include <fincore/attribution/brinson.hpp>
~~~

Update README with the exact structs, operations, explicit-sector table
contract, six-column result schema, and exclusions. Create an acceptance
record with Task 54's full SHA, public API/schema mapping, RED/GREEN and
Anaconda oracle evidence, warning language, and explicit separation from
stateful sector mapping, Brinson-Hood, style/Fama-French, providers, reports,
packaging, and GUI runtime.

- [ ] **Step 4: Run umbrella GREEN plus fresh acceptance gates**

Run current focused and full tests, then use new unique `/tmp` directories
from `mktemp -d` for each gate; do not delete an existing directory:

~~~sh
APPLE_DIR=$(mktemp -d /tmp/fincore-phase10-apple.XXXXXX)
cmake -S . -B "$APPLE_DIR" -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build "$APPLE_DIR" --parallel 4
ctest --test-dir "$APPLE_DIR" --output-on-failure

LLVM_DIR=$(mktemp -d /tmp/fincore-phase10-llvm.XXXXXX)
cmake -S . -B "$LLVM_DIR" -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build "$LLVM_DIR" --parallel 4
ctest --test-dir "$LLVM_DIR" --output-on-failure

IMPLOT_DIR=$(mktemp -d /tmp/fincore-phase10-implot.XXXXXX)
cmake -S . -B "$IMPLOT_DIR" -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build "$IMPLOT_DIR" --parallel 4
ctest --test-dir "$IMPLOT_DIR" --output-on-failure

INVALID_DIR=$(mktemp -d /tmp/fincore-phase10-invalid-imgui.XXXXXX)
cmake -S . -B "$INVALID_DIR" -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui
~~~

Search LLVM configure/build/CTest logs for `AddressSanitizer`,
`UndefinedBehaviorSanitizer`, and `runtime error`; record no sanitizer
diagnostic only if all no-match checks are clean. Record exact third-party and
macOS OpenGL warnings. The last configure must fail with the project's
complete-Dear-ImGui diagnostic. The ImPlot test is compile/link only; do not
launch the GUI.

- [ ] **Step 5: Commit only Task 55 tracked paths**

Run `git diff --check`, audit exact paths, then stage and commit:

~~~sh
git add include/fincore/attribution.hpp tests/unit/attribution_umbrella_test.cpp tests/CMakeLists.txt README.md docs/acceptance/2026-08-11-phase-10-brinson-attribution-acceptance.md
git commit -m "docs: accept phase 10 Brinson attribution"
~~~

Expected: no numeric-source changes, precise fresh-gate evidence, and an
ignored Task 55 report.

### Task 56: Trace Phase 10 acceptance and run the whole-phase review gate

**Files:**

- Modify: `docs/acceptance/2026-08-11-phase-10-brinson-attribution-acceptance.md`

**Consumes:** Task 55's acceptance commit SHA and the complete Task 54–55
range.

- [ ] **Step 1: Append non-self-referential Task 55 traceability**

After the Task 55 initial acceptance commit, append its complete SHA next to
the Task 54 implementation SHA. State that Task 56 is documentation-only,
does not amend Task 55, and does not list or anticipate its own SHA. Do not
alter compiler/test/oracle evidence.

- [ ] **Step 2: Verify trace-only behavior**

Run:

~~~sh
git diff --check
ctest --test-dir build -R 'AttributionUmbrella|Brinson' --output-on-failure
~~~

Expected: only the acceptance document changes, its trace is correct, and the
public attribution consumer remains green.

- [ ] **Step 3: Commit the trace**

Run:

~~~sh
git add docs/acceptance/2026-08-11-phase-10-brinson-attribution-acceptance.md
git commit -m "docs: trace phase 10 Brinson attribution acceptance"
~~~

Expected: one documentation-only commit with no self-SHA.

- [ ] **Step 4: Request and address an independent whole-phase review**

Give a fresh reviewer the Phase 10 design, this plan, Task 54–56 commit range,
the Anaconda oracle evidence, acceptance record, public headers, source,
tests, and net diff. Require explicit Spec and Quality verdicts plus separate
Critical/Important/Minor findings. Independently reproduce the focused Brinson
and attribution-umbrella tests, full headless CTest, a standalone C++23 public
header consumer, and the two-period Python BHB oracle.

If a finding exists, create a narrow RED regression, make the minimal fix,
rerun affected and full tests, obtain a scoped re-review, and record factual
evidence in a later non-self-referential documentation commit. If no finding
exists, record PASS in the ignored Task 56 report without claiming full Python
attribution or project parity.

## Deferred after this plan

The native core deliberately leaves stateful sector mapping/equal-weight
fallbacks, Brinson-Hood, style analysis, Fama-French models and providers,
bootstrap/Bayesian simulation, GARCH/EVT, optimization, reporting/tearsheets,
packaging/release, full Python facade parity, and interactive ImPlot runtime
validation for separate plans. Completing Phase 10 is not a full-project or
release readiness claim.
