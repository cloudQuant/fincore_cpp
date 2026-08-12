# Phase 20 bootstrap-distribution acceptance

## Accepted public boundary and consumed implementation

This acceptance exports Task 88's public leaf through exactly one additional
facade include: `<fincore/simulation.hpp>` now includes
`<fincore/simulation/bootstrap.hpp>`. The leaf implementation is exactly
`4b06aabbb15c0d7deb0c487b764fad0ede3f7a0e` (`feat: add bootstrap
distribution`). The Task 89 consumer includes only `<fincore/simulation.hpp>`
among fincore headers, invokes `bootstrap_distribution` with the named
`bootstrap_statistic::mean`, and checks the exact one-column genuine-`double`
schema, the ordinal index `{0, 1, 2, 3}`, the `bootstrap` label, and the four
deterministic all-equal values `0.01`.

The accepted API is:

~~~cpp
enum class bootstrap_statistic {
    mean,
    sample_standard_deviation,
    median,
    sharpe_ratio,
    sortino_ratio,
    minimum,
    maximum,
};

inline constexpr std::string_view bootstrap_statistic_column{"bootstrap"};

dataframe bootstrap_distribution(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);
~~~

It consumes exactly one index-length-matched genuine-`double` source column,
drops NaNs in source value order, samples the retained values with replacement,
and materializes an independently owned table with ordinal index
`0..resample_count-1` and exactly one genuine-`double` `bootstrap` column. A
zero resample count is a valid typed-empty output. The source index is
observational only: it is not sorted, aligned, deduplicated, or resampled.

Task 88 ran the source oracle through the mandated Anaconda environment. Its
all-equal fixture confirms deterministic mean/median/min/max values, zero
sample standard deviation, and positive-infinite ratio cases; its singleton
fixture confirms the specified `NaN` sample-standard-deviation, Sharpe, and
Sortino cases; its zero-count fixture confirms the empty shape. The C++ seed
uses a fresh local rejection-mapped `std::mt19937_64` stream. It guarantees
repeatability within a supported native build, not equality to Python/NumPy's
generator stream for the same seed.

All selectors are nonempty and NUL-free before a DataFrame C-string boundary.
Missing, wrong-typed, short, malformed, empty/all-NaN, infinite,
unrepresentable, or allocation-failing input is `fincore::input_error`.
Finite degenerate samples intentionally preserve the design's IEEE `NaN` and
positive-infinity statistic values; finite statistic arithmetic overflow and
unknown enum values are errors rather than partial output.

## Umbrella RED and configured GREEN

Before the facade include was added, the new registered consumer was configured
in `/tmp/fincore-phase20-task89-umbrella-red.PA6X3c/build` and built with:

~~~sh
cmake -S . -B /tmp/fincore-phase20-task89-umbrella-red.PA6X3c/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase20-task89-umbrella-red.PA6X3c/build --parallel 4
~~~

Configuration exited `0`; the build exited `1` solely in
`simulation_bootstrap_umbrella_test.cpp`, because `fincore::bootstrap_statistic`
and `fincore::bootstrap_distribution` were absent from the umbrella. CTest was
not run after this failed build, so no stale executable is used as RED evidence.
Adding only `#include <fincore/simulation/bootstrap.hpp>` made the same
consumer available through the facade.

On the configured build, `cmake --build build --parallel 4` exited `0`; the
focused CTest command exited `0` with `64/64` tests and full
`ctest --test-dir build --output-on-failure` exited `0` with `324/324` tests.

## Fresh retained gates and dependency provenance

Raw `tee` logs, status files, full commands, and scans are retained in the
ignored Task 89 report and its adjacent `task-89-logs` directory. Every fresh
positive configuration used the local DataFrame source and no
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` override. The fresh CMake caches record
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH=` and
`FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF`; each generated
`_deps/googletest-src` remote is `https://github.com/google/googletest.git`
at `b514bdc898e2951020cbdca1304b75f5950d1f59`. Therefore GoogleTest is the
project's pinned FetchContent checkout (`v1.15.2` as declared by
`tests/CMakeLists.txt`), not a claimed local-source override.

| Gate | Fresh result |
| --- | --- |
| AppleClang headless | `/tmp/fincore-phase20-task89-apple.6f5wnI/build`: AppleClang 17 Debug configuration, build, and CTest all exited `0`; CTest passed `324/324`. |
| Homebrew LLVM ASan+UBSan headless | `/tmp/fincore-phase20-task89-llvm.qlNVp0/build`: Homebrew clang 22.1.8 Debug configuration with both sanitizers, build, and CTest all exited `0`; CTest passed `324/324`. The sanitizer scan had its expected no-match exit `1`. Its sole warning is third-party GoogleTest `gtest-printers.h:524` `-Wcharacter-conversion`, not an ASan/UBSan finding. |
| ImPlot/demo compile and link only | `/tmp/fincore-phase20-task89-implot.xlOGyC/build`: Debug configure with `FINCORE_BUILD_IMPLOT=ON` and `FINCORE_BUILD_IMPLOT_DEMO=ON`, build, and CTest all exited `0`; CTest passed `325/325`, including `ImPlotAdapterCompile`. The actual executable `/tmp/fincore-phase20-task89-implot.xlOGyC/build/fincore_implot_demo` exists and was checked without launching it. The build has the known three macOS OpenGL deprecation warnings for `glViewport`, `glClearColor`, and `glClear`. |
| Invalid ImGui configuration | `/tmp/fincore-phase20-task89-invalid-imgui.PHbJ5n/build`: configure with `FINCORE_BUILD_IMPLOT=ON` and a nonexistent `FINCORE_IMGUI_SOURCE_DIR` exited `1` as required. A whitespace-tolerant multiline PCRE `FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\\s+files` exited `0` against the physically wrapped diagnostic. |

The invalid-ImGui configuration is dependency-validation evidence only; it
does not replace the positive ImPlot compile/link gate. Neither it nor the
ImPlot gate launches a GUI process.

## Scope and exclusions

Task 89 changes only the simulation facade, one public-only consumer and its
registration, this acceptance record, and README. It does not alter the Task
88 leaf, generic CMake/dependency files, DataFrame behavior, simulation
parameter/GBM/correlation/stress behavior, or ImPlot sources.

This is not acceptance of bootstrap confidence intervals, percentile or
summary schemas, arbitrary callable statistics, Python random-stream parity,
`MonteCarlo`, antithetic or Latin-hypercube sampling, providers, reports,
tearsheets, packaging/release work, full Python simulation parity, or GUI
runtime behavior.

## Retained report

The complete ignored evidence record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-20-bootstrap-distribution/task-89-report.md`.
It retains the RED/GREEN logs, all fresh command outputs and exits, cache and
FetchContent provenance, diagnostics, warning classification, and independent
review verdict.
