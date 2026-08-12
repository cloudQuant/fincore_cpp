# Phase 28 Bayesian posterior return-cone acceptance

## Accepted leaf baseline

The direct Task 121 implementation commit is
`7790aa169a843a4837c91936b8ce21fa6a4f8d22`
(`feat: add bayesian posterior return cone`). It changes exactly these five
paths:

- `CMakeLists.txt`
- `include/fincore/metrics/bayesian.hpp`
- `src/metrics/bayesian.cpp`
- `tests/CMakeLists.txt`
- `tests/unit/bayesian_cone_test.cpp`

Its independent read-only reviews reached **Spec PASS** and **Task-Quality
PASS**, with Critical/Important/Minor findings `0/0/0`. Task 122 does not
modify the accepted leaf header, implementation, library registration, or any
other metrics leaf.

## Public contract and source mapping

`<fincore/metrics.hpp>` gains exactly one production façade line:

```cpp
#include <fincore/metrics/bayesian.hpp>
```

It makes this existing public leaf available through the metrics façade:

```cpp
[[nodiscard]] dataframe compute_bayes_cone(
    const dataframe& predicted_returns,
    double starting_value = 1.0);
```

The source Python helper consumes an array oriented
`[posterior_path, forecast_step]`. The native DataFrame is intentionally its
transpose, `[forecast_step, posterior_path]`: raw rows are forecast steps and
every genuine `double` column is one posterior path. Each path compounds in
the supplied raw row order, then the native code applies linear percentiles
across current path wealths.

The return frame has exactly these seven ordered genuine-`double` columns:

```text
percentile_1, percentile_5, percentile_25, percentile_50,
percentile_75, percentile_95, percentile_99
```

It copies the exact raw index, including duplicates and nonmonotonic values;
it does not sort, deduplicate, align, or synthesize index labels. The result is
freshly materialized and value-owning, retaining no source frame or column
view. An index-empty input with one or more typed-empty double paths returns a
typed-empty frame with all seven columns. Pathless, mixed/non-double, or
index-length-malformed input is an `input_error`.

The finite policy intentionally narrows Python behavior: native code requires
a finite start, every return and multiplier, cumulative wealth, and percentile
result to be finite. It raises `fincore::input_error` rather than propagating
NaN or infinity. A zero or negative starting value and a return at or below
`-1.0` remain valid when the resulting arithmetic is finite.

## Source oracle and leaf evidence

Task 121 ran the prescribed source oracle from
`/Users/yunjinqi/Documents/new_projects/fincore` using only the mandated
Anaconda interpreter:

```sh
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '...'
```

Its raw output is retained at
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-28-bayesian-return-cone/python-oracle.log`.
For the recorded three-path, four-step fixture with `starting_value=2.0`, the
source median is `[2.0, 2.0188, 2.0188, 2.0389880000000002]` and the final
99th percentile is `2.03977984`. The native leaf test transposes that source
fixture into DataFrame `[time,path]` columns and covers all seven values.

The leaf-only public consumer was registered before the leaf header existed.
Its build exited `1` with the genuine missing
`fincore/metrics/bayesian.hpp` diagnostic, and CTest was deliberately not run
after that RED. After the leaf implementation, Task 121's final records show
build exit `0`, focused `BayesianCone` `6/6`, relevant
`BayesianCone|MetricsUmbrella|Returns|Dataframe` `88/88`, full CTest
`387/387`, and `git diff --check` exit `0`.

## Façade-only RED/GREEN

`tests/unit/metrics_bayesian_cone_umbrella_test.cpp` is a public consumer with
only one fincore include:

```cpp
#include <fincore/metrics.hpp>
```

It does not include the Bayesian leaf or DataFrame header separately, thereby
also proving that `metrics.hpp` provides the DataFrame declarations it uses.
Before the façade include was added, `cmake --build build --parallel 4` exited
`1`; the authoritative raw RED is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-28-bayesian-return-cone/task-122-logs/facade-red-build-retry.log`
with status file `facade-red-build-retry.status`. Its only consumer diagnostics
are missing `fincore::compute_bayes_cone` and the seven
`bayesian_cone_percentile_*_column` symbols through `<fincore/metrics.hpp>`.
CTest was intentionally not run after this failed build.

After adding that single façade include, the consumer static-asserts the exact
function pointer type `fincore::dataframe (*)(const fincore::dataframe&,
double)`, the one-argument default-call expression, and references every
planned public percentile constant. It makes both the real default call and a
function-pointer call. Its frame has raw index `{42, -7, 42}` and two genuine
double paths:

```text
path_0: { .10, -.20, .00 }
path_1: {-.10,  .20, .00 }
starting value: 2.0
```

At row zero, its directly calculated wealths are `{2.2, 1.8}`, so the public
`percentile_1`, `percentile_50`, and `percentile_99` columns contain `1.804`,
`2.0`, and `2.196`. At row one the wealths are `{1.76, 2.16}` and median is
`1.96`; the zero row-two returns preserve the row-one endpoint values. The
consumer asserts exact raw-index retention, shape second dimension `7`, seven
ordered `get_columns_info<double>()` records with each type `double`, and
therefore no hidden non-double output column.

The terminal configured GREEN records are retained under the same ignored
`task-122-logs/` root:

| Command | Raw log and status | Observed result |
| --- | --- | --- |
| `cmake --build build --parallel 4` | `current-green-build-retry.log`, `.status` | exit `0` |
| `ctest --test-dir build -R 'BayesianCone|MetricsBayesianConeUmbrella|MetricsUmbrella|Returns|Dataframe' --output-on-failure` | `current-relevant-ctest.log`, `.status` | `89/89` passed, exit `0` |
| `ctest --test-dir build --output-on-failure` | `current-full-ctest.log`, `.status` | `388/388` passed, exit `0` |

## Fresh compiler and configuration gates

Every fresh gate below used a separate, non-overlapping `mktemp` root and
retains raw `tee` output plus command-status files there. The configured-tree
records above are distinct from these fresh builds.

### AppleClang Debug, headless

Raw root: `/tmp/fincore-phase28-apple.T8FSJg`.

```sh
/usr/bin/c++ --version
cmake -S . -B /tmp/fincore-phase28-apple.T8FSJg/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase28-apple.T8FSJg/build --parallel 4
ctest --test-dir /tmp/fincore-phase28-apple.T8FSJg/build --output-on-failure
rg -n -i 'warning:' /tmp/fincore-phase28-apple.T8FSJg/{configure,build,full-ctest}.log
```

`compiler-version.log` records Apple clang `17.0.0 (clang-1700.6.3.2)` and
the configure log identifies `AppleClang 17.0.0.17000603`. Configure, build,
and full CTest statuses are all `0`; full CTest passed `388/388`. The warning
scan produced zero lines and `rg` exit `1`, so no warning was observed in the
captured configure/build/CTest logs.

### Homebrew LLVM 22 Debug, ASan and UBSan, headless

Raw root: `/tmp/fincore-phase28-llvm-sanitize.eS1Fi7`.

```sh
/opt/homebrew/opt/llvm/bin/clang++ --version
CC=/opt/homebrew/opt/llvm/bin/clang \
CXX=/opt/homebrew/opt/llvm/bin/clang++ \
cmake -S . -B /tmp/fincore-phase28-llvm-sanitize.eS1Fi7/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
cmake --build /tmp/fincore-phase28-llvm-sanitize.eS1Fi7/build --parallel 4
ctest --test-dir /tmp/fincore-phase28-llvm-sanitize.eS1Fi7/build -N
ctest --test-dir /tmp/fincore-phase28-llvm-sanitize.eS1Fi7/build --output-on-failure
rg -n -e 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error' \
  /tmp/fincore-phase28-llvm-sanitize.eS1Fi7/{configure,build,ctest-registration,full-ctest}.log
```

`compiler-version.log` records Homebrew clang `22.1.8`; configure identifies
`Clang 22.1.8`. Configure and build exited `0`. `ctest -N` registered exactly
`388` tests, and full CTest passed `388/388` with exit `0`.
`sanitizer-scan.log` is empty with `rg` exit `1`, so the captured logs contain
no AddressSanitizer, UndefinedBehaviorSanitizer, or runtime-error diagnostic.

The separate warning scan has exactly one match:
`build/_deps/googletest-src/googletest/include/gtest/gtest-printers.h:524:35`
reports `-Wcharacter-conversion` for `char8_t` to `char32_t`. It is a
third-party GoogleTest warning, not a fincore warning and not a sanitizer
diagnostic.

### Default local ImGui/ImPlot, compile/link only

Raw root: `/tmp/fincore-phase28-implot.vT29Qd`.

```sh
cmake -S . -B /tmp/fincore-phase28-implot.vT29Qd/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-phase28-implot.vT29Qd/build --parallel 4
test -f /tmp/fincore-phase28-implot.vT29Qd/build/fincore_implot_demo
test -x /tmp/fincore-phase28-implot.vT29Qd/build/fincore_implot_demo
ctest --test-dir /tmp/fincore-phase28-implot.vT29Qd/build -N
ctest --test-dir /tmp/fincore-phase28-implot.vT29Qd/build --output-on-failure
```

No ImGui source override was supplied. `implot-cache.log` records the default
local `FINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui`
and `FINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot`;
both ImPlot and demo cache options are `ON`. Configure/build exited `0`, the
file and executable checks both exited `0`, and `ctest -N` registered
`ImPlotAdapterCompile` as test `#389` among `389` total tests. Full CTest
passed `389/389` with exit `0`.

The warning scan records exactly three macOS OpenGL deprecation warnings in
`examples/implot_dashboard.cpp`: `glViewport` (line 51), `glClearColor` (line
52), and `glClear` (line 53). These are compile warnings only. The demo was
never executed, so this is no positive GUI runtime claim.

### Invalid ImGui negative configuration

Raw root: `/tmp/fincore-phase28-invalid-imgui.43EKbt`.

```sh
cmake -S . -B /tmp/fincore-phase28-invalid-imgui.43EKbt/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase28-invalid-imgui.43EKbt/not-real
rg -U -P 'FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files' \
  /tmp/fincore-phase28-invalid-imgui.43EKbt/configure.log
```

The configure exited `1`, as required, at `cmake/fincore_implot.cmake:47`:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received '/tmp/fincore-phase28-invalid-imgui.43EKbt/not-real'
```

`multiline-diagnostic-match.status` records the byte-accurate single-backslash
PCRE `FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files` and `rg` exit
`0`. This proves the multiline diagnostic without changing CMake.

## GoogleTest FetchContent provenance

The fresh LLVM cache and checkout establish the actual dependency state:

- `tests/CMakeLists.txt` declares remote
  `https://github.com/google/googletest.git` at pinned `GIT_TAG v1.15.2`.
- `fetchcontent-cache.log` records
  `FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF` and an empty
  `FETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH=`. There is no local source
  override in that fresh configuration.
- The generated
  `/tmp/fincore-phase28-llvm-sanitize.eS1Fi7/build/_deps/googletest-src`
  checkout reports `origin https://github.com/google/googletest.git` for fetch
  and push, exact HEAD `b514bdc898e2951020cbdca1304b75f5950d1f59`, and exact
  tag `v1.15.2`.

These are observed FetchContent checkout/cache facts, not a claim that
GoogleTest was already supplied or that no network fetch occurred.

## Independent review and exclusions

A fresh independent read-only document/gate review inspected this unstaged
five-path candidate and its raw evidence before staging. It returned **Spec
PASS** and **Task-Quality PASS**, with Critical/Important/Minor findings
`0/0/0`, and made no edits. This records the review verdict only; it does not
add or predict a Task 122 acceptance SHA.

This acceptance is limited to the deterministic return-cone leaf and its
metrics-façade export. It does not add PyMC, posterior fitting or sampling, a
`Bayesian` class, `compute_consistency_score`, random-state parity, reports,
providers, packaging/release work, or GUI runtime behavior. ImPlot evidence is
strictly compile/link, CTest registration, and build-root executable existence;
the GUI executable was not launched.
