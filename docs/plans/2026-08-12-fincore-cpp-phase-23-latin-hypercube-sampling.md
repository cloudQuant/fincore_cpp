# fincore C++ Phase 23 Latin-Hypercube Sampling Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to
> implement this plan task-by-task.

**Goal:** Add a DataFrame-native Latin-hypercube sampler that exposes the
source matrix's sample-by-dimension shape with safe native seed semantics.

**Architecture:** Extend the existing paths leaf with one prefix and one
function. The implementation uses its local `std::mt19937_64`, existing
open-unit uniform helper, and a private rejection-mapped Fisher-Yates index;
it materializes named double DataFrame columns by value. It adds no generic
matrix type, global random utility, external RNG, model, selector, or facade
include.

**Tech Stack:** C++23, cloudQuant/DataFrame, CMake, GoogleTest, mandatory
Anaconda source oracle, Homebrew LLVM 22 ASan+UBSan, and optional ImPlot
compile/link verification.

**Baseline:** Phase 22 completes at
`f07ba0cfcf2ab56f7a341e5f56e6cfd27c81196b` with 342 headless CTest cases.
Keep it as an ancestor and never amend/squash it.

---

## Global constraints

- Touch only the paths leaf and LHS-specific tests. Do not change accepted
  GBM/gbm-from-returns behavior/tests, bootstrap, parameters, correlation,
  stress, generic helpers, CMake dependency policy, ImPlot source, other
  simulation leaves, bindings, reports, packaging, or facade includes.
- Public API is exactly:

  ```cpp
  inline constexpr std::string_view latin_hypercube_dimension_column_prefix{
      "lhs_dimension_"};

  dataframe latin_hypercube_sampling(
      std::size_t sample_count,
      std::size_t dimension_count,
      std::optional<std::uint64_t> seed = std::nullopt);
  ```

- Return ordinal sample index and exactly the double columns
  `lhs_dimension_0..N-1` in numeric order. Preserve source zero shapes
  `(0,d)`, `(n,0)`, and `(0,0)`.
- Each nonempty dimension must have exactly one point in each stratum
  `[k/n,(k+1)/n)` and all values in `[0,1)`. Never sort/normalize after
  generation just to meet this property.
- Reuse private `make_engine` and `open_unit_uniform` in `paths.cpp`; add only
  a private rejection-mapped bounded index plus local Fisher-Yates permutation.
  No global engine, `std::rand`, external RNG parameter,
  `std::uniform_*_distribution`, NumPy, or generic random helper. Native
  fixed seed repeats; NumPy stream equality is out of scope.
- Validate index/name/vector/cell representability before allocation/random
  work. Translate allocation/length failures to `input_error`; reject
  nonfinite/range failures. Counts are `size_t`, so Python negative dimensions
  are outside the native type interface. Never use invalid spans/fake backing
  arrays to test count guards.
- `simulation.hpp` already includes `paths.hpp`. Do not edit it. Capture true
  leaf/facade RED before declaration and prove automatic GREEN export later.
- Exclude antithetic variates, MonteCarlo, SimResult, terminal statistics,
  simulation VaR/CVaR, calibration, bootstrap changes, full NumPy RNG parity,
  providers, reports, packaging, and GUI runtime.
- Run Python only with
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.

### Task 99: Record design and plan

**Files:**

- Create: `docs/design/2026-08-12-phase-23-latin-hypercube-sampling-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-23-latin-hypercube-sampling.md`

Record exact API/schema/zero shapes, strata/RNG semantics, safe capacities,
facade rationale, oracle, exclusions, and TDD/acceptance workflow. Commit only
the two documents:

```sh
git add docs/design/2026-08-12-phase-23-latin-hypercube-sampling-design.md \
        docs/plans/2026-08-12-fincore-cpp-phase-23-latin-hypercube-sampling.md
git commit -m "docs: plan phase 23 latin hypercube sampling"
```

### Task 100: Add LHS leaf and public facade proof

**Files:**

- Modify: `include/fincore/simulation/paths.hpp`
- Modify: `src/simulation/paths.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/latin_hypercube_sampling_test.cpp`
- Create: `tests/unit/simulation_latin_hypercube_umbrella_test.cpp`

1. Create ignored SDD workspace, task brief, ledger and `task-100-report.md`.
   In `/Users/yunjinqi/Documents/new_projects/fincore`, record source output,
   shape/range/strata and zero/negative boundaries with the mandated Anaconda
   environment. Cover `(4,3)`, `(1,3)`, `(4,0)`, `(0,3)`, `(0,0)`, and Python
   negative-size failures.
2. Register leaf-only `<fincore/simulation/paths.hpp>` and facade-only
   `<fincore/simulation.hpp>` consumers before production declarations. Build
   must fail only because prefix/function are absent; do not run stale CTest.
3. Add exact declaration/constant and minimum implementation: output capacity
   and name checks, ordinal index, per-dimension permutation/jitter, value
   checks, zero-shape materialization, and allocation error translation. Do not
   change existing GBM behavior or `simulation.hpp`.
4. GREEN tests prove exact type/defaults; leaf/facade inclusion; all
   schema/index/type/order and zero shapes; range and one-per-stratum coverage;
   fixed-seed repetition/difference and finite unseeded output; valid-count
   capacity/name/cell guards; ownership; and no NumPy random-stream assertion.
   Tests may verify the published strata property but must not reimplement the
   sampler/RNG.
5. Run:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'LatinHypercubeSampling|SimulationLatinHypercubeUmbrella|GbmPaths|SimulationGbmPathsUmbrella|SimulationGbmFromReturnsUmbrella|BootstrapSummary|BootstrapConfidenceInterval|BootstrapDistribution|SimulationUmbrella|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

   A fresh independent reviewer must produce both Spec and Task-Quality
   verdicts and identify any C/I/M finding before commit. Fix every such
   finding in a new scoped commit and re-review.

   ```sh
   git add include/fincore/simulation/paths.hpp src/simulation/paths.cpp \
           tests/CMakeLists.txt tests/unit/latin_hypercube_sampling_test.cpp \
           tests/unit/simulation_latin_hypercube_umbrella_test.cpp
   git commit -m "feat: add latin hypercube sampling"
   ```

### Task 101: Accept Phase 23 evidence

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-23-latin-hypercube-sampling-acceptance.md`

1. Record Task 100 SHA/API/schema/zero shapes/strata/native RNG boundary,
   source oracle, RED/GREEN/errors/ownership/automatic facade/exclusions.
2. Retain raw `tee` logs for configured/full CTest; fresh Apple headless;
   clean registration-audited LLVM ASan+UBSan; ImPlot/demo compile/link-only
   with actual build-root executable checked but never launched; invalid-ImGui
   multiline diagnostic; and actual GoogleTest FetchContent provenance. If any
   LLVM run is corrupt/time-sliced, preserve it only as a non-acceptance
   diagnostic and use a fresh clean audited retry for success.
3. Independently review docs/logs/gates and commit exactly the two docs:

   ```sh
   git add README.md \
           docs/acceptance/2026-08-12-phase-23-latin-hypercube-sampling-acceptance.md
   git commit -m "docs: accept phase 23 latin hypercube sampling"
   ```

### Task 102: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-23-latin-hypercube-sampling-acceptance.md`

1. Record full Task 100 implementation SHA and direct Task 101 acceptance SHA
   plus exact scopes. State Task 102 is documentation-only and does not amend
   prior commits; do not mention or predict its own SHA.
2. Run focused `LatinHypercubeSampling|SimulationLatinHypercubeUmbrella` and
   diff checks, then commit only this document with:

   ```sh
   git commit -m "docs: trace phase 23 latin hypercube sampling"
   ```

### Final Phase 23 review

Audit Task 100/101/102 scope/ancestry; API/schema/zero-shape/strata/RNG/capacity
semantics; source oracle; no duplicate/global randomness; ownership/tests/facade
export; raw gates/provenance/no-GUI claims; wording and non-self-reference.
Fresh-configure Apple headless, rerun focused/full CTest and mandated Anaconda
oracle, then publish Spec/Task-Quality verdicts. Fix all Critical, Important,
or Minor findings in new scoped commits; do not amend accepted history.
