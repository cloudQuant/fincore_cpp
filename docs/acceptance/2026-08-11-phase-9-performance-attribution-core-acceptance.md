# Phase 9 performance attribution core acceptance

## Scope and traceability

This is a bounded acceptance of the deterministic C++23,
cloudQuant/DataFrame-native performance-attribution core. It accepts public
umbrella visibility, typed long-form loading schemas, deterministic arithmetic,
and build/test evidence. It is not a release, full Python-parity, provider, or
GUI-runtime acceptance.

- Task 50 implementation: `3e6b1623d39c3913462c236283acefdf00f5a8c7`
  (`feat: add performance attribution core`).
- Task 51 regression hardening:
  `566ce45c41c6e36a549cafb41ee90fd8462da2e8`
  (`test: strengthen performance attribution regressions`).
- Task 52 acceptance change exports only
  `<fincore/metrics/perf_attrib.hpp>` from the existing public
  `<fincore/metrics.hpp>` umbrella. It does not change the attribution
  implementation.

## Public API and schema mapping

| Public API | Native schema / result |
| --- | --- |
| `normalize_attribution_positions` | Timestamp index; preserves ordered genuine `double` asset columns, removes optional genuine `double` cash, and dollar-normalizes by per-row absolute asset gross exposure. |
| `compute_exposures` | Wide weight frame plus long-form loading frame yields timestamp-indexed ordered factor `double` columns. |
| `perf_attrib_core` / `perf_attrib` | `perf_attribution_result{risk_exposures, attribution}`. `risk_exposures` has factor columns; `attribution` has factor-contribution columns, then `total_returns`, `common_returns`, `specific_returns`, `tilt_returns`, `timing_returns`. |
| `create_perf_attrib_stats` | Seven-label `std::map<std::string, double>` and ordinal-indexed factor summary with `factor` (`std::string`), `Average Risk Factor Exposure`, `Annualized Return`, and `Cumulative Return` (`double`). |
| `cumulative_returns_less_costs` | One-frame delegates to accepted `cumulative_returns`; two-frame requires the exact same index sequence and subtracts selected cost returns first. |

The loading model is intentionally long-form: each loading row has the date as
the DataFrame index, `symbol` as a genuine `std::string` column, and all
selected factor loadings as genuine `double` columns. Positions are wide
timestamp-indexed `double` asset columns. Required `(timestamp, symbol)` keys
are unique; missing keys, invalid types, malformed lengths, invalid factor
names, or nonidentical input index sequences are `fincore::input_error`.
This is a native DataFrame contract, not a fabricated pandas `MultiIndex`.

## Public-only RED then GREEN

Before adding the umbrella include, `tests/unit/metrics_umbrella_test.cpp`
continued to include only `<fincore/calendar.hpp>` and
`<fincore/metrics.hpp>`. It constructs a two-date/two-asset/two-factor fixture
through `fincore::perf_attrib`, asserts market exposure `0.28`, common return
`0.0058`, and the no-cost cumulative result `0.0098`.

The required RED command was run in the existing build directory:

```bash
cmake --build build --parallel 4
ctest --test-dir build -R MetricsUmbrella --output-on-failure
```

The build failed as intended with:

```text
error: no member named 'perf_attrib' in namespace 'fincore'
error: no member named 'cumulative_returns_less_costs' in namespace 'fincore'
```

Because the compile failed before linking a new test executable, the following
CTest invocation could run the prior binary and reported its then-existing
`7/7` tests. It is not treated as RED success evidence; the compiler failure
is the public-consumer RED proof.

Adding the single public include
`#include <fincore/metrics/perf_attrib.hpp>` to `metrics.hpp` made the exact
fixture compile and pass. The focused GREEN command passed `8/8` umbrella
tests, and the existing configured build passed `231/231` CTest tests.

## Numeric oracle evidence

The source Python oracle was run with the mandated Anaconda command from
`/Users/yunjinqi/Documents/new_projects/fincore`, using the Task 50 three-date,
two-asset, two-factor long/short dollar-position fixture:

```bash
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '<fixture importing fincore.metrics.perf_attrib>'
```

It produced exposures `(market, value)` of
`(0.342857142857143, -0.028571428571429)`, `(0.325, 0.0)`, and
`(0.25, -0.3)`; common returns of `0.002571428571429`, `0.0065`, and
`-0.0145`; and cost-adjusted cumulative returns of `0.019`, `0.006772`, and
`0.033954844`. The seven summary values and ordered factor-summary values
match the tolerance-bounded C++ assertions in
`PerfAttrib.ProducesCoreAttributionStatisticsAndCostAdjustedReturns`.
Task 51 additionally covers NaN/zero-gross/non-finite arithmetic, typed-empty
and custom schemas, malformed/misaligned inputs, missing/duplicate loadings,
factor collisions, and unsorted-but-identical date sequences. This is numeric
oracle/test evidence only; it does not accept the Python warning-and-drop
alignment layer.

## Fresh build evidence

All roots below were created with `mktemp -d`; no preexisting build directory
was removed. Configure, build, and CTest commands exited `0` unless the
explicitly negative configure states otherwise. The explicitly shown sanitizer
`rg` no-match scan returned `1`, as its lack of matches requires.

### AppleClang headless core

Fresh root: `/tmp/fincore-phase9-apple.aZppsy`.

```bash
cmake -S . -B /tmp/fincore-phase9-apple.aZppsy -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase9-apple.aZppsy --parallel 4
ctest --test-dir /tmp/fincore-phase9-apple.aZppsy --output-on-failure
```

CMake identified AppleClang `17.0.0.17000603`; CTest passed `231/231`.

### Homebrew LLVM 22 ASan/UBSan headless core

The binding brief's aggregate spelling `-DFINCORE_ENABLE_SANITIZERS=ON` was
tried in independent `/tmp/fincore-phase9-llvm.wJmF4B`. CMake emitted the
exact warning `Manually-specified variables were not used by the project:
FINCORE_ENABLE_SANITIZERS`; it therefore is not sanitizer evidence. The project
defines the separate `FINCORE_ENABLE_ASAN` and `FINCORE_ENABLE_UBSAN` options,
so the actual sanitizer gate used this fresh root:
`/tmp/fincore-phase9-llvm-asan.CMMQNA`.

```bash
cmake -S . -B /tmp/fincore-phase9-llvm-asan.CMMQNA -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase9-llvm-asan.CMMQNA --parallel 4
ctest --test-dir /tmp/fincore-phase9-llvm-asan.CMMQNA --output-on-failure
rg -n 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error' /tmp/fincore-phase9-llvm-asan.CMMQNA/configure.log /tmp/fincore-phase9-llvm-asan.CMMQNA/build.log /tmp/fincore-phase9-llvm-asan.CMMQNA/ctest.log
```

`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CTest passed `231/231`. The sanitizer scan had no matches for
`AddressSanitizer`, `UndefinedBehaviorSanitizer`, or `runtime error`. The
evidence-based absence is limited to these logs. The build did emit the
third-party GoogleTest warning:

```text
warning: implicit conversion from 'char8_t' to 'char32_t' may change the meaning of the represented code unit [-Wcharacter-conversion]
```

### Default-local ImPlot/demo compile and link boundary

`FINCORE_BUILD_IMPLOT` and `FINCORE_BUILD_IMPLOT_DEMO` default to `OFF` in this
project. To exercise the requested default *local source paths* rather than a
headless default, the fresh root `/tmp/fincore-phase9-implot.DA613y` explicitly
enabled both options while intentionally omitting `FINCORE_IMGUI_SOURCE_DIR`
and `FINCORE_IMPLOT_SOURCE_DIR`:

```bash
cmake -S . -B /tmp/fincore-phase9-implot.DA613y -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-phase9-implot.DA613y --parallel 4
ctest --test-dir /tmp/fincore-phase9-implot.DA613y -N
ctest --test-dir /tmp/fincore-phase9-implot.DA613y --output-on-failure
```

The build compiled and linked `fincore_implot_demo`. `ctest -N` listed
`ImPlotAdapterCompile`; the full run passed `232/232`. No GUI executable was
launched. The build emitted exactly these macOS OpenGL deprecation warnings:

```text
warning: 'glViewport' is deprecated: first deprecated in macOS 10.14 - OpenGL API deprecated. (Define GL_SILENCE_DEPRECATION to silence these warnings) [-Wdeprecated-declarations]
warning: 'glClearColor' is deprecated: first deprecated in macOS 10.14 - OpenGL API deprecated. (Define GL_SILENCE_DEPRECATION to silence these warnings) [-Wdeprecated-declarations]
warning: 'glClear' is deprecated: first deprecated in macOS 10.14 - OpenGL API deprecated. (Define GL_SILENCE_DEPRECATION to silence these warnings) [-Wdeprecated-declarations]
```

These results are not a no-warning claim.

### Fresh invalid ImGui override

Fresh root: `/tmp/fincore-phase9-invalid-imgui.5RlNC8`.

```bash
cmake -S . -B /tmp/fincore-phase9-invalid-imgui.5RlNC8 -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase9-invalid-imgui.5RlNC8/not-a-dear-imgui-checkout
```

The configure exited `1`, as expected, at `cmake/fincore_implot.cmake:47`:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-phase9-invalid-imgui.5RlNC8/not-a-dear-imgui-checkout'
```

No dependency was fetched, cloned, vendored, fabricated, or substituted.

## Deferred scope

This acceptance does not add Python's warning-producing `align_and_warn`
behavior or its silent date/asset dropping; transaction-turnover diagnostics;
market-data joins or factor-provider adapters; Brinson/style attribution;
Bayesian/bootstrap simulation; reporting/tearsheets; packaging/release work;
or interactive GUI/visual smoke validation. The ImPlot result is only a
compile/link and CTest contract, and Phase 9 is not full fincore/Python parity.
