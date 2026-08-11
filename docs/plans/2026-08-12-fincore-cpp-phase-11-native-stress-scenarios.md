# fincore C++ Phase 11 Native Stress Scenarios Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task with a fresh implementer and independent review for each task.

**Goal:** Add the deterministic, DataFrame-native return-stress core of Python `fincore.simulation.scenarios`, with typed outputs and strict time-series boundary contracts.

**Architecture:** Create a `fincore/simulation/scenarios.hpp` leaf that transforms one explicit return column into value-owning scenario results, then derives summaries through the existing public return/drawdown metrics. A compact `fincore/simulation.hpp` umbrella exposes this leaf; its numeric `scenario_table` replaces Python's formatted pandas presentation table without importing reporting machinery.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local Python oracle via `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot compile/link verification.

## Global Constraints

- Use only `fincore::dataframe` (`hmdf::StdDataFrame<fincore::timestamp>`) and existing accepted public metrics. Do not add NumPy, pandas, Python embedding, a random library, a matrix/linear-algebra dependency, providers, or a reporting dependency.
- Create only the simulation leaf and `fincore/simulation.hpp` umbrella. Do not modify accepted metrics, attribution modules, generic DataFrame helpers, calendar behavior, third-party configuration, or ImPlot source.
- Every stress operation takes an explicit genuine-`double` return column; no inferred column selection is permitted. The input keeps its supplied raw timestamp order and duplicate values exactly; it is never sorted, joined, resampled, or reindexed.
- The selected column must be nonempty, present, genuine `double`, vector-length-correct, contain at least one row, and contain no NaN. A selected NaN throws `fincore::input_error` before output construction so timestamps cannot be silently dropped. Signed infinities are not filtered or clamped and retain ordinary IEEE arithmetic.
- `stress_test(returns)` emits `crash`, `spike`, `vol_crush`, `vol_spike` in that order. An explicit overload preserves supplied predefined order, followed by supplied custom order. Reject unknown/duplicate predefined selections, empty custom names, reserved predefined custom names, and duplicate custom names.
- Built-in transformations are exact: crash replaces midpoint row `size()/2` with `-0.20`; spike replaces it with `0.10`; volatility crush multiplies each selected return by `0.5`; volatility spike multiplies each selected return by `2.0`; custom applies `(return + return_shift) * return_multiplier` in that order.
- Each result owns its name, fresh stressed-return DataFrame, cumulative return, maximum drawdown, annual volatility, and final value. Derive those fields through existing public `cumulative_return`, `max_drawdown`, and `annual_volatility` once each; do not copy their formulas. `final_value == 1.0 + cumulative_return`.
- `scenario_table` is a typed data model, not Python's formatted display: index `0..N-1`, one genuine `std::string` `scenario` column, then four genuine `double` columns `cumulative_return`, `max_drawdown`, `volatility`, `final_value`, in exact order. Empty results produce that typed-empty schema.
- Exclude Python `generate_correlation_breakdown`, percent-formatted tables, bootstrap/Monte Carlo/GBM/LHS/antithetic paths, simulation VaR/CVaR, GARCH/EVT, Fama-French/style attribution, providers, reports/tearsheets, packages/releases, full Python simulation parity, and GUI runtime.
- All Python commands must use `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare `python` or `python3`.

---

### Task 57: Add the deterministic native stress-scenario leaf

**Files:**

- Create: `include/fincore/simulation/scenarios.hpp`
- Create: `src/simulation/scenarios.cpp`
- Create: `tests/unit/stress_scenarios_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** `fincore::dataframe`, `timestamp`, `require_double_column`, `input_error`, `returns_column`, `cumulative_return`, `annual_volatility`, `max_drawdown`, and the accepted C++23 DataFrame boundary.

**Produces:**

~~~cpp
inline constexpr std::string_view stress_scenario_name_column{"scenario"};
inline constexpr std::string_view stress_cumulative_return_column{
    "cumulative_return"};
inline constexpr std::string_view stress_max_drawdown_column{"max_drawdown"};
inline constexpr std::string_view stress_volatility_column{"volatility"};
inline constexpr std::string_view stress_final_value_column{"final_value"};

enum class predefined_stress_scenario {
    crash,
    spike,
    volatility_crush,
    volatility_spike,
};

struct custom_stress_scenario {
    std::string name;
    double return_shift{0.0};
    double return_multiplier{1.0};
};

struct stress_scenario_result {
    std::string name;
    dataframe stressed_returns;
    double cumulative_return;
    double max_drawdown;
    double volatility;
    double final_value;
};

std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::string_view column = returns_column);

std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::span<const predefined_stress_scenario> scenarios,
    std::span<const custom_stress_scenario> custom_scenarios = {},
    std::string_view column = returns_column);

dataframe scenario_table(std::span<const stress_scenario_result> results);
~~~

- [ ] **Step 1: Record the Python oracle and create the public-header RED test**

Create `tests/unit/stress_scenarios_test.cpp`, include only
`<fincore/simulation/scenarios.hpp>` among fincore headers, and register it in
`tests/CMakeLists.txt`. Do not create the public header or register the source
before this RED build.

Run this exact oracle and record its complete output in the ignored Task 57
report:

~~~sh
cd /Users/yunjinqi/Documents/new_projects/fincore
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
import numpy as np
from fincore.simulation.scenarios import stress_test
r = np.array([.01, -.02, .03, -.01], dtype=float)
results = stress_test(
    r,
    scenarios=["crash", "spike", "vol_crush", "vol_spike"],
    custom_scenarios={"shift_then_mult": {"return_shift": .01, "return_mult": 1.5}})
for name, value in results.items():
    print(name, value["stressed_returns"].tolist(),
          repr(value["cumulative_return"]), repr(value["max_drawdown"]),
          repr(value["volatility"]), repr(value["final_value"]))
'
~~~

The test must construct one timestamp-indexed `returns` frame with this vector,
plus ignored unrelated metadata. Assert source-order default results and these
finite Python goldens (tolerance `1e-12`):

~~~text
crash:     [-.20 at midpoint], cumulative=-.2160784, max_drawdown=-.22384,
           volatility=1.5472556349873154, final=.7839216
spike:     [.10 at midpoint],  cumulative=.0778922, max_drawdown=-.02,
           volatility=.8694826047713664, final=1.0778922
vol_crush: [.005,-.01,.015,-.005], cumulative=.004824878749999817,
           max_drawdown=-.01, volatility=.1759971590679804, final=1.0048248787499998
vol_spike: [.02,-.04,.06,-.02], cumulative=.017192960000000035,
           max_drawdown=-.04, volatility=.7039886362719217, final=1.01719296
shift_then_mult: [.03,-.015,.06,0], cumulative=.07542300000000002,
           max_drawdown=-.015, volatility=.5279914772039412, final=1.075423
~~~

Assert every stressed frame preserves the original raw index and custom return
column, the default names are exactly `crash`, `spike`, `vol_crush`,
`vol_spike`, and every summary has `final_value == 1.0 + cumulative_return`.

- [ ] **Step 2: Run the RED build before production edits**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R StressScenarios --output-on-failure
~~~

Expected: compilation fails because the public leaf header does not exist; a
CTests result from a stale executable is not valid RED evidence.

- [ ] **Step 3: Implement validation, transformations, and typed summary**

Create the public header with exactly the API above. In the source, keep all
validation/lookup helpers private. Validate the selected return column first;
then require a nonempty selected vector and reject NaN while retaining signed
infinity. Copy the index and selected vector into every output. Do not copy
any unrelated metadata.

Map each enum to its canonical source name and transform with direct arithmetic.
For crash/spike, replace `values[values.size() / 2U]`; for a custom scenario,
apply shift first and multiplier second. Use owned string sets to validate
canonical/custom names before constructing any output. The default overload
should call the explicit overload with the four built-ins in canonical order.

For each stressed DataFrame, call public `cumulative_return`, `max_drawdown`,
and `annual_volatility` exactly once, then derive final value. Construct
`scenario_table` with synthetic ordinal index and the exact string/double
column schema, including a typed-empty schema. Do not add `vol_mult`, a
correlation matrix, random state, or any Python formatting behavior. Register
only `src/simulation/scenarios.cpp` in `CMakeLists.txt`.

- [ ] **Step 4: Add boundary regressions and run GREEN**

Extend the same public-header test with:

- explicit predefined order, empty explicit spans, custom-only scenarios, and
  custom shift-then-multiplier order;
- typed summary schema/order/string names and typed-empty summary;
- unsorted and duplicate input timestamps preserved as supplied, along with
  ignored malformed unrelated numeric metadata;
- missing/empty/wrong-type/short selected column, empty input, selected NaN,
  unknown/duplicate enum, empty/reserved/duplicate custom names all throw
  `fincore::input_error`;
- a signed-infinity selected return remains in the stressed output rather than
  being filtered/clamped; and a one-row finite frame has the established NaN
  annual-volatility boundary;
- result ownership by modifying/destroying the source fixture after `stress_test`
  and observing unchanged result values.

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'StressScenarios|Returns|Drawdown|Ratios|Dataframe' --output-on-failure
ctest --test-dir build --output-on-failure
git diff --check
~~~

Expected: exact finite goldens and strict structural contracts pass; existing
metrics remain green; full CTest has no regression; diff check has no output.

- [ ] **Step 5: Review and commit only Task 57 paths**

Review value ownership, direct transform order, source-order preservation, and
absence of random/provider/report logic. Stage only:

~~~sh
git add CMakeLists.txt tests/CMakeLists.txt include/fincore/simulation/scenarios.hpp src/simulation/scenarios.cpp tests/unit/stress_scenarios_test.cpp
git commit -m "feat: add native stress scenarios"
~~~

Expected: one task-scoped implementation commit and ignored Task 57 report
with RED, Anaconda oracle, GREEN, full CTest, and scope audit.

### Task 58: Export the simulation umbrella and accept Phase 11

**Files:**

- Create: `include/fincore/simulation.hpp`
- Create: `tests/unit/simulation_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-11-native-stress-scenarios-acceptance.md`

**Consumes:** Task 57 public API and established fresh-gate conventions.

- [ ] **Step 1: Add a simulation-umbrella-only RED consumer**

Create `tests/unit/simulation_umbrella_test.cpp`, include only
`<fincore/simulation.hpp>` among fincore headers, and register it. Build a
minimal one-date/two-scenario fixture through `stress_test` and
`scenario_table`; assert a scenario name, transformed output, and numeric
summary field. Do not include the scenarios leaf, internal source headers, or
metrics headers directly.

- [ ] **Step 2: Run the umbrella RED test**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R SimulationUmbrella --output-on-failure
~~~

Expected: the public consumer fails solely because `<fincore/simulation.hpp>`
is absent; a stale test executable is not evidence.

- [ ] **Step 3: Export the leaf and document the bounded simulation surface**

Create `include/fincore/simulation.hpp` with exactly:

~~~cpp
#pragma once

#include <fincore/simulation/scenarios.hpp>
~~~

Update README and create the acceptance record with the exact structs,
overloads, enum, scenario table schema, NaN/index policy, Task 57 full SHA,
Python finite oracle/RED/GREEN evidence, and explicit exclusions. State that
the `scenario_table` is numeric/model data rather than Python's formatted
pandas display, ImPlot evidence is compile/link only, and completion is not
full simulation/Python parity/release/GUI runtime validation.

- [ ] **Step 4: Run umbrella GREEN plus retained fresh acceptance gates**

First run focused/full configured tests. Then use four new `mktemp -d`
directories. For every configure/build/CTest command, enable `set -o pipefail`
and retain terminal output with `2>&1 | tee` in that gate directory. Do not
delete any existing temporary directory.

~~~sh
set -euo pipefail
APPLE_DIR=$(mktemp -d /tmp/fincore-phase11-apple.XXXXXX)
cmake -S . -B "$APPLE_DIR" -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF 2>&1 | tee "$APPLE_DIR/configure.log"
cmake --build "$APPLE_DIR" --parallel 4 2>&1 | tee "$APPLE_DIR/build.log"
ctest --test-dir "$APPLE_DIR" --output-on-failure 2>&1 | tee "$APPLE_DIR/ctest.log"
{
  printf '%s\n' "inputs: $APPLE_DIR/configure.log $APPLE_DIR/build.log $APPLE_DIR/ctest.log"
  if rg -n 'warning:' "$APPLE_DIR/configure.log" "$APPLE_DIR/build.log" "$APPLE_DIR/ctest.log"; then
    echo 'unexpected AppleClang warning matches' >&2
    exit 1
  else
    warning_status=$?
    printf 'rg exit status: %s (expected no-match)\n' "$warning_status"
    test "$warning_status" -eq 1
  fi
} > "$APPLE_DIR/warnings-scan.log"

LLVM_DIR=$(mktemp -d /tmp/fincore-phase11-llvm.XXXXXX)
cmake -S . -B "$LLVM_DIR" -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF 2>&1 | tee "$LLVM_DIR/configure.log"
cmake --build "$LLVM_DIR" --parallel 4 2>&1 | tee "$LLVM_DIR/build.log"
ctest --test-dir "$LLVM_DIR" --output-on-failure 2>&1 | tee "$LLVM_DIR/ctest.log"
{
  printf '%s\n' "inputs: $LLVM_DIR/configure.log $LLVM_DIR/build.log $LLVM_DIR/ctest.log"
  if rg -n -e 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error' "$LLVM_DIR/configure.log" "$LLVM_DIR/build.log" "$LLVM_DIR/ctest.log"; then
    echo 'unexpected sanitizer diagnostic match' >&2
    exit 1
  else
    scan_status=$?
    printf 'rg exit status: %s (expected zero-match)\n' "$scan_status"
    test "$scan_status" -eq 1
  fi
} > "$LLVM_DIR/sanitizer-scan.log"
rg -n 'warning:' "$LLVM_DIR/configure.log" "$LLVM_DIR/build.log" "$LLVM_DIR/ctest.log" > "$LLVM_DIR/warnings-scan.log" || true

IMPLOT_DIR=$(mktemp -d /tmp/fincore-phase11-implot.XXXXXX)
cmake -S . -B "$IMPLOT_DIR" -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON 2>&1 | tee "$IMPLOT_DIR/configure.log"
cmake --build "$IMPLOT_DIR" --parallel 4 2>&1 | tee "$IMPLOT_DIR/build.log"
ctest --test-dir "$IMPLOT_DIR" --output-on-failure 2>&1 | tee "$IMPLOT_DIR/ctest.log"
rg -n 'warning:' "$IMPLOT_DIR/configure.log" "$IMPLOT_DIR/build.log" "$IMPLOT_DIR/ctest.log" > "$IMPLOT_DIR/warnings-scan.log" || true

INVALID_DIR=$(mktemp -d /tmp/fincore-phase11-invalid-imgui.XXXXXX)
if cmake -S . -B "$INVALID_DIR" -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui 2>&1 | tee "$INVALID_DIR/configure.log"; then
  echo "expected invalid ImGui configure to fail" >&2
  exit 1
fi
rg -n 'FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required files' "$INVALID_DIR/configure.log"
~~~

The final configure must exit `1` with the complete Dear-ImGui diagnostic. Run
it in a branch that captures its expected nonzero status rather than masking a
failure. Record all actual exit statuses, paths, compiler versions, test
counts, warning scans, and the zero-match sanitizer scan in both the ignored
Task 58 report and acceptance document. The ImPlot test is compile/link only;
never launch the GUI.

- [ ] **Step 5: Commit only Task 58 tracked paths**

Run `git diff --check`, audit exact paths, then stage and commit:

~~~sh
git add include/fincore/simulation.hpp tests/unit/simulation_umbrella_test.cpp tests/CMakeLists.txt README.md docs/acceptance/2026-08-12-phase-11-native-stress-scenarios-acceptance.md
git commit -m "docs: accept phase 11 stress scenarios"
~~~

Expected: no numeric-source changes, retained fresh-gate evidence, and an
ignored Task 58 report.

### Task 59: Trace Phase 11 acceptance and run the whole-phase review gate

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-11-native-stress-scenarios-acceptance.md`

**Consumes:** Task 58's initial acceptance commit SHA and complete Task 57–58
range.

- [ ] **Step 1: Append non-self-referential Task 58 traceability**

After Task 58 is committed, append its complete SHA next to Task 57's
implementation SHA. State that Task 59 is documentation-only, does not amend
Task 58, and neither lists nor anticipates its own SHA. Do not change source,
test, compiler, oracle, or fresh-gate evidence.

- [ ] **Step 2: Verify trace-only behavior and commit**

Run:

~~~sh
git diff --check
ctest --test-dir build -R 'SimulationUmbrella|StressScenarios' --output-on-failure
git add docs/acceptance/2026-08-12-phase-11-native-stress-scenarios-acceptance.md
git commit -m "docs: trace phase 11 stress scenarios acceptance"
~~~

Expected: one documentation-only commit, green public consumer, and no
self-SHA.

- [ ] **Step 3: Request and address an independent whole-phase review**

Give a fresh reviewer the Phase 11 design, this plan, Task 57–59 commit range,
the Anaconda oracle, acceptance record, public headers, source, tests, saved
fresh-gate logs, and net diff. Require explicit Spec/Quality verdicts and
separate Critical/Important/Minor findings. Independently reproduce focused
and full headless tests, a standalone C++23 simulation umbrella consumer, the
finite Python scenario oracle, and the retained LLVM scan result.

If a Critical/Important finding or Spec failure exists, create a narrow RED
regression, make the smallest fix, rerun affected/full tests, obtain a scoped
re-review, and record factual evidence in a later non-self-referential
documentation commit. If clean, record PASS in the ignored Task 59 report
without claiming a simulation suite, full Python parity, release, or GUI
runtime readiness.

## Deferred after this plan

Native correlation matrices, formatted scenario reports, bootstrap inference,
Monte Carlo and GBM paths, LHS/antithetic sampling, simulation VaR/CVaR,
GARCH/EVT, Fama-French/style work, providers, tearsheets, packaging/release,
full Python simulation parity, and GUI runtime validation remain separate
plans. Completing Phase 11 is not full-project or release readiness.
