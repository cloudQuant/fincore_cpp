# Phase 18 native GBM path-generation acceptance

## Scope and consumed history

This bounded acceptance covers the DataFrame-native counterpart to Python
`fincore.simulation.paths.geometric_brownian_motion`: validated GBM path
propagation, its named time-indexed table representation, a reproducible
native seed, simulation-umbrella export, and a public-only consumer. It is
not acceptance of a release, NumPy generator compatibility, `gbm_from_returns`,
bootstrap, Monte Carlo, antithetic/LHS methods, `SimResult`, calibration
changes, correlation/stress changes, optimization, providers, reports, full
Python simulation parity, or GUI runtime.

Task 81 consumes the Task 80 leaf revision
`5d9ceddb668568d5d5692cf6e3d03fd239e0177f` (`feat: add native gbm path
generation`). Its exact tracked scope is:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `include/fincore/simulation/paths.hpp`
- `src/simulation/paths.cpp`
- `tests/unit/gbm_paths_test.cpp`

The ignored Task 80 report retains the Anaconda source oracle, genuine
public-header RED, current-source GREEN, standards review, and the small
GREEN-only portability correction that adds a `size_t` bound alongside the
timestamp bound before converting a step count. It changes no public contract.

## Source oracle and native representation

The local Python source was run through the required Anaconda environment:

~~~sh
cd /Users/yunjinqi/Documents/new_projects/fincore
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
from fincore.simulation.paths import geometric_brownian_motion
for args in ((100.0, 0.1, 0.0, 1.0, 0.25, 2),
             (100.0, 0.0, 0.0, 0.1, 0.25, 1)):
    result = geometric_brownian_motion(*args, seed=42)
    print(args, result.shape, result.tolist())
'
~~~

The zero-volatility output, used as the cross-language numerical oracle, is:

~~~text
(100.0, 0.1, 0.0, 1.0, 0.25, 2) (2, 5)
[[100.0, 102.53151205244289, 105.12710963760242,
  107.78841508846317, 110.51709180756478],
 [100.0, 102.53151205244289, 105.12710963760242,
  107.78841508846317, 110.51709180756478]]
(100.0, 0.0, 0.0, 0.1, 0.25, 1) (1, 1) [[100.0]]
~~~

Python uses an anonymous `(path, time)` array. Native code deliberately
transposes that shape to a table whose ordinal timestamp index is time and
whose genuine `double` columns are paths:

~~~cpp
inline constexpr std::string_view gbm_path_column_prefix{"path_"};

dataframe geometric_brownian_motion(
    double initial_value,
    double annualized_drift,
    double annualized_volatility,
    double total_years,
    double time_step,
    std::size_t path_count,
    std::optional<std::uint64_t> seed = std::nullopt);
~~~

For `n_steps = trunc(total_years / time_step)`, the result has exact index
`{0, ..., n_steps}`, exactly `path_count` columns `path_0` through
`path_N` in numeric path order, no extra columns, and initial row
`initial_value`. Each later cell is:

~~~text
S_next = S_current * exp((mu - 0.5 * sigma^2) * dt + sigma * sqrt(dt) * Z)
~~~

This is a schema adaptation, not a pandas or NumPy object claim. The
zero-volatility case above is common to both implementations. A supplied seed
uses a local `std::mt19937_64` stream and a midpoint-open Box-Muller transform,
so repeated calls in the same supported native build reproduce the same table.
This deliberately does not assert that a C++ seed produces the same random
shocks as NumPy `default_rng`; nonzero-volatility tests prove only native
same-seed reproducibility and distinct-seed divergence. Omitted seed uses a
local `std::random_device` seed.

All five floating inputs must be finite; `time_step` must be positive,
`total_years` nonnegative, and `path_count` nonzero. A sub-step horizon yields
the Python-compatible initial-value-only table. The leaf rejects
unrepresentable index/name/output sizes and nonfinite model, exponent, or path
arithmetic with `fincore::input_error`. It intentionally does not add a
positive-price policy: finite zero and negative initial values propagate
algebraically. Returned index, labels, and values are independently owned.

## Umbrella-only RED and configured GREEN

`tests/unit/simulation_gbm_paths_umbrella_test.cpp` includes only
`<fincore/simulation.hpp>` among fincore headers. It calls the zero-volatility
oracle fixture and checks the raw ordinal index, exact two-column genuine
double schema (`path_0`, `path_1`), no extra columns, and each time value.

Before publication, it was registered while the umbrella did not export the
paths leaf. The retained command:

~~~sh
cmake --build build --parallel 4
~~~

at `/tmp/fincore-phase18-task81-umbrella-red.KaeoNO` exited 1 solely because
`fincore::geometric_brownian_motion` was absent from the umbrella consumer.
CTest was not run after that failed compilation, so no stale executable is
used as RED evidence. Adding exactly
`#include <fincore/simulation/paths.hpp>` to the umbrella made the consumer
compile; the configured umbrella test passed 1/1, the relevant selected set
passed 50/50, and full configured CTest passed 309/309. The final local
reverification on the committed-source candidate also passed build, relevant
50/50, and full 309/309 with a clean diff check.

## Fresh retained gates

Every positive gate completed configuration, build, and CTest before any
warning/sanitizer scan. No GUI executable was launched. Each used the local
DataFrame, Dear ImGui, ImPlot, and existing local GoogleTest source; no
dependency was fetched, cloned, fabricated, or substituted.

### AppleClang Debug headless

`/tmp/fincore-phase18-apple-final.uXrVlA` was configured with AppleClang
17.0.0.17000603, Debug, and `FINCORE_BUILD_IMPLOT=OFF`. Configure, build, and
CTest each exited 0; CTest passed 309/309. A post-CTest scan of the retained
configure/build/CTest logs found no `warning:` match.

### Homebrew LLVM 22 ASan+UBSan headless

`/tmp/fincore-phase18-llvm-clean.hvGY5k` was configured with
`/opt/homebrew/opt/llvm/bin/clang++` (Homebrew Clang 22.1.8), Debug,
`FINCORE_ENABLE_ASAN=ON`, `FINCORE_ENABLE_UBSAN=ON`, and ImPlot off. Configure,
build, and CTest each exited 0; CTest passed 309/309. The retained post-CTest
scan found no AddressSanitizer, UndefinedBehaviorSanitizer, or runtime-error
diagnostic. Its sole warning is a third-party GoogleTest
`gtest-printers.h:524` `-Wcharacter-conversion` from `char8_t` to `char32_t`;
it is not a fincore or sanitizer diagnostic.

### ImPlot/demo compile and link boundary

`/tmp/fincore-phase18-implot-clean.RczoHG` configured local ImGui and ImPlot
with `FINCORE_BUILD_IMPLOT=ON` and `FINCORE_BUILD_IMPLOT_DEMO=ON`. Configure,
build, and CTest each exited 0; CTest passed 310/310 including
`ImPlotAdapterCompile`. The build-root
`/tmp/fincore-phase18-implot-clean.RczoHG/fincore_implot_demo` exists and was
only checked for that executable artifact; it was never launched. The retained
build log has the three known macOS OpenGL deprecation warnings in
`examples/implot_dashboard.cpp` for `glViewport`, `glClearColor`, and
`glClear`. They are compile warnings, not GUI-runtime evidence.

### Invalid ImGui override

The fresh expected-negative configure at
`/tmp/fincore-phase18-invalid-imgui-clean.5H7MCW` used
`-DFINCORE_BUILD_IMPLOT=ON` with a nonexistent
`FINCORE_IMGUI_SOURCE_DIR`; it exited 1 at
`cmake/fincore_implot.cmake:47`. Its CMake-wrapped two-line message is
verified by a whitespace-tolerant multiline PCRE:

~~~text
FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files
~~~

The matching diagnostic is `FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui
checkout with all required files; received
'/definitely/not/a/dear-imgui-checkout'`. This is dependency-validation
evidence only; it does not replace the positive ImPlot compile/link proof.

## Scope and retained report

The ignored Task 81 record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-18-gbm-path-generation/task-81-report.md`.
The tracked Task 81 scope is limited to the simulation umbrella, this
public-only consumer and registration, README, and this record. It does not
alter the Task 80 leaf, Phase 17 calibration, stress/correlation behavior,
generic DataFrame helpers, third-party configuration, or ImPlot source.

Task 82 is reserved for documentation-only, non-self-referential traceability;
it does not amend the Task 80 leaf or this Task 81 initial acceptance scope.
