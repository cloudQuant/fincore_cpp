# Phase 7 return transformations acceptance

This is bounded acceptance of the native return-transformation slice. It does
not claim pandas parity, local-timezone behavior, full-project completion,
release readiness, or a GUI runtime/visual result.

## Implementation trace

The exact three-SHA implementation trace is:

1. `e81a876e7d5f17f4e8a61a3bbb2907e0107d61d9` — initial native return
   transformations.
2. `c1f5151f0b7d4d35d6f396b78b053ab421903747` — compatibility-rule
   clarification.
3. `feaae2c7fd5e2747c300582d09e76d09de55ace2` — Task 44 compatibility
   stabilization.
4. `5ae7a85744fee2f97f5c898ff4c5af9d464c11e8` — Task 45 initial acceptance
   record.

This Task 46 commit is documentation-only and non-self-referential: it records
the direct Task 45 acceptance SHA without amending Task 45's initial acceptance
record.

## Acceptance checklist

- [x] Retain Task 44's genuine public-header RED; no accepted header was
  removed to manufacture another RED.
- [x] Retain the Task 44 Anaconda-base Python oracle evidence.
- [x] Build and run a public-only umbrella consumer through
  `<fincore/metrics.hpp>` and `<fincore/calendar.hpp>` only.
- [x] Pass a fresh AppleClang headless configure, build, scoped CTest, and
  complete CTest run.
- [x] Pass a separate fresh Homebrew LLVM 22 ASan+UBSan headless configure,
  build, and complete CTest run without sanitizer findings.
- [x] Pass a default local ImGui/ImPlot adapter registration and demo
  compile/link build without executing a GUI.
- [x] Observe the expected fresh invalid-ImGui configure diagnostic without
  fetching or fabricating a dependency.
- [x] Audit exactly the three owned tracked paths before staging.
- [x] Record compiler warnings and sanitizer diagnostics from completed gates.
- [x] Make no GUI execution or visual-validation claim.
- [x] Keep the deferred scope out of this acceptance.

## Prior RED and oracle evidence

Before Task 44 changed production code, `cmake --build build --parallel 4`
exited `1` while compiling `tests/unit/returns_test.cpp`, whose sole fincore
header was `<fincore/metrics/returns.hpp>`. The compiler showed the intended
missing public declarations: the numeric cumulative calls tried to bind a
`double` to the prior `std::string_view` overload, and `normalize`,
`return_aggregation_period`, and `aggregate_returns` were undeclared. This was
the genuine pre-production public-header RED.

The Task 44 local oracle ran with the mandated interpreter
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`. It confirmed
numeric-start cumulative values, normalization, and weekly/monthly/quarterly/
yearly labels. The relevant weekly oracle includes `(2020, 53)=.122`,
`(2021, 1)=-.1`, `(2021, 13)=.32`, `(2021, 52)=.01`, `(2021, 53)=.03`, and
`(2022, 52)=.02`; expected IEEE rounding was documented separately. The
post-review correction oracle also verified that both input orders for the
2018 `(calendar year, ISO week) == (2018, 1)` collision produce `.32` at the
same canonical bucket start.

## Public-only consumer

`MetricsUmbrella.ConsumesReturnTransformationsThroughPublicHeadersOnly` adds
a cross-year `utc_timestamp` fixture without including `returns.hpp` directly.
It calls all four public operations:

```cpp
fincore::cumulative_returns(returns, 100.0, "strategy");
fincore::cumulative_return(returns, 100.0, "strategy");
fincore::normalize(values, 1.0, "equity");
fincore::aggregate_returns(
    returns, fincore::return_aggregation_period::weekly, "strategy");
```

The test checks the public constants, cumulative output index and exact wealth
values, normalized values, and the exact weekly three-column schema. It checks
that `year` and `period` are genuine `int` columns and `strategy` is a genuine
`double` column. Its cross-year boundary output is deliberately three rows:
`(2020, 53)=.122`, `(2021, 1)=-.1`, `(2021, 53)=.03`, with canonical indices
December 28 2020, January 4 2021, and January 1 2021 respectively. Thus it
also preserves the documented nonmonotonic weekly index boundary.

## Fresh offline gates

All positive configurations used the existing local GoogleTest source at
`/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation/build/_deps/googletest-src`, passed it through
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST`, and set
`FETCHCONTENT_FULLY_DISCONNECTED=ON`. CMake was `4.0.2` and Ninja was `1.12.1`.
No gate fetched, cloned, vendored, fabricated, or substituted a dependency.

### AppleClang headless core

Fresh root: `/tmp/fincore-task45-apple.QrDiDn`.
`/usr/bin/c++ --version` reported Apple clang `17.0.0` and CMake identified
`AppleClang 17.0.0.17000603`.

```bash
cmake -S . -B /tmp/fincore-task45-apple.QrDiDn/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task45-apple.QrDiDn/build --parallel 4
ctest --test-dir /tmp/fincore-task45-apple.QrDiDn/build -R 'MetricsUmbrella.*' --output-on-failure
ctest --test-dir /tmp/fincore-task45-apple.QrDiDn/build --output-on-failure
```

All commands exited `0`: scoped `MetricsUmbrella` passed `6/6`, and the full
headless suite passed `212/212` with zero failures. The captured AppleClang
build emitted no compiler warning diagnostic.

### Homebrew LLVM 22 ASan+UBSan headless core

Fresh root: `/tmp/fincore-task45-llvm.lyvhq7`.
`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake identified `Clang 22.1.8` for C and C++.

```bash
cmake -S . -B /tmp/fincore-task45-llvm.lyvhq7/build -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task45-llvm.lyvhq7/build --parallel 4
ctest --test-dir /tmp/fincore-task45-llvm.lyvhq7/build --output-on-failure
```

All commands exited `0`; the full suite passed `212/212` with zero failures.
The build emitted one third-party GoogleTest `-Wcharacter-conversion` warning
at `gtest-printers.h:524` (`char8_t` to `char32_t`). No `AddressSanitizer`,
`UndefinedBehaviorSanitizer`, or `runtime error` diagnostic appeared in the
build or CTest output.

### Default local ImGui/ImPlot adapter and demo compile boundary

Fresh root: `/tmp/fincore-task45-implot.eL39fI`.

```bash
cmake -S . -B /tmp/fincore-task45-implot.eL39fI/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task45-implot.eL39fI/build --parallel 4
ctest --test-dir /tmp/fincore-task45-implot.eL39fI/build --output-on-failure --parallel 4
```

Configure found the default local ImGui and OpenGL; build linked both
`fincore_implot_demo` and `tests/fincore_tests`. CTest registered
`ImPlotAdapterCompile` as test `213`; the full run passed `213/213`, and the
separate registered compile-contract run passed `1/1`. The demo was compiled
and linked only; no GUI executable was launched. The build emitted three macOS
OpenGL deprecation warnings for `glViewport`, `glClearColor`, and `glClear`;
these are not sanitizer findings.

### Invalid ImGui override

Fresh root: `/tmp/fincore-task45-invalid-imgui.d9n93x`.

```bash
cmake -S . -B /tmp/fincore-task45-invalid-imgui.d9n93x/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task45-invalid-imgui.d9n93x/not-a-dear-imgui-checkout \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

The configure exited `1` as expected at `cmake/fincore_implot.cmake:47`:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task45-invalid-imgui.d9n93x/not-a-dear-imgui-checkout'
```

This is the expected negative dependency diagnostic, separate from the
successful default-path build. It stopped at validation; no dependency was
fetched, cloned, vendored, fabricated, or substituted.

## Owned-path audit and deferred scope

Before staging, `git diff --name-only` listed only `README.md` and
`tests/unit/metrics_umbrella_test.cpp`; `git ls-files --others
--exclude-standard` listed only this acceptance record. `git diff --check`
produced no output. The umbrella test has only the two permitted public
fincore includes. The required ignored Task 45 report is outside the commit.

Deferred work includes pandas `MultiIndex` representation, local-timezone
bucketing, additional return-transformation APIs, interactive GUI/visual
validation, broader analytics, data providers, packaging, release validation,
and full Python public-API parity.
