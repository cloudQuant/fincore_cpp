# fincore C++ Phase 16 Correlation-Breakdown Matrix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task.

**Goal:** Add a deterministic, DataFrame-native named matrix counterpart to
Python `fincore.simulation.scenarios.generate_correlation_breakdown`.

**Architecture:** A new simulation correlation leaf copies/validates explicit
asset-name views, materializes an ordinal-indexed genuine-double square
DataFrame, and fills the diagonal with one and off-diagonal cells with the
literal finite target.  The simulation umbrella exports it only after a
leaf-isolated consumer proves the public surface.  No correlation estimator,
PSD repair, path shock, or random simulator is introduced.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

**Baseline:** Accepted Phase 15 ends at
`ea8eff86f247f8e005523c9cd3c0edec4173978c` with 283 headless CTest cases.
Do not amend its leaf, acceptance, trace, or review records.

## Global Constraints

- Use only `fincore::dataframe`, `timestamp`, `input_error`, and the C++
  standard library.  Do not add Eigen, BLAS/LAPACK, NumPy/pandas, Python
  embedding, a covariance/correlation estimator, random generator, provider,
  cache, or numerical dependency.
- Add only this correlation-matrix leaf and later its simulation-umbrella
  export.  Do not modify existing stress-scenario source/header/test, factor
  code, generic DataFrame helpers, metrics/calendar behavior, third-party
  configuration, or ImPlot source.
- Every asset name is nonempty, contains no embedded NUL, and is distinct as a
  complete `std::string_view`.  Copy validated names before a DataFrame
  C-string call.  Bounded non-NUL-terminated views are valid; embedded-NUL or
  duplicate/empty names throw `fincore::input_error`.
- `target_correlation` must be finite.  Do **not** add a PSD policy or clamp
  finite values: all finite targets, including those outside conventional
  correlation bounds, are retained in off-diagonal cells.  The diagonal is
  exactly `1.0`.
- For `N` names, output has exact ordinal index `{0,...,N-1}`, exactly `N`
  genuine `double` columns in supplied name order, and no extras.  It has
  value `1.0` where row and column positions match and target otherwise.
  Guard index/count arithmetic representability before allocation.  The empty
  name span is valid and produces an empty index with zero data columns.
- Return values and labels must be fully owned.  No source time-series index,
  alignment, sort, deduplication, resampling, factor/model state, or metadata
  can influence the result.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare
  `python` or `python3`.
- Exclude estimation/PSD repair/covariance conversion, applying a correlation
  shock to returns, stress-scenario changes, portfolio optimization, Monte
  Carlo/GBM/bootstrap, providers/caches, reports/releases, full Python parity,
  and GUI runtime.

---

### Task 72: Add the named correlation-breakdown matrix leaf

**Files:**

- Create: `include/fincore/simulation/correlation.hpp`
- Create: `src/simulation/correlation.cpp`
- Create: `tests/unit/correlation_breakdown_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Produces:**

~~~cpp
dataframe generate_correlation_breakdown(
    std::span<const std::string_view> asset_names,
    double target_correlation = 1.0);
~~~

- [ ] **Step 1: Record a source-derived oracle and create a public-header RED test**

  Create `tests/unit/correlation_breakdown_test.cpp`, include only
  `<fincore/simulation/correlation.hpp>` among fincore headers, and register
  it in `tests/CMakeLists.txt`.  Do not create the public header or register
  the source before RED is captured.

  Run this exact source oracle from the local Python checkout and save complete
  stdout in the ignored Task 72 report:

  ~~~sh
  cd /Users/yunjinqi/Documents/new_projects/fincore
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
  from fincore.simulation.scenarios import generate_correlation_breakdown
  for target in (1.0, 0.25, -0.5):
      print("target", repr(target))
      print(generate_correlation_breakdown(3, target).tolist())
  print("empty", generate_correlation_breakdown(0, 0.25).tolist())
  '
  ~~~

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ~~~

  Expected: compilation fails only because the public correlation header/API
  is absent.  Do not treat an old executable or no-tests result as RED
  evidence.

- [ ] **Step 2: Implement strict named matrix materialization**

  Create the header/source.  Privately own/validate names, validate finite
  target, check count/index representability, and materialize each double
  column in caller order.  For each row/column position write exact `1.0` on
  the diagonal and the target elsewhere.  Empty input must load an empty index
  and no columns.  Register only `src/simulation/correlation.cpp` in the
  library source list.

- [ ] **Step 3: Add boundary regressions and run GREEN**

  The leaf-isolated public-header test must prove:

  - source oracle values for target `1.0`, `0.25`, and `-0.5`; exact ordinal
    index, caller-order genuine-double schema, and no extra columns;
  - one-asset `1x1` diagonal behavior, valid empty `0x0` output, and finite
    out-of-range targets pass through unchanged off diagonal without a PSD
    policy;
  - empty/duplicate/embedded-NUL asset names and nonfinite target values throw
    `fincore::input_error`; a bounded non-NUL-terminated valid name succeeds;
  - outputs remain unchanged after source name-storage mutation/destruction.

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'CorrelationBreakdown|StressScenarios|SimulationUmbrella|Dataframe' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ~~~

  Expected: focused/source/edge tests and full CTest pass; diff check is
  empty.

- [ ] **Step 4: Review and commit only Task 72 paths**

  Review named schema/order, exact source arithmetic, finite-only policy,
  empty result, string ownership, no accidental stress-engine change, and
  CMake registration.  Stage exactly the five listed paths and commit:

  ~~~sh
  git commit -m "feat: add correlation breakdown matrix"
  ~~~

  Expected: one exact implementation commit plus ignored RED/oracle/GREEN/
  scope evidence.  An independent reviewer must return Spec and Task-Quality
  verdicts before Task 73 starts.

### Task 73: Export the matrix and accept Phase 16

**Files:**

- Modify: `include/fincore/simulation.hpp`
- Create: `tests/unit/simulation_correlation_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-16-correlation-breakdown-acceptance.md`

- [ ] **Step 1: Add a simulation-umbrella-only RED consumer**

  The new consumer includes only `<fincore/simulation.hpp>` among fincore
  headers.  It calls the named constructor for a small target, checks raw
  ordinal index/order/schema/cells, and has no correlation/stress/metric/source
  leaf include.

- [ ] **Step 2: Run umbrella RED, then export exactly one leaf include**

  ~~~sh
  cmake --build build --parallel 4
  ~~~

  Expected: the build fails solely because the simulation umbrella does not
  export the new function.  Do not run a stale test executable after failure.
  Then add exactly `#include <fincore/simulation/correlation.hpp>` to the
  simulation umbrella.

- [ ] **Step 3: Write acceptance/docs and run GREEN plus fresh gates**

  README and acceptance must state the named DataFrame adaptation, exact
  diagonal/off-diagonal rule, no PSD policy, empty output, strict validation,
  Task 72 full SHA, source oracle/RED/GREEN evidence, and exclusions.  State
  no GUI runtime claim.

  Run configured focused/full CTest, then retain every fresh command output
  with `tee` in non-overlapping directories.  Positive gates must complete
  CTest before scans; no GUI is launched.  Retain Apple headless,
  Homebrew-LLVM ASan+UBSan headless, ImPlot+demo compile/link-only (verify the
  actual build-root `fincore_implot_demo` path), and expected invalid-ImGui
  configure failure using a whitespace-tolerant multiline diagnostic search.
  If a transient FetchContent/network failure occurs, retain it and use a new
  retry directory for acceptance proof; do not overwrite evidence.

- [ ] **Step 4: Independently review and commit exact Task 73 scope**

  Independent review must verify public-only consumption, docs/log claims,
  exact scope, fresh gates, and no-GUI wording.  Commit only the five owned
  paths with:

  ~~~sh
  git commit -m "docs: accept phase 16 correlation breakdown matrix"
  ~~~

### Task 74: Add non-self-referential Phase 16 traceability

**Files:**

- Modify:
  `docs/acceptance/2026-08-12-phase-16-correlation-breakdown-acceptance.md`

- [ ] Record full Task 72 implementation SHA and direct Task 73 initial
  acceptance SHA with their distinct scopes.  If a review correction exists,
  identify it as a new correction commit.  State Task 74 is documentation-only
  and does not amend prior commits; do not state or anticipate its own SHA.
- [ ] Run focused `CorrelationBreakdown|SimulationCorrelationUmbrella` CTest,
  diff checks, and commit only the acceptance document with:

  ~~~sh
  git commit -m "docs: trace phase 16 correlation breakdown acceptance"
  ~~~

### Final Phase 16 review

- [ ] Verify Task 72/73/74 ancestry and exact scopes, public leaf/umbrella
  consumption, source oracle, named typed schema, literal diagonal/off-
  diagonal values, empty/finite/name boundaries, ownership, regressions,
  retained fresh gates, documentation accuracy, and non-self-referential
  trace.
- [ ] Independently fresh-configure/build AppleClang headless, rerun focused
  and full CTest, and publish Spec/Task-Quality verdicts.  If a Critical,
  Important, or Minor issue exists, fix it in a new scoped commit and rerun
  final review; never amend accepted commits.
