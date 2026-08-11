# Phase 17 simulation parameter-estimation acceptance

## Scope and consumed history

This bounded acceptance covers the deterministic, DataFrame-native counterpart
to Python fincore.simulation.base.estimate_parameters. It accepts one named
genuine-double return series, NaN-only removal, checked annualized sample
drift/volatility, the simulation-umbrella export, and the public consumer. It
does not accept a release, generic return-validation surface, random source,
GBM/path generation, bootstrap, Monte Carlo, antithetic or Latin-hypercube
sampling, SimResult, risk distribution APIs, optimization, provider, report,
full Python simulation parity, or GUI runtime.

Task 77 consumes the Task 76 leaf revision
6e7ad34e1ce1cd49ea46408aaebad0079283b013
(feat: add simulation parameter estimation). Its exact tracked scope is:

- CMakeLists.txt
- tests/CMakeLists.txt
- include/fincore/simulation/parameters.hpp
- src/simulation/parameters.cpp
- tests/unit/simulation_parameters_test.cpp

The leaf-only Task 76 report retains its initial public-header RED, source
oracle, GREEN tests, selector hardening, and independent review. This
acceptance changes only the simulation façade, a façade-only consumer and
registration, README, and this record. It does not amend the leaf, correlation
or stress behavior, DataFrame helpers, third-party configuration, or ImPlot
source.

## Source oracle and native representation

The local Python source was run through the required Anaconda environment:

~~~sh
cd /Users/yunjinqi/Documents/new_projects/fincore
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
import numpy as np
from fincore.simulation.base import estimate_parameters
values = np.array([0.01, np.nan, -0.02, 0.03, 0.0])
print(estimate_parameters(values, 252))
'
~~~

Its retained stdout is:

~~~text
(1.2599999999999998, 0.3304542328371662)
~~~

Python receives an anonymous array and returns an anonymous pair. The C++ API
instead makes the input schema explicit and returns fully owned scalar values:

~~~cpp
struct simulation_parameters {
    double drift;
    double volatility;
};

simulation_parameters estimate_simulation_parameters(
    const dataframe& returns,
    std::size_t periods_per_year = 252,
    std::string_view column = returns_column);
~~~

For retained observations r_1 through r_n, it computes mean = sum(r_i) / n,
sample_deviation = sqrt(sum((r_i - mean)^2) / (n - 1)), drift = mean *
periods_per_year, and volatility = sample_deviation *
sqrt(periods_per_year). The selected DataFrame column must be genuine double
and structurally valid; the raw timestamp index participates only in that
established boundary and never changes the positional sample order.

As in Python, the leaf drops only NaNs in source order. It intentionally has
stricter native safety boundaries: positive/negative infinity, fewer than two
retained observations, zero periods per year, malformed/missing/non-double
columns, embedded-NUL selectors, and nonfinite intermediate/final arithmetic
including overflow all raise fincore::input_error. It does not sort, align,
deduplicate, resample, clip, compound, or otherwise transform finite returns.

## Umbrella-only RED and configured GREEN

tests/unit/simulation_parameters_umbrella_test.cpp includes only
fincore/simulation.hpp among fincore headers. Its source fixture uses the
Python oracle values and an irregular/duplicate timestamp index, invokes the
simulation umbrella only, and checks both named scalar fields using a
1.0e-12 tolerance.

Before publishing the parameter leaf, that new consumer was registered while
include/fincore/simulation.hpp still contained only the correlation and stress
leaf exports. The clean retained RED is:

~~~sh
cmake --build build --parallel 4
~~~

at /tmp/fincore-phase17-task77-umbrella-red-retry.LTZcg0. It exited 1 solely
because fincore::estimate_simulation_parameters was absent from the umbrella:

~~~text
error: no member named 'estimate_simulation_parameters' in namespace 'fincore'
~~~

CTest was not run after that failed compilation, so no stale executable serves
as RED evidence. The earlier retained directory
/tmp/fincore-phase17-task77-umbrella-red.AwXYXv has the same compilation
failure in its build log, followed only by a wrapper attempt to assign zsh's
read-only status variable; the retry above records the clean exit status and
is the RED evidence used here.

After adding exactly the one façade include
fincore/simulation/parameters.hpp, configured GREEN at
/tmp/fincore-phase17-task77-configured-green.Y3FAjI was:

~~~text
cmake --build build --parallel 4                                         PASS
ctest -R SimulationParametersUmbrella                                    1/1 PASS
ctest -R SimulationParameters|SimulationParametersUmbrella|
        CorrelationBreakdown|StressScenarios|SimulationUmbrella|Dataframe 40/40 PASS
ctest --test-dir build --output-on-failure                               299/299 PASS
~~~

The source oracle was rerun in that retained configured-GREEN directory and
printed the same two Python values above. The post-edit diff check is recorded
with the Task 77 completion evidence.

## Fresh retained gates

Every fresh configuration used the existing local GoogleTest source through
FETCHCONTENT_SOURCE_DIR_GOOGLETEST, so it did not fetch, clone, fabricate, or
substitute any dependency. No GUI executable was launched.

### AppleClang Debug headless

The final fresh AppleClang directory is
/tmp/fincore-phase17-apple-retry.90pZrD. Its compiler-version and configure
logs identify Apple clang 17.0.0 and CMake AppleClang 17.0.0.17000603. The
configuration used Debug, tests enabled, ImPlot disabled, and the local
GoogleTest source override. Configure, build, and CTest all exit 0; CTest
passes 299/299. The warning scan over those three logs has expected zero-match
exit 1.

The first retained AppleClang directory,
/tmp/fincore-phase17-apple.vc7Khy, completed all 83 visible compilation/link
steps but its streamed terminal collection ended before the shell wrote the
status and CTest logs. It is retained without being used as the final gate;
the new retry directory above contains complete status records and the final
headless proof.

### Homebrew LLVM 22 ASan+UBSan headless

Fresh directory: /tmp/fincore-phase17-llvm.nWZ3la. The configuration used
/opt/homebrew/opt/llvm/bin/clang++, Debug, FINCORE_ENABLE_ASAN=ON,
FINCORE_ENABLE_UBSAN=ON, tests enabled, and ImPlot disabled. The compiler is
Homebrew Clang 22.1.8; configure, build, and CTest all exit 0, and CTest
passes 299/299. The post-CTest sanitizer scan for AddressSanitizer,
UndefinedBehaviorSanitizer, and runtime error has expected zero-match exit 1.
Its only warning is the third-party GoogleTest char8_t-to-char32_t
-Wcharacter-conversion warning at gtest-printers.h:524; it is not a fincore
warning or sanitizer diagnostic.

### ImPlot/demo compile and link boundary

Fresh directory: /tmp/fincore-phase17-implot.jGRF3O. AppleClang configured
local ImGui and ImPlot with FINCORE_BUILD_IMPLOT=ON and
FINCORE_BUILD_IMPLOT_DEMO=ON, built them, and ran CTest. Configure, build, and
CTest all exit 0; CTest passes 300/300, including ImPlotAdapterCompile. The
build logs fincore_implot_demo linking and the actual executable exists at the
build root:

~~~text
/tmp/fincore-phase17-implot.jGRF3O/fincore_implot_demo
~~~

That file was checked for existence only and never run. The warning log has
exactly the three expected macOS OpenGL deprecation warnings in
examples/implot_dashboard.cpp for glViewport, glClearColor, and glClear. They
are compile warnings, not GUI-runtime evidence.

### Invalid ImGui override

The initial negative configure at
/tmp/fincore-phase17-invalid-imgui.fFri8W exited 1 as intended, but its
first diagnostic pattern allowed line breaks only between some words. Its log
is retained. The fresh retry at
/tmp/fincore-phase17-invalid-imgui-retry.pR8BF8 also exited 1 and used a
multi-line PCRE in which every word boundary is whitespace tolerant:

~~~text
FINCORE_IMGUI_SOURCE_DIR\s+must\s+name\s+a\s+Dear\s+ImGui\s+checkout\s+with\s+all\s+required\s+files;\s+received
~~~

The retry diagnostic scan exits 0 and matches CMake's complete validation
error. This is dependency-validation evidence only; it neither substitutes a
dependency nor replaces the positive ImPlot compile/link proof.

## Next trace boundary

Task 78 is reserved for documentation-only, non-self-referential traceability.
It will record the initial Task 77 acceptance revision with its own distinct
scope; it does not amend the Task 76 leaf or this acceptance scope.
