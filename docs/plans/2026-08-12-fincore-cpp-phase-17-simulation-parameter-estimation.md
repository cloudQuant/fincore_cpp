# fincore C++ Phase 17 Simulation Parameter Estimation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task.

**Goal:** Add a deterministic, DataFrame-native annualized drift/volatility
calibration counterpart to Python `fincore.simulation.base.estimate_parameters`.

**Architecture:** A new simulation parameter leaf retrieves one genuine-double
return column, drops NaNs in source order, rejects nonfinite/insufficient data,
then computes checked annualized mean and sample volatility into a value-only
struct.  The simulation umbrella exports it only after a leaf-isolated public
consumer proves the public surface.  Random path generation remains out of
scope.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

**Baseline:** Accepted Phase 16 ends at
`29780d38bbe1c1524e56a1fc875e85d1f021a242` with 291 headless CTest cases.
Do not amend its leaf, correction, acceptance, trace, or review records.

## Global constraints

- Use only `fincore::dataframe`, `input_error`, established
  `require_double_column`, and the C++ standard library.  Do not add Eigen,
  BLAS/LAPACK, NumPy/pandas, Python embedding, a random generator, provider,
  cache, or numerical dependency.
- Add only the parameter-estimation leaf and later its simulation-umbrella
  export.  Do not modify existing stress/correlation source/header/test,
  generic DataFrame helpers, metrics/calendar behavior, third-party
  configuration, or ImPlot source.
- The public API must be exactly:

  ```cpp
  struct simulation_parameters { double drift; double volatility; };
  simulation_parameters estimate_simulation_parameters(
      const dataframe& returns,
      std::size_t periods_per_year = 252,
      std::string_view column = returns_column);
  ```

- Remove NaNs only, in input order, matching the Python source.  Reject
  infinities, fewer than two retained observations, zero periods-per-year,
  malformed/missing/non-double selected columns, and nonfinite intermediate or
  final results with `input_error`.
- Use sample standard deviation (denominator `n - 1`) and annualize drift by
  `periods_per_year`, volatility by its square root.  The raw timestamp index
  affects only established frame validation; never sort, align, resample,
  deduplicate, clip, compound, or retain it.
- Result values are fully owned scalars.  This slice excludes the standalone
  Python `annualize` helper, `SimResult`, GBM/path generation, bootstrap,
  Monte Carlo, antithetic/LHS sampling, optimization, providers, reports,
  full Python parity, and GUI runtime.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare
  `python` or `python3`.

### Task 75: Record Phase 17 design and plan

**Files:**

- Create: `docs/design/2026-08-12-phase-17-simulation-parameter-estimation-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-17-simulation-parameter-estimation.md`

- [ ] Record the source/oracle vs strict-native-boundary decision, exact API,
  exclusions, and TDD/acceptance workflow.
- [ ] Commit only these documents with:

  ```sh
  git commit -m "docs: plan phase 17 simulation parameter estimation"
  ```

### Task 76: Add the simulation parameter-estimation leaf

**Files:**

- Create: `include/fincore/simulation/parameters.hpp`
- Create: `src/simulation/parameters.cpp`
- Create: `tests/unit/simulation_parameters_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Record source oracle and public-header RED**

  Create/register the test with only
  `<fincore/simulation/parameters.hpp>` among fincore headers.  Before adding
  the header/source, run the exact oracle and preserve stdout in the ignored
  Task 76 report:

  ```sh
  cd /Users/yunjinqi/Documents/new_projects/fincore
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
  import numpy as np
  from fincore.simulation.base import estimate_parameters
  values = np.array([0.01, np.nan, -0.02, 0.03, 0.0])
  print(estimate_parameters(values, 252))
  '
  ```

  Then run `cmake --build build --parallel 4`.  Expected: compile failure
  only because the public parameter header/API is absent.  Do not treat an old
  executable or no-tests result as RED evidence.

- [ ] **Step 2: Implement checked DataFrame calibration**

  Add the header/source and library source registration.  Retrieve the selected
  genuine-double column, scan once in order to discard NaNs and reject
  infinities, require at least two retained values and positive periods, then
  compute checked mean/sample standard deviation and annualized result fields.
  Guard nonfinite intermediates/finals and return the two scalar fields by
  value.

- [ ] **Step 3: Add boundaries and run GREEN**

  Prove source finite values, NaN removal, non-default period count, custom
  columns, timestamp-order independence, sample (not population) denominator,
  missing/non-double/short/empty columns, all-NaN/single retained/infinite
  inputs, zero periods, and overflow rejection.  Run:

  ```sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'SimulationParameters|CorrelationBreakdown|StressScenarios|SimulationUmbrella|Dataframe' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ```

- [ ] **Step 4: Independently review and commit exact Task 76 scope**

  Review source fidelity, deliberate error-boundary adaptation, denominator,
  numerical guards, no random behavior, and CMake registration.  Commit only
  the five paths with:

  ```sh
  git commit -m "feat: add simulation parameter estimation"
  ```

### Task 77: Export and accept Phase 17

**Files:**

- Modify: `include/fincore/simulation.hpp`
- Create: `tests/unit/simulation_parameters_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-17-simulation-parameter-estimation-acceptance.md`

- [ ] Add an umbrella-only consumer that includes only
  `<fincore/simulation.hpp>`, uses the source fixture, and checks the two
  exact scalar fields.
- [ ] Capture true umbrella RED before exporting exactly
  `#include <fincore/simulation/parameters.hpp>`; do not run stale CTest after
  a failed build.
- [ ] Document the scalar/DataFrame adaptation, source oracle, stricter native
  boundaries, Task 76 SHA, RED/GREEN evidence, and exclusions.  Run configured
  focused/full CTest plus retained fresh Apple headless, LLVM ASan+UBSan
  headless, ImPlot+demo compile/link-only (verify actual build-root demo), and
  invalid-ImGui configure failure using a whitespace-tolerant multiline
  diagnostic match.  Do not launch a GUI.
- [ ] Independently review exact scope and commit only these five paths:

  ```sh
  git commit -m "docs: accept phase 17 simulation parameter estimation"
  ```

### Task 78: Add non-self-referential Phase 17 traceability

**Files:**

- Modify:
  `docs/acceptance/2026-08-12-phase-17-simulation-parameter-estimation-acceptance.md`

- [ ] Record full Task 76 implementation SHA and direct Task 77 initial
  acceptance SHA with distinct scopes.  State Task 78 is documentation-only
  and does not amend prior commits; do not state or anticipate its own SHA.
- [ ] Run focused `SimulationParameters|SimulationParametersUmbrella` CTest,
  diff checks, and commit only the acceptance document with:

  ```sh
  git commit -m "docs: trace phase 17 simulation parameter estimation"
  ```

### Final Phase 17 review

- [ ] Verify Task 76/77/78 ancestry/exact scopes, public leaf/umbrella
  consumption, source oracle, strict named DataFrame boundary, NaN/infinity/
  sample/period/numerical rules, retained fresh gates, documentation accuracy,
  and non-self-referential trace.
- [ ] Independently fresh-configure/build AppleClang headless, rerun focused
  and full CTest, and publish Spec/Task-Quality verdicts.  Fix any Critical,
  Important, or Minor issue in a new scoped commit and rerun final review;
  never amend accepted commits.
