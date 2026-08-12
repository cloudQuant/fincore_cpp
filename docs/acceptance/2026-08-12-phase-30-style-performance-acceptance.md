# Phase 30 native performance-by-style acceptance

## Candidate status, implementation, and exact scope

This Task 130 record accepts Plan A: a pure native C++23 implementation over
the local cloudQuant DataFrame. It maps Python
`fincore.attribution.style.analyze_performance_by_style` without embedding a
Python runtime or adding a numerical dependency.

The final Task 129 implementation is
`b5bdd7b28060ebcc0f4a7b59c1402c4bd970ed98`
(`feat: add native performance by style`). Its exact five-path scope is:

- `include/fincore/attribution/style.hpp`
- `src/attribution/style.cpp`
- `tests/CMakeLists.txt`
- `tests/unit/style_performance_test.cpp`
- `tests/unit/attribution_style_performance_umbrella_test.cpp`

Independent specification review returned **PASS** and independent quality
review returned **APPROVE**, with Critical/Important/Minor `0/0/0`. Task 130
changes no code, test, CMake, design, or plan path. Its candidate tracked scope
is exactly `README.md` and this acceptance document. The candidate remains
unstaged and uncommitted until a fresh independent read-only Task 130 review
is dispatched and any Critical, Important, or Minor finding is corrected.

## Task 131 commit traceability

The complete Task 129 implementation is commit
`b5bdd7b28060ebcc0f4a7b59c1402c4bd970ed98`, whose direct parent is the
Phase 30 plan commit `1d69fc4d4d2d6e3ad1fe988a96d78c7f858afc62`. Its exact
five-path scope is:

- `include/fincore/attribution/style.hpp`
- `src/attribution/style.cpp`
- `tests/CMakeLists.txt`
- `tests/unit/style_performance_test.cpp`
- `tests/unit/attribution_style_performance_umbrella_test.cpp`

The initial Task 130 acceptance is commit
`e644893cdee3b78ad7c6913934e805809e6550a4`, whose direct parent is the Task
129 implementation commit `b5bdd7b28060ebcc0f4a7b59c1402c4bd970ed98`. Its exact
two-path scope is:

- `README.md`
- `docs/acceptance/2026-08-12-phase-30-style-performance-acceptance.md`

Task 131 is a documentation-only traceability update. It does not amend the
Task 129 implementation or the Task 130 acceptance, and it deliberately
neither states nor anticipates its own commit SHA.

## Public API and Python-to-native mapping

The accepted public leaf and existing `<fincore/attribution.hpp>` façade
expose exactly these two overloads, with no options object or unused Python
arguments:

```cpp
[[nodiscard]] dataframe analyze_performance_by_style(
    const dataframe& asset_returns);

[[nodiscard]] dataframe analyze_performance_by_style(
    const dataframe& asset_returns,
    const dataframe& style_exposures);
```

`asset_returns` is time-by-asset: raw rows are observations and its stable,
named, genuine-double columns are assets. Explicit `style_exposures` uses the
accepted Phase 29 representation: an ordinal index, exactly one genuine string
column named `asset`, and ordered genuine-double style columns. Exposure rows
align to return columns by exact asset identity, not row position. Missing
return assets receive `0.0`; extra exposure assets are ignored. This is the
native counterpart to pandas `reindex(returns.columns, fill_value=0.0)`.

For each style, aligned weights are summed in return-asset order. When the
finite total is positive, every aligned weight is divided by it; finite
negative hedge weights remain negative. A zero or negative total emits `0.0`
for every observation. Each positive-total row result is the ordered dot
product of finite returns and normalized weights. The one-argument overload
calls public `style_analysis(asset_returns)` once and delegates the returned
Phase 29 exposure frame to the explicit overload.

The result always owns fresh index, names, and values. Its index is ordinal
`0..observation_count-1`, deliberately ignoring duplicate or nonmonotonic raw
return labels. It contains exactly one genuine-double `{style}_return` column
per input style, in stable style-column order, with no identity or hidden
column.

## Empty, validation, and error boundary

Empty-shaped returns, an exposure frame with no rows, or an exposure surface
with no genuine-double styles returns a canonical empty `0x0` DataFrame. The
last case is represented natively by the Phase 29 `asset` column with no style
columns and corresponds to pandas' empty `N x 0` style table.

Nonempty computation strictly requires:

- genuine-double, finite, exact-index-length return columns with nonempty,
  NUL-free, distinct asset names;
- an ordinal exposure index; exactly one genuine string `asset` column; no
  other non-style typed columns; and exact-index-length columns;
- nonempty, NUL-free, distinct exposure asset identities and nonempty,
  NUL-free, collision-free style/output names;
- finite exposure weights, aligned totals, normalized weights, products, and
  accumulated output values; and
- representable ordinal indexes, counts, cells, names, and allocations.

Malformed schema, identity, name, finite-arithmetic, ordinal, capacity, or
allocation boundaries are translated to `fincore::input_error`. Output value
ownership is tested after input mutation and after local input destruction.

## Exact Python source oracle

Task 129 ran the plan oracle from
`/Users/yunjinqi/Documents/new_projects/fincore` using the required Anaconda
interpreter:

```sh
/Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base python -
```

The retained source record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-30-style-performance/source-oracle.md`.
Its five observations use return assets `ALPHA`, `BETA`, `GAMMA`, raw index
`[42,-7,42,100,3]`, and styles `blend={2,1,0}`,
`hedged={1,-.5,.5}`, `zero={0,0,0}`, and
`negative_total={-1,0,0}`. Python emits ordinal index `[0,1,2,3,4]`, columns
`blend_return, hedged_return, zero_return, negative_total_return`, and:

| row | blend_return | hedged_return | zero_return | negative_total_return |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 0.013333333333333334 | -0.005 | 0.0 | 0.0 |
| 1 | -0.01 | -0.01 | 0.0 | 0.0 |
| 2 | 0.016666666666666666 | 0.045 | 0.0 | 0.0 |
| 3 | 0.013333333333333334 | -0.03 | 0.0 | 0.0 |
| 4 | -0.006666666666666667 | 0.015 | 0.0 | 0.0 |

The same oracle proves that reordered exposure rows are identical, empty
returns and empty exposures are `(0,0)`, and the automatic result has ordinal
shape `(5,7)` with ordered columns `equal_weight_return, winner_return,
loser_return, value_return, growth_return, high_return, low_return`. Native
tests assert the complete explicit and automatic matrices rather than only
representative endpoints.

## Genuine RED/GREEN and Task 129 proof

Both public consumers were registered before the production declarations.
`tests/unit/style_performance_test.cpp` included only
`<fincore/attribution/style.hpp>` among fincore headers, while
`tests/unit/attribution_style_performance_umbrella_test.cpp` included only
`<fincore/attribution.hpp>`. `cmake --build build --parallel 4` exited `1` with
an independent `no member named 'analyze_performance_by_style' in namespace
'fincore'` error in each consumer. No CTest ran against that failed/stale
build. This is a genuine dual leaf/façade missing-overload RED, not an
after-the-fact narrative.

A follow-up focused RED then proved the ordinal exposure-index boundary:
`StylePerformance.RejectsMalformedExposureStructureAndIdentities` failed when
nonordinal `{0,2,1}` was accepted. Exact ordinal validation made it GREEN.
Retained records are `red.md` and `verification.md` beside the oracle above.
Final Task 129 GREEN was focused `10/10`, related `49/49`, and full CTest
`407/407`; `git diff --check` exited `0`.

## Task 130 current and fresh gates

Raw tee logs and explicit status files are retained below
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-30-style-performance/task130/logs/`.
The `/tmp` roots name the environment-local build trees used on 2026-08-12.

### Current configured build

| Command | Result |
| --- | --- |
| `cmake --build build --parallel 4` | exit `0`; no work required |
| `ctest --test-dir build -R 'StylePerformance|AttributionStylePerformanceUmbrella' --output-on-failure` | `10/10`, exit `0` |
| `ctest --test-dir build -R 'StyleAnalysis|StylePerformance|Attribution|Dataframe' --output-on-failure` | `49/49`, exit `0` |
| `ctest --test-dir build --output-on-failure` | `407/407`, exit `0` |
| `git diff --check` | exit `0` |

### Fresh AppleClang 17 Debug headless

Raw root: `/tmp/fincore-task130-apple.dLLcyZ`.

```sh
cmake -S . -B /tmp/fincore-task130-apple.dLLcyZ/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=OFF -DFINCORE_ENABLE_UBSAN=OFF
cmake --build /tmp/fincore-task130-apple.dLLcyZ/build --parallel 4
ctest --test-dir /tmp/fincore-task130-apple.dLLcyZ/build -N
ctest --test-dir /tmp/fincore-task130-apple.dLLcyZ/build \
  -R 'StylePerformance|AttributionStylePerformanceUmbrella' --output-on-failure
ctest --test-dir /tmp/fincore-task130-apple.dLLcyZ/build --output-on-failure
```

CMake was `4.0.2`, Ninja `1.12.1`, and `/usr/bin/c++` reported Apple clang
`17.0.0 (clang-1700.6.3.2)`; CMake identified
`AppleClang 17.0.0.17000603`. Configure/build exited `0`, `ctest -N`
registered exactly `407`, focused passed `10/10`, and full passed `407/407`.
The configure and build warning scan found `0` matches.

### Fresh Homebrew LLVM 22 Debug with ASan and UBSan

Raw root: `/tmp/fincore-task130-llvm.dQTHpe`.

```sh
cmake -S . -B /tmp/fincore-task130-llvm.dQTHpe/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
cmake --build /tmp/fincore-task130-llvm.dQTHpe/build --parallel 4
ctest --test-dir /tmp/fincore-task130-llvm.dQTHpe/build -N
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer \
ctest --test-dir /tmp/fincore-task130-llvm.dQTHpe/build \
  --output-on-failure --parallel 4
```

Homebrew Clang and `llvm-symbolizer` were `22.1.8`. Configure/build exited
`0`; `ctest -N` registered exactly `407`; the complete runtime passed
`407/407`. Scanning the raw CTest log for AddressSanitizer, UBSan,
LeakSanitizer, sanitizer summaries, and `runtime error` found `0` diagnostics.
The exact `style.cpp` compile and final `fincore_tests` link commands contain
both `-fsanitize=address` and `-fsanitize=undefined`; `otool -L` shows
`libclang_rt.asan_osx_dynamic.dylib` from LLVM 22. The only build warning is
upstream GoogleTest's `gtest-printers.h:524` `char8_t`-to-`char32_t`
`-Wcharacter-conversion` warning; it is neither fincore code nor a sanitizer
finding.

### Fresh local ImGui/ImPlot compile/link-only gate

Raw root: `/tmp/fincore-task130-implot.k22BYH`.

The AppleClang 17 Debug/Ninja configure used testing ON, ImPlot ON, demo ON,
local `/Users/yunjinqi/Documents/new_projects/imgui`, local
`/Users/yunjinqi/Documents/new_projects/implot`, and the same local DataFrame.
Configure/build exited `0`; `ctest -N` registered exactly `408` tests,
including `#408 ImPlotAdapterCompile`; full CTest passed `408/408`.

The only build warnings are exactly three macOS OpenGL deprecations in
`examples/implot_dashboard.cpp`: `glViewport`, `glClearColor`, and `glClear`.
The build-root `fincore_implot_demo` is a non-symlink regular file, executable,
and a Mach-O 64-bit arm64 executable. It does not occur in the CTest
registration, this gate never invoked it, and no GUI was launched.

### Fresh invalid-ImGui negative configure

Raw root: `/tmp/fincore-task130-invalid-imgui.AUDXkG`.

With ImPlot ON, testing OFF, and explicit nonexistent
`FINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task130-invalid-imgui.AUDXkG/definitely-does-not-exist`,
configure exited `1` as required. The input path was confirmed absent. A true
multiline, whitespace-tolerant PCRE used `rg -U -P` and single-backslash
`\s+` semantics to match the complete diagnostic, including its exact path:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task130-invalid-imgui.AUDXkG/definitely-does-not-exist'
```

## Dependency provenance

DataFrame was not fetched: every fresh cache records both
`FINCORE_DATAFRAME_SOURCE_DIR` and `DataFrame_SOURCE_DIR` as
`/Users/yunjinqi/Documents/new_projects/DataFrame`. That local cloudQuant
checkout was HEAD `90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230` with origin
`https://github.com/cloudQuant/DataFrame.git`.

Tests declare GoogleTest with CMake FetchContent from the official
`https://github.com/google/googletest.git`, shallow tag `v1.15.2`. The actual
successful LLVM `_deps/googletest-src` remote is that URL, HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`, exactly tagged `v1.15.2`.
It is therefore inaccurate to call GoogleTest a local dependency. The positive
graphics gate explicitly used local ImGui HEAD
`46d39d56febc2a00bdd2270dc88c8a13f2a0441a` and local ImPlot HEAD
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`.

## Acceptance boundary and exclusions

The evidence supports only the deterministic, value-owning performance by
static style-exposure surface and its automatic composition through public
`style_analysis`. It does not add or accept style tilts, regression
attribution, factor fetching or a provider, Python runtime embedding, SciPy or
another solver, plots, reports, packaging/release work, or GUI runtime
behavior. ImPlot evidence is compile/link and registered adapter-test evidence
only; the demo was never executed.
