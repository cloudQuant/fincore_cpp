# Phase 8 round-trip core acceptance

## Pre-verification acceptance checklist

The following checklist was first written as unchecked items before Task 48's
fresh verification gates; it is checked only after the corresponding evidence
recorded below was observed.

- [x] Task 47 public-header RED observation is recorded.
- [x] Task 47 Python oracle is recorded from the mandatory Anaconda environment.
- [x] Focused and full Task 47 test results are recorded.
- [x] The umbrella consumer includes only public `metrics.hpp` and `calendar.hpp`.
- [x] Source/path audit confirms the four owned tracked paths only.
- [x] Fresh AppleClang headless configure, build, and full CTest pass.
- [x] Fresh Homebrew LLVM 22 ASan+UBSan configure, build, and full CTest pass.
- [x] Default local ImGui/ImPlot/demo compile and link evidence is recorded.
- [x] Invalid ImGui override fails with the expected negative configure diagnostic.
- [x] Build warnings and sanitizer diagnostics are recorded.
- [x] No GUI executable is launched.
- [x] Deferred scope remains explicit and bounded.

## Consumed implementation

Task 48 consumes Task 47's accepted implementation commit
`4b0046675872f2cd463677647a70776254154921` and its accepted custom-field
correction `61caa9c209f050bdd4fac2256707297ffd08fa2e`.

## Task 47 predecessor evidence

Task 47 recorded the required public-header RED observation before production
files existed: `cmake --build build --parallel 4 && ctest --test-dir build -R
RoundTrips --output-on-failure` stopped at
`fatal error: 'fincore/metrics/round_trips.hpp' file not found`; no focused
test binary was generated.

Its prescribed interleaved long/short Python oracle was refreshed before the
production implementation with:

```bash
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '<imports fincore.metrics.round_trips; creates the prescribed UTC fixture; prints groupby_consecutive and extract_round_trips split dictionaries>'
```

The recorded oracle's grouped schema/order is `amount`, `symbol`, `price`,
with rows `(4,A,15)`, `(-4,A,20)`, `(-2,B,30)`, `(2,B,20)`; the extracted
schema/order is `pnl`, `open_dt`, `close_dt`, `long`, `rt_returns`, `symbol`,
`duration`, with two FIFO events at PnL `20`, returns `1/3`, and long then
short flags. Task 47's focused `RoundTrips` suite passed `7/7`; its focused
`RoundTrips|Transactions|Positions|Dataframe` suite passed `57/57`; and its
then-full headless suite passed `219/219`, with `git diff --check` silent.

## Public-only consumer

`MetricsUmbrella.ConsumesRoundTripsThroughPublicHeadersOnly` has exactly the
two public fincore includes `<fincore/calendar.hpp>` and
`<fincore/metrics.hpp>`; it does not include `round_trips.hpp` directly. Its
long/short fixture calls both public APIs. The A long legs have an exact
eight-hour gap and consolidate to `+4 @ 15`; the following close is `-4 @ 25`.
The test asserts both grouped blocks, ordinal extracted index `{0, 1}`, and
the first FIFO event exactly: PnL `40`, open `A@09:00`, close `A@19:00`, long
`true`, return `2/3`, symbol `A`, and duration `10h`. It retrieves and
verifies the concrete `std::string`, `bool`, and `timestamp` result columns.

## Fresh offline gates

Every positive configuration used the pre-existing local GoogleTest source
`$PWD/build/_deps/googletest-src` through
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` and set
`FETCHCONTENT_FULLY_DISCONNECTED=ON`; no gate fetched, cloned, vendored,
fabricated, or substituted a dependency. CMake was `4.0.2` and Ninja was
`1.12.1`.

### AppleClang headless

`/usr/bin/c++ --version` reported Apple clang `17.0.0`; CMake identified
`AppleClang 17.0.0.17000603`. In fresh root
`/tmp/fincore-task48-apple.SrpGQP`, the completed commands were:

```bash
cmake -S . -B /tmp/fincore-task48-apple.SrpGQP/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task48-apple.SrpGQP/build --parallel 4
ctest --test-dir /tmp/fincore-task48-apple.SrpGQP/build --output-on-failure
```

All three exited `0`; CTest passed `220/220`. The build emitted no compiler
warning diagnostic.

### Homebrew LLVM 22 ASan+UBSan headless

`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake identified `Clang 22.1.8` for C and C++. In fresh root
`/tmp/fincore-task48-llvm.TTYFef`, the completed commands were:

```bash
cmake -S . -B /tmp/fincore-task48-llvm.TTYFef/build -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task48-llvm.TTYFef/build --parallel 4
ctest --test-dir /tmp/fincore-task48-llvm.TTYFef/build --output-on-failure
```

All three exited `0`; CTest passed `220/220`. The build emitted one third-party
GoogleTest `-Wcharacter-conversion` warning at `gtest-printers.h:524` for
implicit `char8_t` to `char32_t` conversion. Neither build nor CTest emitted
`AddressSanitizer`, `UndefinedBehaviorSanitizer`, or `runtime error`
diagnostics.

### Default local ImGui/ImPlot compile and link boundary

In fresh root `/tmp/fincore-task48-implot.QFRz1a`, the completed commands were:

```bash
cmake -S . -B /tmp/fincore-task48-implot.QFRz1a/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task48-implot.QFRz1a/build --parallel 4
ctest --test-dir /tmp/fincore-task48-implot.QFRz1a/build --output-on-failure --parallel 4
```

All commands exited `0`; the build linked `fincore_implot_demo` and
`tests/fincore_tests`, and CTest passed `221/221` including
`ImPlotAdapterCompile`. The demo was compiled and linked only; no GUI
executable was launched. The only warnings were the three expected macOS
OpenGL deprecation diagnostics in `examples/implot_dashboard.cpp` for
`glViewport`, `glClearColor`, and `glClear`.

### Invalid ImGui override

In fresh root `/tmp/fincore-task48-invalid-imgui.ipw8Y7`, this negative command
exited `1` as expected:

```bash
cmake -S . -B /tmp/fincore-task48-invalid-imgui.ipw8Y7/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task48-invalid-imgui.ipw8Y7/not-a-dear-imgui-checkout \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

CMake stopped at `cmake/fincore_implot.cmake:47` with:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task48-invalid-imgui.ipw8Y7/not-a-dear-imgui-checkout'
```

This is the expected dependency-validation diagnostic and no dependency was
fetched, cloned, vendored, fabricated, or substituted.

## Scope audit and deferred work

Before staging, `git status --short` named exactly the owned four paths:
`include/fincore/metrics.hpp`, `tests/unit/metrics_umbrella_test.cpp`,
`README.md`, and this acceptance record. `git diff --name-only` listed the
three modified tracked paths, `git ls-files --others --exclude-standard`
listed only this new acceptance record, and `git diff --check` was silent.
The direct-header audit found only the two permitted public fincore includes in
the umbrella consumer; the sole `round_trips.hpp` reference is the public
umbrella export.

This acceptance does not cover portfolio-value-normalized return joins,
synthetic closing transactions, sector mappings, dynamic summary tables,
attribution, reports/tearsheets, interactive GUI or visual validation,
packaging/release validation, or full Python public-API parity. It is not a
full-project, GUI-runtime, or release-readiness claim.
