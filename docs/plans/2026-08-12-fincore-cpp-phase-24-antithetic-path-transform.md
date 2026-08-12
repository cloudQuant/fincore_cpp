# Phase 24 Antithetic Path Transform Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to
> implement this plan task-by-task.

**Goal:** Add a DataFrame-native antithetic path transform that doubles named
path columns while preserving the native time-indexed orientation.

**Architecture:** Extend only the existing paths leaf. Copy the input index and
original genuine-double path columns, append deterministic `antithetic_` names,
and mirror every value around the first value of the first path column. The
existing simulation façade exposes this automatically through `paths.hpp`.

**Tech Stack:** C++23, cloudQuant/DataFrame, CMake, GoogleTest, the mandatory
Anaconda Python source oracle, Homebrew LLVM 22 ASan+UBSan, and optional local
ImPlot compile/link acceptance.

---

### Task 103: Record Phase 24 design and executable plan

**Files:**

- Create: `docs/design/2026-08-12-phase-24-antithetic-path-transform-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-24-antithetic-path-transform.md`

1. Record the source-orientation to DataFrame-orientation mapping, exact
   declaration/prefix/output naming, validation and IEEE propagation policy,
   test matrix, façade rationale, and exclusions.
2. Commit only the two documents:

   ```sh
   git add docs/design/2026-08-12-phase-24-antithetic-path-transform-design.md \
           docs/plans/2026-08-12-fincore-cpp-phase-24-antithetic-path-transform.md
   git commit -m "docs: plan phase 24 antithetic path transform"
   ```

### Task 104: Add the leaf API and automatic façade proof

**Files:**

- Modify: `include/fincore/simulation/paths.hpp`
- Modify: `src/simulation/paths.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/antithetic_variates_test.cpp`
- Create: `tests/unit/simulation_antithetic_variates_umbrella_test.cpp`

1. Create ignored Phase 24 SDD workspace, brief, ledger, and Task 104 report.
   Run the mandated Anaconda source oracle in
   `/Users/yunjinqi/Documents/new_projects/fincore` for:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import numpy as np
   from fincore.simulation.paths import antithetic_variates
   paths = np.array([[100., 110., 120.], [100., 90., 80.]])
   print(antithetic_variates(paths))
   print(antithetic_variates(np.array([[1.]])))
   for shape in ((0, 0), (0, 2), (2, 0)):
       try:
           antithetic_variates(np.empty(shape))
       except Exception as error:
           print(shape, type(error).__name__, str(error))
   '
   ```

2. Before production declarations, add/register two consumers: one includes
   only `<fincore/simulation/paths.hpp>` and one includes only
   `<fincore/simulation.hpp>`. Assert the intended function/constant use, run
   `cmake --build build --parallel 4`, and record the genuine missing-symbol
   RED. Do not run stale CTest after a failed build.

3. Add the exact API:

   ```cpp
   inline constexpr std::string_view antithetic_path_column_prefix{
       "antithetic_"};
   dataframe antithetic_variates(const dataframe& paths);
   ```

   Validate a nonempty, genuine-double-only path table; copy raw index and
   source columns; append `antithetic_ + source_name` columns in source order;
   reject impossible/colliding names and unrepresentable output shape before
   construction; retain IEEE arithmetic rather than filtering finite values.
   Do not edit `simulation.hpp`, GBM, LHS, bootstrap, or unrelated leaves.

4. Add public-property tests for the canonical mirror with input index
   `{9, 3, 9}` and source columns `path_0`, `path_1`; exact order/schema;
   singleton; NaN/infinity propagation; empty/wrong-type/mixed-column/collision
   rejection; exact raw index preservation; source destruction/mutation
   ownership; and façade-only automatic export. Avoid a duplicate mirror
   implementation in tests.

5. Run:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'AntitheticVariates|SimulationAntitheticVariatesUmbrella' --output-on-failure
   ctest --test-dir build -R 'AntitheticVariates|SimulationAntitheticVariatesUmbrella|GbmPaths|SimulationGbmPathsUmbrella|SimulationGbmFromReturnsUmbrella|LatinHypercubeSampling|SimulationLatinHypercubeUmbrella|SimulationUmbrella|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

6. Commit only the five owned paths:

   ```sh
   git add include/fincore/simulation/paths.hpp \
           src/simulation/paths.cpp \
           tests/CMakeLists.txt \
           tests/unit/antithetic_variates_test.cpp \
           tests/unit/simulation_antithetic_variates_umbrella_test.cpp
   git commit -m "feat: add antithetic path transform"
   ```

7. Dispatch a fresh independent read-only review. Fix every Critical,
   Important, or Minor finding in a new scoped commit, then rerun focused and
   full CTest. Do not amend accepted history.

### Task 105: Accept Phase 24 evidence

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-24-antithetic-path-transform-acceptance.md`

1. Record the final Task 104 implementation/fix SHA chain, API/schema/order,
   orientation mapping, source oracle, RED/GREEN evidence, raw-index and IEEE
   boundary, validation/ownership/collision policy, automatic façade, review,
   and exclusions.
2. Retain raw `tee` logs for configured focused/full CTest and fresh Apple
   headless; clean registration-audited LLVM ASan+UBSan; ImPlot/demo
   compile/link-only with build-root executable checked but never launched;
   invalid-ImGui multiline diagnostic; and actual GoogleTest FetchContent
   provenance. Treat any corrupt/time-sliced LLVM build only as a
   non-acceptance diagnostic and use a new clean retry for success.
3. Independently review the documents/logs/gates, then commit exactly the two
   docs:

   ```sh
   git add README.md \
           docs/acceptance/2026-08-12-phase-24-antithetic-path-transform-acceptance.md
   git commit -m "docs: accept phase 24 antithetic path transform"
   ```

### Task 106: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-24-antithetic-path-transform-acceptance.md`

1. Record full Task 104 implementation/fix SHA chain and direct Task 105
   acceptance SHA plus exact scopes. State Task 106 is documentation-only and
   does not amend prior commits; do not mention or predict its own SHA.
2. Run focused `AntitheticVariates|SimulationAntitheticVariatesUmbrella` and
   diff checks, then commit only this document:

   ```sh
   git commit -m "docs: trace phase 24 antithetic path transform"
   ```

### Final Phase 24 review

Audit Task 104/105/106 scope/ancestry; API/schema/order/orientation/mirror
semantics; empty/mixed/collision/nonfinite boundaries; source oracle; no
GBM/LHS/MonteCarlo drift; ownership/tests/façade export; raw gates/provenance/
no-GUI claims; and trace wording/non-self-reference. Fresh-configure Apple
headless, rerun focused/full CTest and the mandated Anaconda oracle, then
publish Spec/Task-Quality verdicts. Fix all Critical, Important, or Minor
findings in new scoped commits; do not amend accepted history.
