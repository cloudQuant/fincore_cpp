# Phase 25 DataFrame-native simulation-result core Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to
> implement this plan task-by-task.

**Goal:** Add a value-owning, DataFrame-native terminal-path result object
with source-derived terminal statistics and VaR/CVaR.

**Architecture:** Add a small simulation-result leaf that validates and copies
the established time-indexed named-path DataFrame. The factory materializes
finite terminal values once, computes named statistics, and constructs an
immutable result object whose methods use that owned terminal distribution.
The simulation umbrella is updated only after the leaf is accepted.

**Tech Stack:** C++23, cloudQuant/DataFrame, CMake, GoogleTest, existing
`fincore::detail::linear_percentile`, mandatory Anaconda Python source oracle,
Homebrew LLVM 22 ASan+UBSan, and optional local ImPlot compile/link acceptance.

---

### Task 108: Record Phase 25 design and executable plan

**Files:**

- Create: `docs/design/2026-08-12-phase-25-simulation-result-core-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-25-simulation-result-core.md`

1. Record the selected result-value approach and rejected full-orchestrator
   and loose-free-function alternatives; public API; orientation; validation;
   singleton/nonfinite/alpha rules; source oracle; test matrix; and exclusions.
2. Commit only these documents:

   ```sh
   git add docs/design/2026-08-12-phase-25-simulation-result-core-design.md \
           docs/plans/2026-08-12-fincore-cpp-phase-25-simulation-result-core.md
   git commit -m "docs: plan phase 25 simulation result core"
   ```

### Task 109: Add the terminal simulation-result leaf

**Files:**

- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `include/fincore/simulation/result.hpp`
- Create: `src/simulation/result.cpp`
- Create: `tests/unit/simulation_result_test.cpp`

1. Create ignored Phase 25 SDD workspace, brief, ledger, and Task 109 report.
   Run this Anaconda oracle from
   `/Users/yunjinqi/Documents/new_projects/fincore` and record raw output:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import numpy as np
   from fincore.simulation.base import SimResult
   paths = np.array([
       [100., 95., 80.],
       [100., 105., 110.],
       [100., 110., 130.],
       [100., 120., 190.],
   ])
   result = SimResult(paths)
   print(result.statistics)
   for alpha in (0.01, 0.05, 0.5, 0.95, 0.99, 0.0, 1.0):
       print(alpha, result.var(alpha), result.cvar(alpha))
   print(SimResult(np.array([[1.]])).statistics)
   '
   ```

2. Before adding production declarations, create/register a leaf-only consumer
   that includes only `<fincore/simulation/result.hpp>`, asserts the proposed
   type/function/method calls, and runs:

   ```sh
   cmake --build build --parallel 4
   ```

   Record the genuine missing-header/symbol RED. Do not run stale CTest after
   this failed build.

3. Declare precisely the two named statistics structs, `simulation_result`
   with const accessors/path and observation counts/terminal VaR/CVaR methods,
   and `analyze_simulation_paths(const dataframe&)`. The result stores its own
   `dataframe`, terminal values, and statistics behind a private invariant;
   it must have no public mutable state or raw ownership pointer.

4. In the source, materialize the DataFrame's raw index and every genuine
   double path column in column order. Reject an empty index, zero columns,
   non-double/mixed columns, wrong lengths, nonfinite terminal values, and
   output/statistical allocation or finite-intermediate failures as
   `input_error`. Preserve all nonterminal raw values and raw index exactly.
   Use `values.back()` only; do not sort/deduplicate the index. Use
   `detail::linear_percentile` for all finite percentile/VaR calculations.
   For a valid singleton return a NaN sample standard deviation. Return NaN for
   invalid alpha rather than throwing. CVaR averages the values `<=` the
   public terminal VaR.

5. Add public tests that use no private production helpers:

   - transpose the oracle into raw index `{9, 3, 9}` with named path columns;
     assert exact owned index/schema/order, all named statistics, five valid
     alpha VaR/CVaR values, and `path_count`/`observation_count`;
   - singleton parity and invalid alpha (zero/one/NaN/infinity) NaN results;
   - valid nonterminal NaN/infinity retention and nonfinite terminal rejection;
   - empty/no-column/non-double/mixed/malformed validation;
   - source mutation/destruction proves the result, stored paths, statistics,
     and terminal methods remain value-owned.

6. Run:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R SimulationResult --output-on-failure
   ctest --test-dir build -R 'SimulationResult|GbmPaths|SimulationGbmPathsUmbrella|SimulationGbmFromReturnsUmbrella|AntitheticVariates|SimulationAntitheticVariatesUmbrella|SimulationParameters|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

7. Commit exactly the five owned paths:

   ```sh
   git add CMakeLists.txt tests/CMakeLists.txt \
           include/fincore/simulation/result.hpp src/simulation/result.cpp \
           tests/unit/simulation_result_test.cpp
   git commit -m "feat: add simulation result core"
   ```

8. Dispatch a fresh independent read-only review. Correct every Critical,
   Important, or Minor issue in a new scoped commit and rerun focused/full
   CTest; never amend accepted history.

### Task 110: Export and accept the result core

**Files:**

- Modify: `README.md`
- Modify: `include/fincore/simulation.hpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/simulation_result_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-12-phase-25-simulation-result-core-acceptance.md`

1. Create/register a consumer that includes only `<fincore/simulation.hpp>`;
   before adding the one result-header umbrella include, build it and record
   the genuine missing-export RED. Do not use a stale binary.
2. Add only `#include <fincore/simulation/result.hpp>` to the simulation
   façade. The façade consumer must construct/analyze a named DataFrame and
   assert type, raw index, all named statistics, methods, and absence of an
   extra non-double column through total-schema evidence.
3. Document the final leaf/fix SHA chain, C++ API, native orientation,
   finite-terminal policy, singleton/invalid-alpha behavior, ownership,
   source oracle/RED/GREEN, independent review, and exclusions. Keep the
   acceptance document free of the Task 110 SHA until Task 111.
4. Retain raw `tee` logs for current focused/full CTest, fresh Apple headless,
   clean registration-audited Homebrew LLVM ASan+UBSan, ImPlot/demo
   compile/link-only with build-root executable checked but never launched,
   invalid-ImGui multiline diagnostic, and actual GoogleTest FetchContent
   provenance. If any LLVM attempt is corrupt/time-sliced, retain it only as a
   non-acceptance diagnostic and use a unique clean retry.
5. Dispatch a fresh independent read-only document/gate review. Fix every
   Critical, Important, or Minor finding before staging exactly:

   ```sh
   git add README.md include/fincore/simulation.hpp tests/CMakeLists.txt \
           tests/unit/simulation_result_umbrella_test.cpp \
           docs/acceptance/2026-08-12-phase-25-simulation-result-core-acceptance.md
   git commit -m "docs: accept phase 25 simulation result core"
   ```

### Task 111: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-25-simulation-result-core-acceptance.md`

1. Record complete Task 109 implementation/fix chain plus direct Task 110
   acceptance SHA and exact scopes. State Task 111 is documentation-only and
   does not amend earlier commits; do not mention or predict its own SHA.
2. Run focused `SimulationResult|SimulationResultUmbrella` CTest and diff
   checks, then commit only this document:

   ```sh
   git commit -m "docs: trace phase 25 simulation result core"
   ```

### Final Phase 25 review

Audit Task 109/110/111 scope and ancestry; API/class invariant; DataFrame
orientation/order/index ownership; source oracle/finite-terminal/singleton/
invalid-alpha/VaR/CVaR boundaries; use of the shared percentile primitive;
no GBM/antithetic/LHS/bootstrap/MonteCarlo drift; façade export; raw gates,
FetchContent provenance, no-GUI claims; and trace wording/non-self-reference.
Fresh-configure Apple headless, rerun focused/full CTest and the mandatory
Anaconda oracle, then publish Spec/Task-Quality verdicts. Correct every
Critical, Important, or Minor finding in new scoped commits; do not amend
accepted history.
