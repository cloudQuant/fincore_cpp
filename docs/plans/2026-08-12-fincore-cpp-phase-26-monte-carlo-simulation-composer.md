# Phase 26 native Monte Carlo simulation-composer Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a DataFrame-native, value-owning Monte Carlo simulation composer
that turns accepted historical-return GBM paths into an accepted terminal
simulation result.

**Architecture:** Add a leaf that delegates once to `gbm_from_returns`,
optionally once to `antithetic_variates`, and once to
`analyze_simulation_paths`. It owns no random engine, statistics loop, path
cache, or mutable class state. The simulation façade exports the leaf only
after the implementation and leaf review are accepted.

**Tech Stack:** C++23, cloudQuant/DataFrame, CMake, GoogleTest, accepted
native GBM/antithetic/result leaves, mandatory Anaconda Python source oracle,
Homebrew LLVM 22 ASan+UBSan, and optional local ImPlot compile/link acceptance.

---

### Task 112: Record Phase 26 design and executable plan

**Files:**

- Create: `docs/design/2026-08-12-phase-26-monte-carlo-simulation-composer-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-26-monte-carlo-simulation-composer.md`

1. Record the selected stateless composer, rejected full-class and duplicate-
   result alternatives, exact public API, fixed source-compatible annualization,
   leaf delegation, native orientation, validation ownership, source oracle,
   test matrix, gates, and exclusions.
2. Commit exactly these two documents:

   ```sh
   git add docs/design/2026-08-12-phase-26-monte-carlo-simulation-composer-design.md \
           docs/plans/2026-08-12-fincore-cpp-phase-26-monte-carlo-simulation-composer.md
   git commit -m "docs: plan phase 26 monte carlo simulation composer"
   ```

### Task 113: Add the native Monte Carlo composer leaf

**Files:**

- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `include/fincore/simulation/monte_carlo.hpp`
- Create: `src/simulation/monte_carlo.cpp`
- Create: `tests/unit/monte_carlo_simulation_test.cpp`

1. Create ignored Phase 26 SDD workspace, brief, ledger, and Task 113 report.
   Run this exact Anaconda oracle in
   `/Users/yunjinqi/Documents/new_projects/fincore` and retain raw output:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import numpy as np
   from fincore.simulation.monte_carlo import MonteCarlo
   returns = np.array([.01, .01, .01, .01])
   for antithetic in (False, True):
       result = MonteCarlo(returns).simulate(
           n_paths=2, horizon=4, antithetic=antithetic, seed=42)
       print(antithetic, result.paths.tolist(), result.statistics)
   for horizon in (0, 1):
       try:
           result = MonteCarlo(returns).simulate(
               n_paths=2, horizon=horizon, seed=42)
           print("horizon", horizon, result.paths.tolist())
       except Exception as error:
           print("horizon", horizon, type(error).__name__, str(error))
   '
   ```

2. Before production declarations, create/register a leaf-only consumer which
   includes only `<fincore/simulation/monte_carlo.hpp>`, static-asserts the
   exact function-pointer type, constructs a named DataFrame, and calls the
   planned function. Run:

   ```sh
   cmake --build build --parallel 4
   ```

   Record the genuine missing-header/symbol RED. Do not run CTest after the
   failed build.

3. Declare exactly:

   ```cpp
   [[nodiscard]] simulation_result simulate_monte_carlo(
       const dataframe& returns,
       std::size_t path_count = 1000U,
       std::size_t horizon = 252U,
       bool antithetic = false,
       std::optional<std::uint64_t> seed = std::nullopt,
       std::string_view column = returns_column);
   ```

   Include the accepted paths/result public headers; do not declare a class,
   options struct, drift/volatility/period parameter, callback, global engine,
   or overload.

4. Implement one local composition only:

   ```cpp
   dataframe paths = gbm_from_returns(
       returns, horizon, path_count, 252U, seed, column);
   if (antithetic) {
       paths = antithetic_variates(paths);
   }
   return analyze_simulation_paths(paths);
   ```

   Do not inspect/clean/sort the selected returns yourself, duplicate GBM or
   result calculations, catch/reword `input_error`, or use another random
   source. Let accepted public leaves preserve their established native rules.

5. Add public tests without private production helpers:

   - static-assert exact signature/default one-argument expression; transpose
     the constant source oracle to a named native `returns` DataFrame and
     assert raw ordinal index `{1,2,3,4}`, two path columns, exact source
     values, counts, all terminal statistics, and VaR/CVaR;
   - assert `antithetic=true` appends the two accepted `antithetic_path_N`
     columns, doubles path count, retains the source index, matches exact
     constant-oracle mirror values/statistics, and remains value-owned;
   - use nonconstant returns to compare actual false/true outputs respectively
     against direct public `analyze_simulation_paths(gbm_from_returns(...))`
     and `analyze_simulation_paths(antithetic_variates(gbm_from_returns(...)))`;
     assert same seed repeats and distinct seeds diverge without NumPy stream
     claims;
   - assert inherited missing/wrong/malformed/embedded-NUL column, all-NaN,
     one-retained, infinity, zero path-count, and overflow errors propagate as
     `input_error`; assert zero horizon reaches the result invariant and raises
     native `input_error` instead of fabricating a result;
   - mutate/destroy the source frame after the call and prove returned paths,
     statistics, and VaR/CVaR are unchanged.

6. Run:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R MonteCarloSimulation --output-on-failure
   ctest --test-dir build -R 'MonteCarloSimulation|SimulationResult|GbmPaths|AntitheticVariates|SimulationParameters|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

7. Commit exactly the five owned paths:

   ```sh
   git add CMakeLists.txt tests/CMakeLists.txt \
           include/fincore/simulation/monte_carlo.hpp \
           src/simulation/monte_carlo.cpp \
           tests/unit/monte_carlo_simulation_test.cpp
   git commit -m "feat: add native monte carlo simulation composer"
   ```

8. Dispatch a fresh independent read-only review. Correct every Critical,
   Important, or Minor finding in a new scoped commit and rerun focused/full
   CTest; never amend accepted history.

### Task 114: Export and accept the simulation composer

**Files:**

- Modify: `README.md`
- Modify: `include/fincore/simulation.hpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/monte_carlo_simulation_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-12-phase-26-monte-carlo-simulation-composer-acceptance.md`

1. Create/register a consumer that includes only `<fincore/simulation.hpp>`.
   Before adding the one Monte Carlo header facade include, build it and record
   the genuine missing-export RED. Do not use a stale binary.
2. Add only `#include <fincore/simulation/monte_carlo.hpp>` to the simulation
   facade. The consumer must call the composer through that facade and assert
   exact function type, native index/schema, all statistics/risk methods, path
   count when antithetic is enabled, and total-schema evidence that excludes an
   extra non-double output column.
3. Document source mapping/fixed 252 annualization, composition, terminal and
   zero-horizon policies, source oracle/RED/GREEN, seed/non-NumPy distinction,
   ownership, independent review, gates, and exclusions. Keep the acceptance
   document free of the Task 114 SHA until Task 115.
4. Retain raw `tee` logs for current focused/full CTest, fresh Apple headless,
   clean registration-audited Homebrew LLVM ASan+UBSan, ImPlot/demo
   compile/link-only with build-root executable checked but never launched,
   invalid-ImGui multiline diagnostic, and actual GoogleTest FetchContent
   provenance. If an LLVM attempt is corrupt/time-sliced, retain it only as a
   non-acceptance diagnostic and use a unique clean retry.
5. Dispatch a fresh independent read-only document/gate review. Fix every
   Critical, Important, or Minor finding before staging exactly:

   ```sh
   git add README.md include/fincore/simulation.hpp tests/CMakeLists.txt \
           tests/unit/monte_carlo_simulation_umbrella_test.cpp \
           docs/acceptance/2026-08-12-phase-26-monte-carlo-simulation-composer-acceptance.md
   git commit -m "docs: accept phase 26 monte carlo simulation composer"
   ```

### Task 115: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-26-monte-carlo-simulation-composer-acceptance.md`

1. Record the complete Task 113 implementation/fix chain plus direct Task 114
   acceptance SHA and exact scopes. State Task 115 is documentation-only and
   does not amend earlier commits; do not mention or predict its own SHA.
2. Run focused `MonteCarloSimulation|MonteCarloSimulationUmbrella` CTest and
   diff checks, then commit only this document:

   ```sh
   git commit -m "docs: trace phase 26 monte carlo simulation composer"
   ```

### Final Phase 26 review

Audit Task 113/114/115 scope and ancestry; API/defaults; single public-leaf
composition; fixed source-compatible `252U`; path/index/schema/order and
ownership; constant source oracle, antithetic, seed, zero-horizon and inherited
error boundaries; absence of a class/cache/RNG/drift-volatility drift; façade
export; raw gates, FetchContent provenance, no-GUI claims; and trace wording.
Fresh-configure Apple headless, rerun focused/full CTest and the mandatory
Anaconda oracle, then publish Spec/Task-Quality verdicts. Correct every
Critical, Important, or Minor finding in new scoped commits; do not amend
accepted history.
