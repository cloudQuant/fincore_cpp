# fincore C++ Phase 19 GBM-from-Returns Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task. Steps use checkbox syntax for tracking.

**Goal:** Add a source-compatible, DataFrame-native gbm_from_returns
convenience API that composes accepted calibration and GBM path propagation.

**Architecture:** Extend the existing simulation/paths leaf rather than adding
a second random-path abstraction. The wrapper delegates to public parameter
estimation, derives the exact source daily/duration inputs, delegates to public
GBM once, removes its initial price row, and materializes owned named
cumulative-return columns. The existing simulation facade already exports the
paths leaf, so acceptance proves public access with an isolated consumer
instead of adding a redundant include.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through /Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

**Baseline:** Accepted Phase 18 ends at
d684d712263dc5770cf9686d092b051dc1137704 with 309 headless CTest cases.
Do not amend its leaf, acceptance, trace, or review records.

## Global Constraints

- Use only existing fincore::dataframe, timestamp, input_error, public
  estimate_simulation_parameters, public geometric_brownian_motion, and C++
  standard-library facilities. Do not add Eigen, BLAS/LAPACK, NumPy/pandas,
  Python embedding, a new RNG, providers, caches, or GUI source.
- Modify only the accepted paths leaf for the implementation task. Do not alter
  Phase 17 parameter source/header/test, Phase 18 raw GBM semantics, existing
  stress/correlation leaves, generic DataFrame helpers, metrics/calendar
  behavior, third-party configuration, or ImPlot source.
- Public API must be exactly:

  ~~~cpp
  dataframe gbm_from_returns(
      const dataframe& returns,
      std::size_t horizon = 252U,
      std::size_t path_count = 1000U,
      std::size_t periods_per_year = 252U,
      std::optional<std::uint64_t> seed = std::nullopt,
      std::string_view column = returns_column);
  ~~~

- Delegate exactly once to
  estimate_simulation_parameters(returns, periods_per_year, column) and once
  to geometric_brownian_motion. Preserve the source transformations
  dt=1/periods_per_year, mu_daily=drift/periods_per_year,
  sigma_daily=volatility/sqrt(periods_per_year), and
  T=horizon/periods_per_year.
- Preserve the source double-transform-plus-truncation behavior: horizon is
  not an exact output-row guarantee. For constant 0.01 returns with horizon=3
  and periods_per_year=5, native output must contain exactly two rows and
  values 0.002002001334000303, 0.004008010677341911 within 1e-12.
- Result is the native transpose of Python's stripped price array: columns
  path_0 through path_N are genuine doubles in numeric order, their values are
  delegated price values after index zero minus one, and ordinal indexes are
  retained underlying steps {1,...,n_steps}. A zero-step result has an empty
  index and exactly path_count empty genuine-double path columns.
- Inherit existing estimator rules for returns and existing GBM rules for path
  count/RNG/path arithmetic. Reject bounded selector names containing embedded
  NUL before any DataFrame C-string use, and reject nonfinite wrapper
  intermediates/final price - 1.0 values with input_error.
- Explicit seed is native repeatability only; do not claim NumPy random-stream
  equivalence. Omitted seed uses existing native behavior.
- Exclude parameter/GBM behavior changes, antithetic/LHS, bootstrap,
  MonteCarlo, SimResult, simulation VaR/CVaR, correlation/stress behavior,
  optimization, providers, reports, full NumPy RNG parity, GUI runtime, and
  packaging/release work.
- All Python commands must use
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python; never bare
  python or python3.

### Task 83: Record Phase 19 design and plan

**Files:**

- Create: docs/design/2026-08-12-phase-19-gbm-from-returns-design.md
- Create: docs/plans/2026-08-12-fincore-cpp-phase-19-gbm-from-returns.md

- [ ] **Step 1: Record source-compatible boundary and public shape**

  Write the exact public signature, source transformation, native
  transpose/index policy, RNG non-parity boundary, inherited validation, and
  exclusions. Include the source values below verbatim.

  ~~~text
  horizon=4, path_count=2, periods_per_year=4:
  0.0025031276057951857, 0.005012520859401182,
  0.0075281954445340915, 0.010050167084168393
  horizon=3, path_count=1, periods_per_year=5:
  0.002002001334000303, 0.004008010677341911
  horizon=0, path_count=2, periods_per_year=4:
  two empty paths
  ~~~

- [ ] **Step 2: Record TDD and acceptance constraints**

  State that a paths-header-only test must RED on absent API, that
  nonzero-volatility native behavior is compared to the public GBM leaf rather
  than NumPy, and that the later simulation-umbrella consumer has no separate
  include mutation because simulation.hpp already includes paths.hpp.

- [ ] **Step 3: Verify document scope and commit**

  Run:

  ~~~sh
  git diff --check
  git add docs/design/2026-08-12-phase-19-gbm-from-returns-design.md \
          docs/plans/2026-08-12-fincore-cpp-phase-19-gbm-from-returns.md
  git diff --cached --check
  git commit -m "docs: plan phase 19 gbm from returns"
  ~~~

### Task 84: Add the calibrated GBM paths leaf API

**Files:**

- Modify: include/fincore/simulation/paths.hpp
- Modify: src/simulation/paths.cpp
- Modify: tests/unit/gbm_paths_test.cpp

**Interfaces:**

- Consumes: simulation_parameters estimate_simulation_parameters(const
  dataframe&, std::size_t, std::string_view) and dataframe
  geometric_brownian_motion(double, double, double, double, double,
  std::size_t, std::optional<std::uint64_t>).
- Produces: dataframe gbm_from_returns(const dataframe&, std::size_t,
  std::size_t, std::size_t, std::optional<std::uint64_t>, std::string_view) in
  the paths public header.

- [ ] **Step 1: Record source oracle and add public-header-only RED**

  Before production edits, append this Anaconda run and its exact output to the
  ignored Task 84 report:

  ~~~sh
  cd /Users/yunjinqi/Documents/new_projects/fincore
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
  import numpy as np
  from fincore.simulation.paths import gbm_from_returns
  for horizon, paths, frequency in ((4, 2, 4), (3, 1, 5), (0, 2, 4)):
      result = gbm_from_returns(np.array([0.01, 0.01, 0.01, 0.01]),
                                horizon=horizon, n_paths=paths,
                                frequency=frequency, seed=42)
      print((horizon, paths, frequency), result.shape, result.tolist())
  '
  ~~~

  Add a test whose only fincore include is paths.hpp. It calls:

  ~~~cpp
  fincore::gbm_from_returns(
      fincore::make_series({19, -3, 19, 7}, "returns",
                           {0.01, 0.01, 0.01, 0.01}),
      4U, 2U, 4U, 42U);
  ~~~

  Run cmake --build build --parallel 4. Expected: compilation fails only
  because gbm_from_returns is absent; do not run CTest after a failed build.

- [ ] **Step 2: Declare and implement exact public composition**

  Include the accepted parameter declaration in paths.hpp and add the exact
  signature. In paths.cpp, own and validate column before C-string boundaries;
  invoke public parameter estimation exactly once; calculate
  periods_as_double, sqrt(periods_as_double), daily drift/volatility,
  duration, and step with finite checks; invoke public GBM exactly once with
  initial value 1.0; remove its first row; subtract 1.0 from every retained
  price; and load owned index/value vectors under existing path_N labels.
  Preserve an empty typed path table when delegated GBM has only its initial
  row. Convert allocation/length exceptions to input_error consistently with
  the existing leaf.

- [ ] **Step 3: Add concrete leaf tests**

  Add these test groups to tests/unit/gbm_paths_test.cpp:

  ~~~cpp
  TEST(GbmPaths, CalibratedPathsMatchZeroVolatilityPythonOracleAndSchema)
  TEST(GbmPaths, CalibratedPathsPreserveSourceDoubleTruncationAndZeroStepShape)
  TEST(GbmPaths, CalibratedPathsDelegateParametersAndNativeGbmExactly)
  TEST(GbmPaths, CalibratedPathsOwnResultAndPreserveNamedReturnColumn)
  TEST(GbmPaths, CalibratedPathsRejectStructuralSelectorAndNumericalBoundaries)
  ~~~

  The first asserts index {1,2,3,4}, exactly two double path columns, and the
  four source values at 1e-12. The second asserts the exact two-row
  horizon=3, periods_per_year=5 values and two empty typed columns at horizon
  zero. The third compares every nonzero-volatility cell to explicit public
  estimator-plus-public-GBM materialization under the same native seed, and
  demonstrates same-seed equality/different-seed divergence. The fourth uses
  strategy plus a nonmonotonic/duplicate raw return index and mutates
  source/name storage after return. The fifth covers empty/wrong-type/short
  columns, periods_per_year=0, path_count=0, NaN-only/one retained/infinite
  returns inherited from estimation, embedded-NUL column name, output-size
  limits, and nonfinite wrapper/path arithmetic.

- [ ] **Step 4: Run GREEN and scoped regression**

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'GbmPaths|SimulationParameters|SimulationGbmPathsUmbrella|Dataframe' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ~~~

- [ ] **Step 5: Independently review and commit exact leaf scope**

  Review public API/defaults, source double transformation, one-public-call
  composition, native schema/index, finite/error boundaries, ownership, and
  no Phase 17/18 behavior drift. Commit only the three listed paths:

  ~~~sh
  git commit -m "feat: add calibrated gbm paths"
  ~~~

### Task 85: Accept public simulation-facade access

**Files:**

- Create: tests/unit/simulation_gbm_from_returns_umbrella_test.cpp
- Modify: tests/CMakeLists.txt
- Modify: README.md
- Create: docs/acceptance/2026-08-12-phase-19-gbm-from-returns-acceptance.md

**Interfaces:**

- Consumes: the Task 84 gbm_from_returns declaration through the existing
  simulation.hpp inclusion of paths.hpp.
- Produces: an acceptance record and a public-only facade consumer.

- [ ] **Step 1: Add isolated umbrella consumer and verify public access**

  Create a consumer that includes only simulation.hpp among fincore headers.
  It calls the constant-return horizon=4, path_count=2, periods_per_year=4,
  seed=42 fixture and asserts index {1,2,3,4}; path_0/path_1 genuine-double
  schema; exactly two columns; and all four source-oracle cells. Register it
  in tests/CMakeLists.txt. There is no second umbrella RED/include edit:
  simulation.hpp already exports paths.hpp; Task 84 paths-header RED is the
  real missing-public-API evidence.

- [ ] **Step 2: Record README and acceptance boundaries**

  Record Task 84 full SHA/scope, source-compatible daily/duration transform,
  retained physical-step indexes, zero-step table, seed/no-NumPy boundary,
  inherited estimator and GBM validation, absence of GUI runtime, and all
  excluded Monte Carlo/bootstrap/antithetic/LHS/SimResult work. Do not claim a
  new umbrella include was added.

- [ ] **Step 3: Run configured and fresh gates**

  Run current-build focused and full CTest:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'GbmPaths|SimulationGbmPathsUmbrella|SimulationGbmFromReturnsUmbrella|SimulationParameters|Dataframe' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ~~~

  Retain fresh directories/logs for AppleClang Debug headless; Homebrew LLVM
  22 Debug with ASan+UBSan headless; ImPlot/demo compile/link-only with the
  actual build-root demo existence check but no launch; and invalid
  FINCORE_IMGUI_SOURCE_DIR expected configure failure checked with:

  ~~~sh
  rg -U -P 'FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files' configure.log
  ~~~

- [ ] **Step 4: Independently review and commit exact acceptance scope**

  Review facade isolation, source oracle/known truncation boundary, docs,
  retained gate evidence, and scope. Commit only the four listed paths:

  ~~~sh
  git commit -m "docs: accept phase 19 gbm from returns"
  ~~~

### Task 86: Add non-self-referential Phase 19 traceability

**Files:**

- Modify: docs/acceptance/2026-08-12-phase-19-gbm-from-returns-acceptance.md

- [ ] **Step 1: Record completed implementation and acceptance identities**

  Append the full Task 84 implementation SHA and exact three-path scope, plus
  the direct Task 85 initial acceptance SHA and exact four-path scope. State
  that Task 86 is documentation-only and does not amend earlier commits. Do
  not state or anticipate the Task 86 commit SHA.

- [ ] **Step 2: Verify and commit only traceability**

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'GbmPaths|SimulationGbmFromReturnsUmbrella' --output-on-failure
  git diff --check
  git add docs/acceptance/2026-08-12-phase-19-gbm-from-returns-acceptance.md
  git diff --cached --check
  git commit -m "docs: trace phase 19 gbm from returns"
  ~~~

### Final Phase 19 review

- [ ] **Step 1: Audit scope and implementation chain**

  Verify Task 84/85/86 linear ancestry and exact scopes; leaf/header and
  facade-only consumer isolation; exact API/defaults; source conversion,
  truncation, schema/index, ownership, validation, native RNG boundary, and
  no scope creep.

- [ ] **Step 2: Reproduce independent source and build evidence**

  Rerun the Anaconda deterministic source oracle, fresh-configure/build
  AppleClang Debug headless with ImPlot off, run focused
  GbmPaths|SimulationGbmFromReturnsUmbrella, then full CTest. Check retained
  Apple/LLVM/ImPlot/invalid-ImGui logs and ensure no GUI was launched.

- [ ] **Step 3: Publish verdict and repair findings**

  Publish Spec and Task-quality verdicts. Fix every Critical, Important, or
  Minor finding in a new scoped commit, run one scoped re-review, and do not
  amend accepted commits.
