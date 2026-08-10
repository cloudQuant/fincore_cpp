# fincore C++ Phase 3B Rolling Paired Metrics Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add the Python-shaped, DataFrame-native rolling paired metrics that
produce one completed-window endpoint series, while retaining the accepted
native timestamp-alignment and scalar-metric semantics.

**Architecture:** Extend the existing public rolling facade with a private
paired endpoint helper. It aligns strategy and benchmark once on common
timestamps in left-input order, slices both aligned DataFrames per complete
window, and delegates to public alpha, beta, and capture functions. The phase
does not add any multi-column rolling result or modify `rolling_engine`.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, local Python
fincore oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, and Homebrew LLVM for ASan+UBSan verification.

---

## Contract shared by every task

- `fincore::dataframe` remains `hmdf::StdDataFrame<fincore::timestamp>`;
  timestamps remain signed UTC Unix nanoseconds. Do not add another DataFrame
  alias, local-time conversion, or timezone dependency.
- A paired public entry validates both named double columns by calling
  `align_on_common_index` once, before length/window calculation. Common
  timestamps preserve the left input's supplied order; do not sort,
  deduplicate, outer-align, or globally finite-filter.
- Each output is a one-column double series named by the requested returns
  column. It contains completed aligned-window endpoints only. Empty genuine
  double columns/no common timestamps and aligned sources shorter than the
  window produce an empty typed result. Missing, wrong-typed, empty-named, and
  index-length-mismatched columns throw `input_error`.
- `window == 0` throws `input_error` before any short/empty result. Every
  window delegates to an accepted public scalar metric; no alpha/beta/capture
  formula is copied into the rolling implementation.
- `roll_alpha` resolves its annualization exactly once. `roll_beta` ignores
  period and annualization, preserving public beta behavior. Capture functions
  resolve their period-derived annualization exactly once. NaN and infinity
  rules stay owned by each scalar delegate.
- Only public headers may appear in consumer tests. Do not modify
  `rolling_engine`, scalar alpha/beta/captures, DataFrame alignment, optional
  ImPlot behavior, or fetch/vendor/fabricate Dear ImGui.

### Task 25: Implement paired endpoint rolling metrics

**Files:**

- Modify: `include/fincore/metrics/rolling.hpp`
- Modify: `src/metrics/rolling.cpp`
- Modify: `tests/unit/rolling_metrics_test.cpp`

**Step 1: Generate finite Python goldens and write public-header RED tests**

Under Anaconda, generate and record the five oracle vectors in the design for
the six UTC daily observations and window `3`. Add public-header tests for all
five declarations, output index `{3, 4, 5, 6}`, requested-column preservation,
alpha custom risk-free/annualization, beta's intentionally ignored period and
annualization arguments, and capture period behavior.

Add regressions for a mismatched/unsorted left timestamp fixture: explicitly
align it with the public helper, assert its common index remains in left order,
then compare every rolling output against the same public scalar delegate over
each aligned window. Cover NaN by the same direct-delegation comparison rather
than inventing a new rolling filter. Cover empty genuine double columns,
no-common-index frames, short/exactly-one-window frames, zero window, custom
columns, and all missing/wrong-typed/short-column input errors on either side.

Register no new production source: the phase extends existing rolling sources.

**Step 2: Run it to verify failure**

Run:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R RollingMetrics --output-on-failure
```

Expected: public-only compilation fails because the five declarations do not
yet exist.

**Step 3: Implement the minimum paired facade**

Declare exactly the five functions in the design. Add private helpers to
validate a positive window, align once, make two identically indexed windows,
and emit a returns-named endpoint series. Resolve alpha/capture annualization
outside the per-window loop and delegate respectively to `alpha`, `beta`,
`up_capture`, `down_capture`, and `up_down_capture`. Do not add an alpha/beta
two-column DataFrame, `rolling_beta`, `rolling_regression`, or a new filter.

**Step 4: Run focused verification**

Run:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R 'RollingMetrics|AlphaBeta|Ratios|Dataframe' --output-on-failure
git diff --check
```

Expected: focused tests pass and the diff check has no output.

**Step 5: Commit**

```sh
git add include/fincore/metrics/rolling.hpp src/metrics/rolling.cpp \
        tests/unit/rolling_metrics_test.cpp
git commit -m "feat: add paired rolling metrics"
```

### Task 26: Publish and accept the Phase 3B surface

**Files:**

- Modify: `tests/unit/metrics_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-10-phase-3b-rolling-paired-metrics-acceptance.md`
- Modify: `README.md`

**Step 1: Add a public umbrella consumer regression**

Extend `MetricsUmbrella` through only `<fincore/metrics.hpp>` to call all five
new APIs. Task 25 already supplied the genuine public-header RED for their
then-absent declarations, while Phase 3A already added `metrics/rolling.hpp`
to the umbrella. Therefore record the prior RED evidence and add a consumer
GREEN without removing accepted functionality to manufacture another failure.
Start an unchecked tracked acceptance checklist covering the public
API/output contract, oracle, fresh core/sanitizer CTest, and retained ImPlot
dependency gate.

**Step 2: Publish and document only accepted behavior**

`metrics.hpp` already exposes `metrics/rolling.hpp`, so do not change the
umbrella include graph. Extend only its public consumer test and record actual
full commit SHAs, compiler versions, test counts, output/alignment semantics,
and the fact that beta ignores period/annualization. Explicitly retain deferred
multi-column and full-index rolling features plus the wider native facade.

**Step 3: Run final verification in fresh directories**

Run in separate `mktemp -d` directories:

```sh
cmake -S . -B <fresh-core> -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build <fresh-core> --parallel 4
ctest --test-dir <fresh-core> --output-on-failure

cmake -S . -B <fresh-sanitize> -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build <fresh-sanitize> --parallel 4
ctest --test-dir <fresh-sanitize> --output-on-failure
```

Also configure ImPlot with a fresh invalid `FINCORE_IMGUI_SOURCE_DIR` and
record the expected documented diagnostic. Do not make a positive ImPlot/demo
attempt without a real user-supplied Dear ImGui checkout.

**Step 4: Commit**

```sh
git add tests/unit/metrics_umbrella_test.cpp \
        docs/acceptance/2026-08-10-phase-3b-rolling-paired-metrics-acceptance.md \
        README.md
git commit -m "docs: accept phase 3B paired rolling metrics"
```

## Deferred after this plan

Multi-column `roll_alpha_beta` and `rolling_regression`, full-index
`rolling_beta` (including multi-factor inputs), portfolio positions and
transactions, round trips, performance attribution/stats, Bayesian/bootstrap
functions, optimization, simulation, reports/tearsheets, a full native facade,
packaging/release, and a positive ImPlot build/demo remain separate work.
Completing this plan is not full fincore parity or project completion.
