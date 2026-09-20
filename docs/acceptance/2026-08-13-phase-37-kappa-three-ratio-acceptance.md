# Phase 37 native Kappa-three ratio acceptance

## Candidate, route, and exact scope

This record accepts the Phase 37 native C++23/DataFrame leaf
`kappa_three_ratio` on the isolated implementation branch
`codex/fincore-cpp-foundation-impl`. It is a narrow metric-slice acceptance,
not a full Python/C++ parity claim and not authorization to merge, rebase,
push, or otherwise reconcile the divergent primary checkout.

The reviewed design and executable plan are commit
`2bf256dd5630ce7919340ab69a292da6d33e1542` (`docs: plan phase 37 kappa three
ratio`), the direct child of accepted Phase 36 trace
`f460860f5a40711b3adf1de23e36c8c8f738a046`. The route is the next smallest
valuable, non-alias, dependency-closed public calculation: a one-column,
third-order lower-partial-moment ratio. At the design's historical Python pin,
it was Empyrical-reachable and used by the extended report; the current Python
source retains the direct metric leaf and operation registration but no longer
ships those legacy facade/report modules. It is not a claim that it is the
smallest missing Python name or utility.

The smaller apparent candidates remain out of scope: market-correlation and
other forwarding names are aliases of existing canonical calculations, while
`simulation.base.annualize` is a utility rather than the selected public
metric/report route. `adjusted_sharpe_ratio`, `mar_ratio`, and
`conditional_sharpe_ratio` require larger independent numerical contracts.

The complete Task 157 implementation/fix chain is:

- `9c4680b96b714b0ca4a5999ff25391bc1376a99f` (`feat: add native kappa three
  ratio`), direct child of Task 156; and
- `2ba633d86074c11815a1243f77cf7522f1231d62` (`test: harden kappa selector
  precedence coverage`), direct child of the implementation commit.

Across the Task 156 base, that chain changes exactly four paths, 285 insertions
and no deletions:

- `include/fincore/metrics/ratios.hpp`
- `src/metrics/ratios.cpp`
- `tests/unit/ratios_test.cpp`
- `tests/unit/metrics_umbrella_test.cpp`

No CMake target, dependency, facade include, helper, result type, Python
runtime, source target, provider, report source, plot source, package, or GUI
runtime surface is added.

The first independent post-implementation review correctly rejected the
initial candidate as incomplete because it covered only one bounded selector,
one embedded-NUL precedence context, and only DataFrame—not selector—lifetime.
The second commit adds two legal bounded non-NUL selectors, NUL precedence for
both valid-prefix and malformed frames, malformed-before-NaN-MAR validation,
regular-versus-duplicate/nonmonotonic index equality, and simultaneous frame
and selector lifetime coverage. Final independent contract, Python-oracle, and
C++ quality reviews of the two-commit implementation chain found no remaining
actionable code or test finding. This is why the implementation is accepted as
a two-commit chain rather than by amending the original commit.

## Python mapping and exact native API

At the design's historical Python pin, `kappa_three_ratio` was exported from
`fincore.metrics.ratios`; its `DUAL_RETURNS_REGISTRY` entry was metadata, while
an explicit `_dual_method` in `fincore.empyrical` supplied Empyrical
class/instance reachability. The historical report engine then called it for
the `"Kappa Three Ratio"` statistic. Those are route-selection facts, not
current-source claims.

At current Python HEAD `2354bb560788f372502151a8b1b53accf536a6c0`, the direct
metric remains exported from `fincore.metrics.ratios` and registered in
`fincore.metrics.operations`; the legacy `empyrical.py`, `_registry.py`, and
`report/compute.py` modules are absent. The current root package excludes
legacy facades. No current Empyrical or report reachability is claimed here.
The metric is not a flat top-level `fincore` export.

Python exposes five parameters, but `risk_free`, `period`, and `annualization`
are dead on the successful numerical path. The native public leaf deliberately
exposes only the meaningful inputs:

```cpp
double kappa_three_ratio(
    const dataframe& returns,
    double mar = 0.0,
    std::string_view column = returns_column);
```

The exact three-parameter function pointer and one-argument default call are
compiled in both the ratios and metrics-umbrella consumers. A future Python
adapter may preserve the wider Python call shape, discard the three dead
parameters, and call this leaf; Phase 37 adds no such adapter, overload,
alias, ndarray/Series API, automatic-input API, or parameter object.

`<fincore/metrics.hpp>` already includes the ratios leaf, so no facade include
change was needed. The result is an owning scalar and retains no source frame,
index, value span, or selector storage after return.

## Calculation, validation, and compatibility contract

The native definition preserves this order exactly:

1. Reject an embedded NUL in `column` before every DataFrame access.
2. Call `require_double_column`; its missing, wrong-type, and malformed-frame
   diagnostics occur before numerical shortcuts.
3. Return NaN for fewer than two raw values; remove NaN only through the
   existing `valid_values` helper; then return NaN for fewer than two cleaned
   values.
4. Return NaN for NaN `mar`; compute each `mar - value` and return NaN when
   that deviation is NaN, including same-signed infinity subtraction.
5. Compute the cubed downside LPM in original row order before the arithmetic
   mean. With strict `lpm3 < 1e-30`, return `+inf` only when `mu > mar`;
   otherwise return `(mu - mar) / std::pow(lpm3, 1.0 / 3.0)` directly.

The implementation does not finite-filter, sort, align timestamps, deduplicate
labels, clamp, add an epsilon, take an absolute value, replace `std::pow` with
`std::cbrt`, normalize zero, or translate Python warnings into a C++ channel.
Legal bounded non-NUL-terminated selectors work; a `std::string_view`
containing an embedded NUL is rejected before it can be truncated by a
downstream C-string boundary.

NaN is the only cleaned-away value. Positive infinity can yield `+inf`,
negative infinity and mixed signed infinities yield NaN according to direct
IEEE arithmetic, `mar=-inf` yields `+inf`, and same-signed return/MAR infinity
subtraction yields NaN. The overflow fixture `[-DBL_MAX, 0.0]` produces
negative zero, asserted by both equality to zero and `std::signbit`. The exact
threshold remains strict: `nextafter(1e-10, 0.0)^3` is below `1e-30`, while
`1e-10^3` is not.

## Mandatory Anaconda source oracle

The source oracle ran from `/Users/yunjinqi/Documents/new_projects/fincore`
with `PYTHONDONTWRITEBYTECODE=1` and the required command form
`/Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base python`.
It exited `0` under Python `3.11.8`, NumPy `1.26.4`, and pandas `3.0.3`, and
imported `fincore/metrics/ratios.py`. Its exact 25 lines were:

```text
ordinary=0.1587401051968199; warnings=0; messages=-
ordinary_mar_0.1=-0.9280157765364051; warnings=0; messages=-
dead_params=0.1587401051968199; warnings=0; messages=-
empty=nan; warnings=0; messages=-
singleton=nan; warnings=0; messages=-
nan_mixed=0.4807498567691359; warnings=0; messages=-
one_clean=nan; warnings=0; messages=-
all_nan=nan; warnings=0; messages=-
all_positive=inf; warnings=0; messages=-
equal_mar=nan; warnings=0; messages=-
strict_below_negative=nan; warnings=0; messages=-
strict_below_positive_mu=inf; warnings=0; messages=-
nominal_boundary=-0.9999999999999987; warnings=0; messages=-
positive_inf=inf; warnings=0; messages=-
negative_inf=nan; warnings=1; messages=invalid value encountered in scalar divide
both_inf=nan; warnings=1; messages=invalid value encountered in reduce
mar_nan=nan; warnings=0; messages=-
mar_pos_inf=nan; warnings=1; messages=invalid value encountered in scalar divide
mar_neg_inf=inf; warnings=0; messages=-
same_pos_inf_mar=nan; warnings=2; messages=invalid value encountered in subtract | invalid value encountered in scalar subtract
same_neg_inf_mar=nan; warnings=2; messages=invalid value encountered in subtract | invalid value encountered in scalar subtract
max_overflow=-0.0; warnings=1; messages=overflow encountered in power
duplicate_nonmonotonic=0.19078570709222184; warnings=0; messages=-
below_lpm3=9.999999999999997e-31; hex=0x1.4484bfeebc29ep-100; below=True
exact_lpm3=1e-30; hex=0x1.4484bfeebc2a0p-100; below=False
```

Python's whole checkout is currently dirty and has advanced from the design's
historical pin: HEAD is `2354bb560788f372502151a8b1b53accf536a6c0`, while
`fincore/metrics/ratios.py` itself is clean. This is therefore a verified
current-source semantic oracle, not a claim to reproduce the whole historical
checkout. The Kappa hunk's intervening changes are type-only/commentary, and
the current oracle exactly matches the designed output above. Python warning
counts and messages are source evidence only; native tests assert the result
classifications and values rather than emitting warnings.

## Genuine RED, GREEN, review, and configured-tree gates

The retained, ignored historical evidence directory
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-37-kappa-three-ratio/` is
evidence storage only; no Superpowers skill was invoked. Its genuine
pre-implementation `task-157-red-build.log` records:

```sh
cmake --build build --target fincore_tests --parallel 4 -- -k 0
```

at Task 156 baseline `2bf256d`, exiting `1`. Ninja attempted both
`ratios_test.cpp` and `metrics_umbrella_test.cpp`; its four substantive
diagnostics all report `no member named 'kappa_three_ratio' in namespace
'fincore'`. No CTest followed that failed build. The symbol is absent from the
baseline header, source, and tests. This historical RED is not fabricated or
rerun after implementation.

At exact clean Task 157 candidate `2ba633d`, direct current-tree commands
returned:

| Gate | Result |
| --- | --- |
| `cmake --build build --parallel 4` | exit 0; no work pending |
| `ctest --test-dir build -N` | exit 0; 478 registered |
| `ctest --test-dir build -R 'Ratios|MetricsUmbrella' --output-on-failure --parallel 4` | exit 0; 37/37 passed in 0.20 s |
| `ctest --test-dir build -R 'Dataframe|Basic|Returns|Drawdown|Ratios|Stats|PerfStats|AnalysisContext|Rolling' --output-on-failure --parallel 4` | exit 0; 167/167 passed in 1.24 s |
| `ctest --test-dir build --output-on-failure --parallel 4` | exit 0; 478/478 passed in 3.27 s |
| `git diff --check` and `git diff --cached --check` before this documentation | exit 0; no output and no staged content |

These gates are current configured-tree evidence only; the independently fresh
compiler, sanitizer, graphics, and dependency-provenance gates follow.

## Fresh acceptance gates and provenance

### AppleClang Debug headless

Fresh root `/tmp/fincore-phase37-apple-headless.dOvc31/build` configured exact
clean `2ba633d` with CMake `4.0.2`, Ninja `1.12.1`, `/usr/bin/c++`
(AppleClang `17.0.0.17000603`), Debug, testing ON, and ImPlot/demo OFF:

```sh
cmake -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation \
  -B /tmp/fincore-phase37-apple-headless.dOvc31/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_BUILD_IMPLOT_DEMO=OFF
cmake --build /tmp/fincore-phase37-apple-headless.dOvc31/build --parallel 4
ctest --test-dir /tmp/fincore-phase37-apple-headless.dOvc31/build -N
ctest --test-dir /tmp/fincore-phase37-apple-headless.dOvc31/build \
  -R 'Ratios|MetricsUmbrella' --output-on-failure --parallel 4
ctest --test-dir /tmp/fincore-phase37-apple-headless.dOvc31/build \
  --output-on-failure --parallel 4
```

All commands exited 0: configure, 116/116 Ninja build, 478 registrations,
37/37 focused tests, and 478/478 full tests. A case-insensitive build warning
scan found zero matches. This root has its own clean official GoogleTest
FetchContent checkout at tag `v1.15.2`, commit
`b514bdc898e2951020cbdca1304b75f5950d1f59`; no GoogleTest source-directory or
disconnected FetchContent override was supplied. Durable configure, build,
registration, focused, full, warning, and provenance logs remain in the parent
temporary root.

### Homebrew LLVM 22 ASan+UBSan headless

Fresh root `/tmp/fincore-task158-llvm-fetchcontent.qBSEW7/build` configured
with CMake `4.0.2`, Ninja `1.12.1`, Homebrew LLVM `22.1.8`, Debug, testing ON,
ImPlot/demo OFF, and both sanitizers ON. It used no GoogleTest source override
and normal FetchContent:

```sh
cmake -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation \
  -B /tmp/fincore-task158-llvm-fetchcontent.qBSEW7/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_BUILD_IMPLOT_DEMO=OFF -DFINCORE_ENABLE_ASAN=ON \
  -DFINCORE_ENABLE_UBSAN=ON -DFETCHCONTENT_QUIET=OFF
cmake --build /tmp/fincore-task158-llvm-fetchcontent.qBSEW7/build --parallel 4
ctest --test-dir /tmp/fincore-task158-llvm-fetchcontent.qBSEW7/build -N
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer \
ctest --test-dir /tmp/fincore-task158-llvm-fetchcontent.qBSEW7/build \
  -R 'Ratios|MetricsUmbrella' --output-on-failure --parallel 4
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer \
ctest --test-dir /tmp/fincore-task158-llvm-fetchcontent.qBSEW7/build \
  --output-on-failure --parallel 4
```

Configure and the 116/116 Ninja build exited 0. The root's own clean official
GoogleTest checkout is `v1.15.2` at
`b514bdc898e2951020cbdca1304b75f5950d1f59`; cache entries prove an empty
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` and both disconnected options OFF.
Registration was 478, strict-runtime focused CTest was 37/37 in 3.12 s, and
strict-runtime full CTest was 478/478 in 39.59 s with:

```text
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer
```

Durable logs report no AddressSanitizer, UndefinedBehaviorSanitizer,
LeakSanitizer, or `runtime error:` diagnostic. The captured
`ratios.cpp` compile command and the `fincore_tests` final link each contain
one `-fsanitize=address` and one `-fsanitize=undefined` flag (two occurrences
of each across those two anchors). `otool -L` and a focused
`DYLD_PRINT_LIBRARIES` Kappa test show the LLVM ASan dylib loaded; `nm -a`
finds `___ubsan_handle_*` references. The sole warning is upstream GoogleTest
`gtest-printers.h:524` `char8_t` to `char32_t` conversion, not project source.

### Local ImGui/ImPlot compile-link-only

Fresh durable root `/tmp/fincore-phase37-implot-durable.8l2UTo/build` was
configured from its clean detached source
`/tmp/fincore-phase37-implot-durable.8l2UTo/source` at exact `2ba633d`, with
CMake `4.0.2`, Ninja `1.12.1`, AppleClang `17.0.0.17000603`, local clean
DataFrame `90aa4acf`, ImGui `46d39d56`, and ImPlot `7eeb9168`. Testing,
`FINCORE_BUILD_IMPLOT`, and `FINCORE_BUILD_IMPLOT_DEMO` were ON; sanitizers
were OFF. Its own normal FetchContent GoogleTest checkout is clean official
`v1.15.2` at `b514bdc898e2951020cbdca1304b75f5950d1f59`; cache evidence has
an empty `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` and both disconnected options
OFF. Durable `commands.log`, configure/build/CTest logs, provenance, warning,
artifact, process-audit, and `toolchain-versions.log` logs remain in the
parent temporary root.

```sh
cmake -S /tmp/fincore-phase37-implot-durable.8l2UTo/source \
  -B /tmp/fincore-phase37-implot-durable.8l2UTo/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/cc \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON -DFINCORE_ENABLE_ASAN=OFF \
  -DFINCORE_ENABLE_UBSAN=OFF
cmake --build /tmp/fincore-phase37-implot-durable.8l2UTo/build --parallel 4
ctest --test-dir /tmp/fincore-phase37-implot-durable.8l2UTo/build -N -V
ctest --test-dir /tmp/fincore-phase37-implot-durable.8l2UTo/build \
  --output-on-failure --parallel 4
```

All four commands exited 0. The Debug build completed 130/130 and
compiled/linked `fincore_implot`, ImGui, ImPlot, `fincore_tests`, and the
arm64 Mach-O `fincore_implot_demo`. The retained Ninja link command includes
`libfincore_implot.a`, `libimplot.a`, `libimgui.a`, DataFrame, GLFW 3.4, and
macOS OpenGL. Verbose registration found 479 tests: the 478 headless tests
plus exactly one `ImPlotAdapterCompile` test. Full CTest passed 479/479 in
1.93 s.

The demo is intentionally **NOT_RUN**: this acceptance invokes no demo command
and the retained post-CTest process inspection finds no `fincore_implot_demo`
process. No GUI was launched. An initial artifact audit incorrectly checked an
`examples/` output path and recorded that non-existence; the retained corrected
audit resolves the actual build-root executable from Ninja's link command,
verifies its arm64 Mach-O identity and dynamic GLFW/OpenGL links, and still
does not execute it. The build had exactly three compiler warning diagnostics,
all macOS OpenGL deprecations in `examples/implot_dashboard.cpp:51-53`
(`glViewport`, `glClearColor`, and `glClear`); header notes and macro context
are separately counted, not additional warnings. This is compile/link and
adapter-test evidence, not visual, interactive, or GUI-runtime acceptance.

### Invalid-ImGui negative configure

Fresh durable root `/tmp/fincore-phase37-invalid-imgui-durable.KIE68m/build`
used a clean detached source at exact `2ba633d`, local DataFrame and ImPlot,
ImPlot ON, testing/demo OFF, and an absent dynamic
`FINCORE_IMGUI_SOURCE_DIR`. CMake `4.0.2` with AppleClang
`17.0.0.17000603` exited exactly 1. The dynamic path was absent both before
and after configuration; `command-manifest.txt`, `configure-status.log`,
`configure.log`, `diagnostic-comparison.log`, and `artifact-absence.log` are
retained in that temporary root. The exact command was:

```sh
cmake -S /tmp/fincore-phase37-invalid-imgui-durable.KIE68m/source \
  -B /tmp/fincore-phase37-invalid-imgui-durable.KIE68m/build -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=OFF \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase37-invalid-imgui-durable.KIE68m/absent-imgui-source
```

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-phase37-invalid-imgui-durable.KIE68m/absent-imgui-source'
```

The normalized actual-versus-expected comparison passed, including that exact
dynamic path and `cmake/fincore_implot.cmake:47` call site. No Ninja/Makefile,
`fincore_implot`, demo, test executable, or target-directory artifact was
generated. Status is configure expected-negative **PASS**; build **NOT_RUN**;
CTest **NOT_RUN**; demo/GUI **NOT_RUN**. This negative gate does not substitute
for a positive adapter or demo build.

## Dependency provenance and exclusions

The local CMake-selected source dependencies were clean when audited:

| Dependency | HEAD |
| --- | --- |
| cloudQuant DataFrame | `90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230` |
| Dear ImGui | `46d39d56febc2a00bdd2270dc88c8a13f2a0441a` |
| cloudQuant ImPlot | `7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4` |
| GoogleTest in each fresh positive root | `b514bdc898e2951020cbdca1304b75f5950d1f59` (`v1.15.2`) |

No local dependency was fetched, cloned, fabricated, or copied by the gates;
GoogleTest is the only normal FetchContent dependency and each positive root
has its own clean official checkout. Phase 37 does not implement aliases,
Python/container adapters, adjusted or conditional Sharpe, `mar_ratio`,
calendar resampling, EVT, providers, reports as a native subsystem, plot
behavior/runtime, packages, or any GUI-runtime validation.

Task 158 accepts only the isolated implementation branch at `2ba633d`. It
does not turn local build success into a release, package, live-service, or
full-parity claim, and it does not reconcile the separate primary branch or
push any ref.
