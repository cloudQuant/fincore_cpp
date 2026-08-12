# fincore C++ Phase 18 GBM Path Generation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task.

**Goal:** Add a reproducible native, DataFrame-native Geometric Brownian Motion
path generator corresponding to Python
`fincore.simulation.paths.geometric_brownian_motion`.

**Architecture:** A new simulation paths leaf validates finite model inputs,
calculates the Python-compatible truncated step count, generates normal shocks
from a documented native MT19937-64/Box-Muller stream, and materializes a
time-indexed named DataFrame with one column per path.  The simulation umbrella
exports it only after an isolated consumer proves public access.  This is path
propagation, not a full Monte Carlo framework.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

**Baseline:** Accepted Phase 17 ends at
`6e877f8f9001819d46bbbbd4e497e9464b796209` with 299 headless CTest cases.
Do not amend its leaf, acceptance, trace, or review records.

## Global constraints

- Use only `fincore::dataframe`, `timestamp`, `input_error`, and C++ standard
  library facilities.  Do not add Eigen, BLAS/LAPACK, NumPy/pandas, Python
  embedding, a third-party RNG/numerical dependency, provider, cache, or GUI
  source.
- Add only the GBM paths leaf and later its simulation-umbrella export.  Do
  not modify Phase 17 parameter source/header/test, existing stress/correlation
  source/header/test, generic DataFrame helpers, metrics/calendar behavior,
  third-party configuration, or ImPlot source.
- Public API must be exactly:

  ```cpp
  inline constexpr std::string_view gbm_path_column_prefix{"path_"};
  dataframe geometric_brownian_motion(
      double initial_value,
      double annualized_drift,
      double annualized_volatility,
      double total_years,
      double time_step,
      std::size_t path_count,
      std::optional<std::uint64_t> seed = std::nullopt);
  ```

- All double model inputs must be finite; `time_step > 0`, `total_years >= 0`,
  and `path_count > 0`.  Preserve Python's truncation: `n_steps =
  trunc(total_years / time_step)`, allowing initial-only output when it is zero.
  Reject unrepresentable output sizes/indexes/labels or nonfinite intermediate
  or final model arithmetic with `input_error`.
- Do not impose a positive-price policy: finite zero/negative `initial_value`
  are propagated algebraically.  All output indexes, labels, and values are
  owned by value.
- Output is the native transpose of Python: ordinal time index
  `{0,...,n_steps}`, exactly `path_count` genuine double columns named
  `path_0...path_N`, first row `initial_value`; no extra columns.  Each next
  cell follows the source GBM equation.
- With a supplied seed, use local `std::mt19937_64` plus Box-Muller normals and
  prove same-seed native reproducibility.  Do not claim equality with NumPy
  `default_rng` random samples.  Without seed, seed from `std::random_device`.
- Exclude `gbm_from_returns`, parameter-estimation changes, antithetic/LHS,
  bootstrap, Monte Carlo, SimResult, correlation/stress behavior,
  optimization/providers, reports, full NumPy RNG parity, and GUI runtime.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never bare
  `python` or `python3`.

### Task 79: Record Phase 18 design and plan

**Files:**

- Create: `docs/design/2026-08-12-phase-18-gbm-path-generation-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-18-gbm-path-generation.md`

- [ ] Record the native-transpose shape, source equation/oracle, RNG
  non-parity boundary, validation, exclusions, and TDD/acceptance workflow.
- [ ] Commit only these documents:

  ```sh
  git commit -m "docs: plan phase 18 gbm path generation"
  ```

### Task 80: Add the native GBM paths leaf

**Files:**

- Create: `include/fincore/simulation/paths.hpp`
- Create: `src/simulation/paths.cpp`
- Create: `tests/unit/gbm_paths_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Record source oracle and public-header RED**

  Create/register a test whose only fincore include is
  `<fincore/simulation/paths.hpp>`.  Before creating production header/source,
  preserve the exact source oracle in the ignored Task 80 report:

  ```sh
  cd /Users/yunjinqi/Documents/new_projects/fincore
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
  from fincore.simulation.paths import geometric_brownian_motion
  for args in ((100.0, 0.1, 0.0, 1.0, 0.25, 2),
               (100.0, 0.0, 0.0, 0.1, 0.25, 1)):
      result = geometric_brownian_motion(*args, seed=42)
      print(args, result.shape, result.tolist())
  '
  ```

  Run `cmake --build build --parallel 4`.  Expected: compilation fails only
  because the public paths header/API is absent.  Never use no-tests or an old
  executable as RED evidence.

- [ ] **Step 2: Implement validated native path propagation**

  Add the header/source and CMake registration.  Use checked finite/shape
  validation, Python-compatible truncation, ordinal index/name creation,
  MT19937-64, a local safe uniform-to-Box-Muller normal helper, and per-path
  iterative GBM propagation.  Validate every sensitive multiplication,
  exponent, and result before loading the DataFrame.

- [ ] **Step 3: Add boundary tests and run GREEN**

  Leaf-isolated tests must prove: zero-volatility Python sequence and native
  transpose; zero-step source shape; exact typed schema/index/path ordering;
  same seed produces identical tables and differing seeds produce at least one
  different nonzero-volatility cell; unseeded valid construction; zero and
  negative initial values; all finite/step/time/path-count invalid cases;
  output ownership; and overflow rejection.  Run:

  ```sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'GbmPaths|SimulationParameters|CorrelationBreakdown|StressScenarios|SimulationUmbrella|Dataframe' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ```

- [ ] **Step 4: Independently review and commit exact Task 80 scope**

  Review equation/orientation/RNG boundaries, finite and representability
  guards, deterministic source test, no unintended simulation behavior, and
  CMake registration.  Commit only the five paths:

  ```sh
  git commit -m "feat: add native gbm path generation"
  ```

### Task 81: Export and accept Phase 18

**Files:**

- Modify: `include/fincore/simulation.hpp`
- Create: `tests/unit/simulation_gbm_paths_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-18-gbm-path-generation-acceptance.md`

- [ ] Add a public-only consumer that includes only
  `<fincore/simulation.hpp>`, calls a zero-volatility path, and checks the
  named time-indexed shape/cells.
- [ ] Capture true umbrella RED before adding exactly
  `#include <fincore/simulation/paths.hpp>`; do not run stale CTest after the
  expected failed build.
- [ ] Record source oracle, native transpose/RNG boundary, validation,
  Task80 SHA, RED/GREEN, no-GUI scope, and exclusions.  Run configured
  focused/full CTest and retain fresh Apple headless, LLVM ASan+UBSan headless,
  ImPlot+demo compile/link-only (actual build-root demo), and invalid-ImGui
  whitespace-tolerant diagnostic evidence.  Do not launch GUI.
- [ ] Independently review and commit only these five paths:

  ```sh
  git commit -m "docs: accept phase 18 gbm path generation"
  ```

### Task 82: Add non-self-referential Phase 18 traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-18-gbm-path-generation-acceptance.md`

- [ ] Record full Task80 implementation SHA and direct Task81 initial
  acceptance SHA and their scopes.  State Task82 is documentation-only and
  does not amend earlier commits; do not state or anticipate its own SHA.
- [ ] Run focused `GbmPaths|SimulationGbmPathsUmbrella` CTest, diff checks,
  then commit only the acceptance document:

  ```sh
  git commit -m "docs: trace phase 18 gbm path generation"
  ```

### Final Phase 18 review

- [ ] Verify Task80/81/82 ancestry/scope, public leaf/umbrella isolation,
  source oracle, equation/transposed named schema, seeded-native/no-NumPy RNG
  boundary, finite/shape/overflow behavior, logs/docs, fresh gates, and
  non-self-referential trace.
- [ ] Independently fresh-configure/build AppleClang headless, rerun focused
  and full CTest, and publish Spec/Task-Quality verdicts.  Fix every Critical,
  Important, or Minor issue in a new scoped commit and rerun final review; do
  not amend accepted commits.
