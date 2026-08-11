# Phase 16 named correlation-breakdown matrix acceptance

## Scope and consumed history

This bounded acceptance covers the deterministic, named DataFrame-native
adaptation of Python `fincore.simulation.scenarios.generate_correlation_breakdown`.
It accepts the simulation-umbrella export, literal diagonal/off-diagonal
materialization, strict names and finite target boundaries, typed-empty matrix,
isolated public consumer, and fresh build evidence. It is not a release,
correlation estimator, PSD policy, covariance conversion, path shock, random
simulation, provider, full Python API parity, or GUI-runtime acceptance.

Task 73 consumes the complete Task 72 leaf chain:

- initial leaf revision
  `1e94efb0aa9365db592fa8845dec9ed3677d541a`
  (`feat: add correlation breakdown matrix`);
- test-only correction
  `c1ffef2a5276d706d88eba02664f41f1e4b977f4`
  (`test: remove invalid correlation span probe`).

The correction removes a test that constructed a `std::span` whose claimed
range was not valid C++23 storage. It changes only
`tests/unit/correlation_breakdown_test.cpp`; the leaf source, public contract,
and remaining coverage are unchanged. A standards-valid runtime span with an
unrepresentable element count cannot be materialized in this process, so the
pre-allocation count/index/square guards remain production-audited rather than
being exercised through undefined test setup.

Task 72's ignored report retains the complete source oracle, genuine leaf
public-header RED, GREEN evidence, and the correction rationale. The source
command ran from the local Python checkout through exactly
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; its complete
observable matrix oracle was:

```text
target 1.0
[[1.0, 1.0, 1.0], [1.0, 1.0, 1.0], [1.0, 1.0, 1.0]]
target 0.25
[[1.0, 0.25, 0.25], [0.25, 1.0, 0.25], [0.25, 0.25, 1.0]]
target -0.5
[[1.0, -0.5, -0.5], [-0.5, 1.0, -0.5], [-0.5, -0.5, 1.0]]
empty []
```

Python takes an anonymous integer dimension and returns nested values. The
native API deliberately replaces that dimension with explicit asset names so
the same literal matrix can be represented safely as a named `DataFrame`.
That is a schema adaptation, not a claim of a Python object or pandas API.

## Accepted public contract

`<fincore/simulation.hpp>` exports:

```cpp
fincore::dataframe generate_correlation_breakdown(
    std::span<const std::string_view> asset_names,
    double target_correlation = 1.0);
```

For `N` validated names, the returned table has exactly ordinal index
`{0, ..., N - 1}`, exactly `N` genuine `double` columns in supplied name
order, and no extra columns. Cell `(row, column)` is `1.0` when the positions
are equal and the literal `target_correlation` otherwise. The returned index,
column labels, and values are fully owned. The names are copied and validated
before any DataFrame C-string boundary, so bounded non-NUL-terminated views
are valid and later mutation/destruction of source storage cannot affect the
result.

An empty asset span is valid and produces a true `0x0` table: empty ordinal
index and zero data columns. A nonempty name must be nonempty, contain no
embedded NUL, and be distinct as a complete `std::string_view`; invalid names
raise `fincore::input_error`. `target_correlation` must be finite; NaN and both
infinities raise `input_error`. The implementation guards index/count and
square-allocation representability before constructing the table.

There is intentionally no correlation validity or PSD policy. Finite targets
outside usual correlation bounds remain untouched on off-diagonal cells, while
the diagonal remains exactly `1.0`. No input time-series index, alignment,
sorting, deduplication, resampling, correlation/covariance estimation, repair,
factor/model state, random source, bootstrap/GBM/Monte Carlo process, stress
scenario behavior, provider/cache, report/release, or GUI-runtime behavior can
influence the result.

## Umbrella-only RED and configured GREEN

`tests/unit/simulation_correlation_umbrella_test.cpp` includes only
`<fincore/simulation.hpp>` among fincore headers. It has no direct correlation,
stress, metric, or source leaf include. Its small named two-asset consumer
checks the raw ordinal index, caller-order genuine-double schema, no extra
columns, and all four literal cells.

Before publication, the newly registered consumer was built with:

```sh
cmake --build build --parallel 4
```

It exited `1` solely because `fincore::generate_correlation_breakdown` was not
exported by the simulation umbrella. The retained build log is
`/tmp/fincore-phase16-task73-umbrella-red.lhMDHI/build.log`. CTest was not run
after that compilation failure, so no stale test executable serves as RED
evidence.

After adding exactly
`#include <fincore/simulation/correlation.hpp>` to the simulation umbrella,
the configured build succeeded. The isolated
`SimulationCorrelationUmbrella` CTest passed `1/1`; the configured relevant
selection `CorrelationBreakdown|SimulationCorrelationUmbrella|StressScenarios|SimulationUmbrella|Dataframe`
passed `32/32`; and the configured full headless suite passed `291/291`.
`git diff --check` produced no output. The post-GREEN include audit found no
prohibited direct leaf include in the umbrella-only consumer.

## Fresh retained gates

Every fresh command's terminal output was retained with `tee` in a unique
directory. Each positive gate completed configure, build, and CTest before its
post-CTest scan. No GUI executable was launched. CMake was `4.0.2`; every
fresh configuration completed directly, so no FetchContent/DNS/network retry
directory was needed.

### AppleClang headless

Fresh directory: `/tmp/fincore-phase16-apple.qQYFPJ`. `/usr/bin/c++ --version`
reported Apple clang `17.0.0 (clang-1700.6.3.2)` and CMake identified
AppleClang `17.0.0.17000603`.

```sh
cmake -S . -B /tmp/fincore-phase16-apple.qQYFPJ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase16-apple.qQYFPJ --parallel 4
ctest --test-dir /tmp/fincore-phase16-apple.qQYFPJ --output-on-failure
```

All commands exited `0`; CTest passed `291/291`. Only after CTest, the retained
`warning:` scan over configure, build, and CTest logs returned status `1`
(expected no match).

### Homebrew LLVM 22 ASan+UBSan headless

Fresh directory: `/tmp/fincore-phase16-llvm.nsrWZR`.
`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake identified Clang `22.1.8` for C and C++.

```sh
cmake -S . -B /tmp/fincore-phase16-llvm.nsrWZR -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase16-llvm.nsrWZR --parallel 4
ctest --test-dir /tmp/fincore-phase16-llvm.nsrWZR --output-on-failure
```

All commands exited `0`; CTest passed `291/291`. The post-CTest retained
sanitizer scan found no `AddressSanitizer`, `UndefinedBehaviorSanitizer`, or
`runtime error` diagnostic (status `1`, expected no match). The only retained
warning was a third-party GoogleTest `-Wcharacter-conversion` at
`gtest-printers.h:524` for implicit `char8_t` to `char32_t` conversion; it is
neither a fincore warning nor a sanitizer finding.

### ImPlot/demo compile and link boundary

Fresh directory: `/tmp/fincore-phase16-implot.bk469Y`. AppleClang configured
the local Dear ImGui and ImPlot defaults with
`FINCORE_BUILD_IMPLOT=ON` and `FINCORE_BUILD_IMPLOT_DEMO=ON`:

```sh
cmake -S . -B /tmp/fincore-phase16-implot.bk469Y -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-phase16-implot.bk469Y --parallel 4
ctest --test-dir /tmp/fincore-phase16-implot.bk469Y --output-on-failure
test -x /tmp/fincore-phase16-implot.bk469Y/fincore_implot_demo
```

All four commands exited `0`. The build linked `fincore_implot_demo`, the
actual target path is the build root
`/tmp/fincore-phase16-implot.bk469Y/fincore_implot_demo`, and CTest passed
`292/292`, including `ImPlotAdapterCompile`. The demo is not a CTest
registration and was never run. The retained warning scan records exactly the
three expected macOS OpenGL deprecation warnings in
`examples/implot_dashboard.cpp` for `glViewport`, `glClearColor`, and
`glClear`; they are compile warnings, not GUI-runtime evidence.

### Invalid ImGui override

Fresh directory: `/tmp/fincore-phase16-invalid-imgui.B8pucq`. The expected
negative configure command exited `1`:

```sh
cmake -S . -B /tmp/fincore-phase16-invalid-imgui.B8pucq -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/definitely/not/a/dear-imgui-checkout
```

The retained PCRE multiline, whitespace-tolerant search with `-U -P` and
`\s+` across every break matched:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received '/definitely/not/a/dear-imgui-checkout'
```

This verifies dependency validation only. It does not replace the positive
ImPlot compile/link evidence, launch a GUI executable, or claim visual/runtime
validation.

## Scope and retained report

The ignored Task 73 record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-16-correlation-breakdown/task-73-report.md`.
It records the exact RED/GREEN chronology, retained directories, compiler and
CTest summaries, warning/sanitizer outcomes, demo artifact location, and no-GUI
execution boundary. The tracked Task 73 scope is limited to the simulation
umbrella, this isolated consumer and its registration, README, and this
acceptance record. It does not alter the Task 72 leaf, stress-scenario source,
generic DataFrame helpers, third-party configuration, ImPlot source, provider
surface, or prior phase material.

Task 74 is reserved for documentation-only, non-self-referential traceability;
it does not amend the Task 72 leaf chain or this Task 73 acceptance scope.
