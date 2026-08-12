# fincore C++ Phase 20 Bootstrap Distribution Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to
> implement this plan task-by-task.

**Goal:** Add a reproducible DataFrame-native bootstrap distribution core for
Python `fincore.simulation.bootstrap.bootstrap` and its seven named built-in
statistics.

**Architecture:** A new simulation leaf validates and owns a selected
return-column name, drops NaNs, rejects signed infinity, samples retained
values with replacement from a local MT19937-64 stream, evaluates one named
statistic per sample, then materializes an ordinal one-column DataFrame.  The
simulation facade exports that leaf only after a separate public-only consumer
proves it.  Confidence intervals and summary reporting stay separate.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, source
oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`,
Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot compile/link
verification.

**Baseline:** Phase 19 ends at
`deae354af86bab2f79ad5de79d7abc591630048c` with 315 headless CTest cases.
The Phase 19 correction is documentation-only; do not amend or change Phase
18/19 leaf, acceptance, trace, or correction records.

---

## Global constraints

- Use only `fincore::dataframe`, `timestamp`, `input_error`, and the C++
  standard library.  Do not add NumPy/pandas/Python embedding, an RNG or
  numerical third-party dependency, generic DataFrame helper changes,
  providers, caches, GUI source, or source-generator edits.
- Add only the bootstrap distribution leaf and its later simulation-facade
  export.  Do not change GBM, calibrated GBM, parameters, correlation, stress,
  metrics, attribution, CMake dependency policy, or optional ImPlot code.
- Public API is exactly:

  ```cpp
  enum class bootstrap_statistic {
      mean,
      sample_standard_deviation,
      median,
      sharpe_ratio,
      sortino_ratio,
      minimum,
      maximum,
  };

  inline constexpr std::string_view bootstrap_statistic_column{"bootstrap"};

  dataframe bootstrap_distribution(
      const dataframe& returns,
      std::size_t resample_count = 10000U,
      bootstrap_statistic statistic = bootstrap_statistic::mean,
      std::optional<std::uint64_t> seed = std::nullopt,
      std::string_view column = returns_column);
  ```

- The selected column must be genuine `double`, index-length-matched,
  nonempty, and NUL-free.  Preserve source order only; do not use timestamps
  beyond input-frame validation.  Drop only NaNs before sampling, reject signed
  infinity, and throw `input_error` if no finite retained value remains.
- Each sample has retained-input length and replacement sampling.  A supplied
  seed repeats native output within the supported build; use a fresh local
  `std::mt19937_64` and an unbiased rejection-mapped bounded index.  Do not
  claim NumPy `default_rng` stream equivalence.  Missing seed uses local
  `std::random_device`.
- Output has ordinal index `{0,...,resample_count-1}` and exactly one
  genuine-`double` column named `bootstrap`; count zero is a valid typed empty
  result after input/statistic validation.  Results own labels/index/values.
- `sample_standard_deviation` uses `ddof=1`; singleton samples are `NaN`.
  Sharpe and Sortino use `sqrt(252)`, singleton `NaN`, and zero denominator
  gives `NaN` only when sample mean is zero and positive infinity otherwise.
  Sortino zeroes only values strictly greater than zero before its sample
  standard deviation.  These deliberate degenerate IEEE outputs are allowed.
  Reject unknown enum, unsafe output sizes, allocation failures, and finite
  arithmetic overflow with `input_error`.
- Exclude `bootstrap_ci`, `bootstrap_summary`, custom callable statistics,
  percentile/interpolation semantics, full Python RNG parity, Monte Carlo,
  antithetic/LHS methods, SimResult, providers/reports, packaging, and GUI
  runtime.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never invoke
  bare `python` or `python3`.

### Task 87: Record the Phase 20 design and plan

**Files:**

- Create: `docs/design/2026-08-12-phase-20-bootstrap-distribution-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-20-bootstrap-distribution.md`

1. Record option A (distribution core with all built-ins), DataFrame shape,
   finite/degenerate behavior, source/native RNG boundary, oracle, exclusions,
   and evidence policy.
2. Commit only the two documents:

   ```sh
   git add docs/design/2026-08-12-phase-20-bootstrap-distribution-design.md \
           docs/plans/2026-08-12-fincore-cpp-phase-20-bootstrap-distribution.md
   git commit -m "docs: plan phase 20 bootstrap distribution"
   ```

### Task 88: Add the bootstrap distribution leaf

**Files:**

- Create: `include/fincore/simulation/bootstrap.hpp`
- Create: `src/simulation/bootstrap.cpp`
- Create: `tests/unit/bootstrap_distribution_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

1. **Create the ignored SDD workspace and capture source oracle.**  At task
   start, create
   `.superpowers/sdd/2026-08-12-fincore-cpp-phase-20-bootstrap-distribution/`
   with task brief, ledger, and ignored `task-88-report.md`.  In
   `/Users/yunjinqi/Documents/new_projects/fincore`, run:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import math
   import numpy as np
   from fincore.simulation.bootstrap import bootstrap
   values = np.array([0.01, -0.01, 0.02, 0.0])
   for name in ("mean", "std", "median", "sharpe", "sortino", "min", "max"):
       print(name, bootstrap(values, n_samples=5, statistic=name, seed=42).tolist())
   print("constant", bootstrap(np.array([0.01, 0.01, 0.01]), 4, "std", 42).tolist())
   print("singleton", bootstrap(np.array([0.01]), 3, "sharpe", 42).tolist())
   print("nan_drop", bootstrap(np.array([0.01, np.nan, 0.01]), 3, "mean", 42).tolist())
   print("zero", bootstrap(np.array([0.01]), 0, "mean", 42).tolist())
   '
   ```

   Record values and known NumPy warnings truthfully.  Add/register a test
   whose only fincore include is `<fincore/simulation/bootstrap.hpp>`.  Before
   adding production header/source, run `cmake --build build --parallel 4`.
   Expected: only the missing public bootstrap header/API causes compilation to
   fail.  Do not use an old executable or no-tests result as RED evidence.

2. **Implement the minimal leaf.**  Add header/source and CMake registrations.
   Copy a checked NUL-free selector into owned `std::string`; validate its
   double column before sampling.  Build a retained vector by dropping NaNs and
   rejecting infinities.  Validate enum and result row/index/capacity bounds
   before allocation.  Use a local engine, rejection-mapped index helper,
   value-copy sample, named-statistic switch, ordinal index, and one typed
   `bootstrap` column.  Catch allocation/length exceptions and translate to
   `input_error`.  Do not call private DataFrame internals or modify prior
   simulation sources.

3. **Write GREEN tests.**  The leaf test must prove the exact public signature
   and schema; deterministic source-compatible constant/singleton/NaN-drop/
   zero-count cases; all seven statistic cases; nonconstant row-wise
   relationships (mean/median/extrema/std/ratios); same-seed equality and
   differing-seed distinction; unseeded finite output; raw input index
   irrelevance; ownership after local source mutation/destruction; missing,
   non-double, short, empty, NUL, all-NaN, signed-infinite, unknown-enum,
   unrepresentable, and allocation boundary failures.  Do not assert nonzero
   sample values against NumPy's different random generator.

4. **Run GREEN and independent scoped review.**

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'BootstrapDistribution|GbmPaths|SimulationParameters|CorrelationBreakdown|StressScenarios|SimulationUmbrella|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

   A fresh reviewer must check the source oracle, actual leaf-only RED,
   exact API/schema, non-NumPy RNG boundary, all enum/degenerate cases,
   NUL/ownership/overflow guards, CMake registration, and exact five-path
   scope.  Record both Spec and Task-Quality verdicts.  Fix every Critical,
   Important, or Minor finding in a new scoped commit and re-review; do not
   amend accepted work.

5. **Commit only Task 88's owned paths.**

   ```sh
   git add CMakeLists.txt tests/CMakeLists.txt \
           include/fincore/simulation/bootstrap.hpp \
           src/simulation/bootstrap.cpp tests/unit/bootstrap_distribution_test.cpp
   git commit -m "feat: add bootstrap distribution"
   ```

### Task 89: Export and accept Phase 20

**Files:**

- Modify: `include/fincore/simulation.hpp`
- Create: `tests/unit/simulation_bootstrap_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-20-bootstrap-distribution-acceptance.md`

1. Add a public-only consumer that includes only `<fincore/simulation.hpp>`;
   it must call the accepted bootstrap API and assert exact one-column schema,
   ordinal index, and deterministic all-equal values.  Before adding exactly
   `#include <fincore/simulation/bootstrap.hpp>` to the facade, capture a
   fresh true umbrella RED.  Do not run CTest after a failed build.
2. Record Task 88 full SHA, public boundary, source/native RNG distinction,
   finite input and degenerate IEEE semantics, RED/GREEN, full exclusions, and
   actual dependency provenance.  Retain raw `tee` logs for fresh gates; do
   not say GoogleTest is local unless the used CMake cache proves it.
3. Run configured focused/full CTest, then fresh AppleClang headless and
   Homebrew LLVM ASan+UBSan headless configure/build/CTest.  Run optional
   ImPlot/demo compile/link only and verify the actual build-root demo path
   without launching it.  Run the invalid-ImGui configure gate and use a
   whitespace-tolerant multiline match for its wrapped diagnostic.  Record
   exact commands, exits, warnings, and source/fetch provenance.
4. Independently review public isolation, docs evidence, all gates, and scope;
   commit exactly the five Task 89 paths:

   ```sh
   git add include/fincore/simulation.hpp tests/CMakeLists.txt \
           tests/unit/simulation_bootstrap_umbrella_test.cpp README.md \
           docs/acceptance/2026-08-12-phase-20-bootstrap-distribution-acceptance.md
   git commit -m "docs: accept phase 20 bootstrap distribution"
   ```

### Task 90: Add non-self-referential Phase 20 traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-20-bootstrap-distribution-acceptance.md`

1. Record the full Task 88 implementation SHA and direct Task 89 initial
   acceptance SHA plus their exact scopes.  State that Task 90 is
   documentation-only and does not amend prior commits.  Do not write or
   predict its own SHA.
2. Run focused `BootstrapDistribution|SimulationBootstrapUmbrella` CTest and
   diff checks, then commit only the acceptance document:

   ```sh
   git add docs/acceptance/2026-08-12-phase-20-bootstrap-distribution-acceptance.md
   git commit -m "docs: trace phase 20 bootstrap distribution"
   ```

### Final Phase 20 review

1. Audit Task 88/89/90 ancestry and exact scopes; leaf/umbrella isolation;
   source oracle; API/schema; Python/native RNG distinction; all statistic,
   NaN/infinity, degenerate, ownership, and capacity boundaries; raw gate
   logs/dependency provenance; README/acceptance precision; and non-self-
   referential trace.
2. Fresh-configure an AppleClang headless build, rerun focused and full CTest,
   and publish both Spec and Task-Quality verdicts.  Any Critical, Important,
   or Minor issue requires a new scoped correction and fresh final re-review;
   no historical commit is amended.
