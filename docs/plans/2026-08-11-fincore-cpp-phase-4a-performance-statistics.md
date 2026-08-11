# fincore C++ Phase 4A Performance Statistics Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a DataFrame-native C++ facade for the deterministic Python
`perf_stats` summary, using the established scalar metrics as delegates.

**Architecture:** A new public `perf_stats.hpp` exposes one and two-input
overloads that return a stable keyed `std::map<std::string, double>`.  The
source validates requested DataFrame columns, resolves the requested frequency
once, and composes existing public metrics without copying any formula or
creating a string-indexed timestamp DataFrame.  Alpha/Beta use the existing
native aligned pair delegate with Python's daily default behavior.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, local Python
fincore oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, and Homebrew LLVM for ASan+UBSan verification.

---

## Contract shared by every task

- `fincore::dataframe` remains `hmdf::StdDataFrame<fincore::timestamp>`;
  timestamps remain signed UTC Unix nanoseconds.  Do not use it as a synthetic
  string-indexed summary table.
- `perf_stats` returns a `std::map<std::string, double>` with the exact 13
  no-factor keys in the design and exactly `Alpha`/`Beta` added for the
  benchmark overload.  Key lookup, not iteration order, is the contract.
- Both overloads validate requested named double columns before resolving the
  frequency.  Missing, empty-named, wrong-typed, and malformed inputs raise
  `input_error`; empty genuine doubles are valid and yield the full key set of
  scalar-delegate results.
- Resolve `annualization_factor(period)` once.  Do not add an optional custom
  annualization parameter: Python `perf_stats` does not expose one.  The
  requested factor applies only to the same Python period-sensitive metrics;
  `Alpha`/`Beta` use `alpha_beta(..., daily_annualization)`, and `Omega ratio`
  retains its default zero-threshold daily call.
- Compose public scalar functions only.  Do not create a new non-finite
  filter, duplicate metric mathematics, alter `analysis_context`,
  `rolling_engine`, DataFrame alignment, ImPlot behavior, or fetch/vendor any
  dependency.

### Task 29: Add the native performance-statistics facade

**Files:**

- Create: `include/fincore/metrics/perf_stats.hpp`
- Create: `src/metrics/perf_stats.cpp`
- Create: `tests/unit/perf_stats_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing public-header tests**

Under Anaconda, reproduce and retain the finite daily/weekly oracle values in
the design.  Add `PerfStats` tests that include only the new public header and
call both absent overloads.  Assert the 13/15 exact key sets and all finite
daily goldens, including Alpha/Beta.  Cover Python's weekly convention that
changes annual return/volatility/Sharpe/Calmar/Sortino but leaves Alpha/Beta
daily-based.

Add direct-delegation regressions for mismatched benchmark timestamps and a
NaN/infinity fixture.  Verify custom return/benchmark names, empty genuine
double input, missing/empty/wrong-typed/malformed columns, and an unknown
frequency.  Do not test implementation-private helpers or rely on map
iteration order.

**Step 2: Run it to verify failure**

Run:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R PerfStats --output-on-failure
```

Expected: public-only compilation fails because
`fincore/metrics/perf_stats.hpp` and the declarations do not exist.

**Step 3: Implement the smallest composing facade**

Declare precisely the two overloads from the design.  Add `perf_stats.cpp` to
the core target and implement a private common helper that validates columns,
resolves the requested annualization once, fills the exact named map from the
accepted delegates, and obtains Alpha/Beta in one `alpha_beta` call for the
benchmark overload.  Do not add an `analysis_context` method, a bootstrap API,
or a custom annualization extension.

**Step 4: Run focused verification**

Run:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R 'PerfStats|Returns|Ratios|Risk|Stats|AlphaBeta|Dataframe' --output-on-failure
git diff --check
```

Expected: focused tests pass and the diff check has no output.

**Step 5: Commit**

```sh
git add include/fincore/metrics/perf_stats.hpp src/metrics/perf_stats.cpp \
        tests/unit/perf_stats_test.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add performance statistics facade"
```

### Task 30: Publish and accept the Phase 4A surface

**Files:**

- Modify: `include/fincore/metrics.hpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-11-phase-4a-performance-statistics-acceptance.md`
- Modify: `README.md`

**Step 1: Add a public umbrella consumer regression**

Extend `MetricsUmbrella` through only `<fincore/metrics.hpp>` to call the
unary and benchmark `perf_stats` overloads and check the core key/optional
factor-key contract.  Task 29 provides the genuine public-header RED, so do
not remove accepted functionality merely to manufacture a second failure.
Create an unchecked tracked acceptance checklist for public API, oracle,
structural/non-finite delegation, fresh core/sanitizer verification, and the
optional ImPlot boundary.

**Step 2: Publish only accepted behavior**

Include the new header once in `metrics.hpp`.  Document the exact C++ map
shape, named input columns, 13/15 key contract, Python period convention,
common-index alpha/beta behavior, and deferred bootstrap/portfolio/report
work.  Record actual full commit SHAs, compilers, commands, test counts, and
warnings only after they occur.

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

Also reconfigure the optional default ImPlot path and the fresh invalid-ImGui
override.  Do not launch a GUI; compile/test contracts remain distinct from an
interactive visual smoke test.

**Step 4: Commit**

```sh
git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp \
        docs/acceptance/2026-08-11-phase-4a-performance-statistics-acceptance.md \
        README.md
git commit -m "docs: accept phase 4A performance statistics"
```

## Deferred after this plan

Python `perf_stats_bootstrap`, `calc_bootstrap`, and
`calc_distribution_stats`; portfolio positions and transactions; round trips;
performance attribution; multi-column/full-index rolling APIs; Bayesian and
advanced risk models; optimization; simulation; reports/tearsheets; data
providers; packaging/release; and interactive ImPlot visual smoke testing
remain separate work.  Completing this plan is not full fincore parity or
project completion.
