# Phase 33 native common-sense ratio acceptance

## Candidate, route, and exact scope

This record accepts Plan A: a pure native C++23 implementation over the local
cloudQuant DataFrame, without an embedded Python/pandas runtime. Phase 32
completed every successful local public calculation path in Python
`fincore.attribution.style`: style analysis, performance by style, rolling
style tilts, and correlation attribution now have native calculation APIs.
Python's remaining `StyleResult` mapping helpers are container conveniences,
and `fetch_style_factors` is an always-failing provider stub rather than a
local calculation path.

Phase 33 therefore leaves the style route and implements the smallest useful
dependency-closed package gap: Python `metrics.ratios.common_sense_ratio`.
The source function composes `tail_ratio` and `win_rate`; both are already
accepted public native APIs. The nearby alternatives remain separate work:
`mar_ratio` and `var_excess_return` add annualization or risk policy;
weekly/monthly consecutive statistics require pandas-resampling decisions;
EVT opens a new advanced-risk API; and provider, solver, plot, report, and
stub routes are not dependency-closed scalar work.

The reviewed Phase 33 design and implementation plan is
`7ee5a2566d9e6270c6c9df09815e265e265875c6` (`docs: plan phase 33 common sense
ratio`), whose exact two-path scope is:

- `docs/design/2026-08-13-phase-33-common-sense-ratio-design.md`
- `docs/plans/2026-08-13-fincore-cpp-phase-33-common-sense-ratio.md`

Task 141 implementation commit
`ae9f49d9a694faf1343f72769a2f2279421e1465` (`feat: add native common sense
ratio`) is its direct child and changes exactly:

- `include/fincore/metrics/ratios.hpp`
- `src/metrics/ratios.cpp`
- `tests/unit/extended_ratios_test.cpp`
- `tests/unit/metrics_umbrella_test.cpp`

The direct review-fix commit
`759af93ecdc8cabad62d08c849fd6927ae44d504` (`test: harden common sense ratio
boundaries`) changes only the same two unit-test files. It adds independent
undefined-tail/nonzero-win and unclamped-positive-infinite-tail regressions;
it makes no production, CMake, facade-include, or dependency change.

Task 140's independent plan review and Task 141's final independent
specification and task-quality reviews all returned **PASS**. The final Task
141 review count is Critical/Important/Minor `0/0/0` for both specification
and quality.

At the Task 142 review handoff, the acceptance candidate consists only of
`README.md` and this new acceptance record. Both remain unstaged and
uncommitted until a fresh independent document/gate review returns PASS. This
section intentionally does not state or anticipate a Task 142 commit SHA. The
independent review returned Spec **PASS** and Task-Quality **PASS**, each with
Critical/Important/Minor `0/0/0`; it made no workspace edit and required no
correction.

## Exact public API and source mapping

`<fincore/metrics/ratios.hpp>` declares exactly one new native symbol:

```cpp
double common_sense_ratio(
    const dataframe& returns,
    std::string_view column = returns_column);
```

Python also exposes `metrics.stats.common_sense_ratio`, but that function is
only a compatibility delegate to the ratio implementation. The flat native
`fincore` namespace therefore has one declaration in the ratio leaf rather
than a duplicate stats declaration. Existing `<fincore/metrics.hpp>` already
includes `ratios.hpp`, `risk.hpp`, and `stats.hpp`, so the function is visible
through the public facade without adding an include. `src/metrics/ratios.cpp`
was already part of the `fincore` target and both tests were already
registered; Phase 33 changes no CMake file or source list.

The implementation performs these operations in order:

1. Reject `column.find('\0')` before any DataFrame C-string boundary.
2. Resolve the selected column through public `require_double_column`.
3. Return NaN when the selected raw span has fewer than two rows.
4. Call public `tail_ratio(returns, column)` exactly once.
5. Call public `win_rate(returns, column)` exactly once.
6. Return NaN if either delegate is NaN or the win rate is zero.
7. Return positive infinity when the win rate is exactly one.
8. Otherwise return `tail_ratio * win_rate / (1 - win_rate)` directly.

No private percentile or win-count formula is copied. Only `std::isnan`
gates delegated results: a legitimate positive-infinite tail survives, and
the general quotient is not passed through a finite clamp. The accepted
native leaves constrain a non-NaN win rate to `[0,1]`.

## Validation, raw-count branches, and ownership

The embedded-NUL rejection precedes every DataFrame call, preventing a
length-aware selector such as `"returns\0suffix"` from being truncated at a
C-string boundary. A bounded non-NUL-terminated view is valid because the
public column helper owns its exact extent before lookup. Missing, empty,
wrong-typed, and index-mismatched selected columns remain `fincore::input_error`
even when their raw row count would be below two.

The minimum is deliberately based on raw selected rows rather than finite
rows:

- a genuine-double typed-empty column and a one-row finite column return NaN;
- `{.01, NaN}` has two raw rows, delegates to one retained finite value, and
  returns positive infinity because both accepted leaves report an all-win
  retained sample;
- two NaNs return NaN;
- zeros remain in the win-rate denominator but are not wins;
- zero wins returns NaN; and
- all finite wins with at least two raw rows returns positive infinity.

The independent `{0,0,.01}` regression proves the delegated tail may be NaN
while public `win_rate` is the defined nonzero value `1/3`; the final result
must still be NaN. A second all-finite extreme regression uses nineteen
negative denormals plus `double::max()`: public `tail_ratio` is positive
infinity, public `win_rate` is `0.05`, and the final result remains positive
infinity. Those cases kill both a broad `isfinite` rejection and a finite
quotient clamp.

The function returns a scalar and keeps no column or selector view after the
call. Named and default calls share the same validation and delegate path.

## Deliberate signed-infinity divergence

Python drops NaN but retains signed infinity. NumPy percentile interpolation
therefore produces NaN for the recorded positive- and negative-infinity
fixtures, accompanied by RuntimeWarnings. The accepted native `tail_ratio`
and `win_rate` instead use the shared native finite-value policy, dropping
NaN and both signed infinities.

Phase 33 composes those accepted public leaves instead of introducing a third
cleaning rule. Both native fixtures
`{.01, +inf, -.02, .03}` and `{.01, -inf, -.02, .03}` are evaluated from the
retained finite values and return `3.294117647058822`. This is a documented
native compatibility boundary, not a claim of NumPy infinity parity.

## Mandatory Anaconda Python source oracle

Task 141 ran the plan's exact command from
`/Users/yunjinqi/Documents/new_projects/fincore` with
`/Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base
python`. It exited `0` and printed:

```text
mixed 4 1.4594594594594592 0.5 1.4594594594594592
zeros_mixed 5 1.6249999999999998 0.4 1.0833333333333333
all_win 3 2.6363636363636362 1.0 inf
no_win 3 0.05263157894736847 0.0 nan
single 1 1.0 1.0 nan
one_finite_two_raw 2 1.0 1.0 inf
nan_mixed 4 1.6470588235294115 0.6666666666666666 3.294117647058822
all_nan 2 nan nan nan
zero_tail 2 19.0 0.5 19.0
positive_infinity 4 nan 0.75 nan
negative_infinity 4 nan 0.5 nan
```

NumPy also emitted two percentile interpolation RuntimeWarnings for the
signed-infinity cases. The complete command, warnings, and output are retained
in `.superpowers/sdd/2026-08-13-fincore-cpp-phase-33-common-sense-ratio/source-oracle.md`.

## Genuine RED, GREEN, evidence repair, and reviews

Before either production declaration or definition existed, the existing
leaf test and metrics-facade test were given compile uses. The leaf consumer
included only `<fincore/metrics/ratios.hpp>` among fincore headers; the facade
consumer used `<fincore/metrics.hpp>`. The configured build exited `1` because
both translation units could not find `fincore::common_sense_ratio`. No CTest
ran after the failed build, so no stale binary is cited.

The original terminal RED was described in the contemporaneous report but was
not copied to a raw ignored log. Review correctly rejected that evidence gap.
The repair is explicitly a review-after exact-base recovery, not a claim that
the recovered log is the original timestamp output: an isolated clone at
exact Task 140 base `7ee5a25` received only the committed two-test diff while
`ratios.hpp` and `ratios.cpp` remained byte-identical to the base. A fresh full
build and targeted facade-object build both exited `1` only on the missing
symbol. The raw 300-line compiler log is retained as
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-33-common-sense-ratio/task-141-red-build.log`,
and `task-141-red.md` records commands, scope proof, exit statuses, and the
chronology limitation.

The initial implementation GREEN passed focused `19/19`, related `47/47`,
and full `438/438` CTest. Independent review found no production defect but
requested the two stronger delegate-boundary fixtures and retained RED. After
the test-only correction, final Task 141 verification passed focused `20/20`,
related `48/48`, and full `439/439`; both scoped re-reviews returned PASS with
Critical/Important/Minor `0/0/0`.

## Task 142 current and fresh gates

Raw Task 142 logs are retained below
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-33-common-sense-ratio/task-142-logs/`.

### Current configured headless tree

- `cmake --build build --parallel 4`: exit `0`, no work.
- `ctest --test-dir build -N`: exactly `439` tests.
- focused `ExtendedRatios|MetricsUmbrella`: `20/20`, exit `0`.
- full CTest: `439/439`, exit `0`.
- `git diff --check`: exit `0`.

The raw logs and corrected explicit status file are in `current/`.

### Fresh AppleClang 17 Debug headless

Root: `/tmp/fincore-phase33-task142-apple.aXm6Uo`. The exact configure was:

```sh
cmake -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation \
  -B /tmp/fincore-phase33-task142-apple.aXm6Uo/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_BUILD_IMPLOT_DEMO=OFF -DFINCORE_ENABLE_ASAN=OFF \
  -DFINCORE_ENABLE_UBSAN=OFF
```

Configure and `cmake --build ... --parallel 4` exited `0`; Ninja completed
`116/116`. CMake was 4.3.2, Ninja was
`1.13.0.git.kitware.jobserver-pipe-1`, and the cache identified AppleClang
`17.0.0.17000603` (`clang-1700.6.3.2`). `ctest -N` registered exactly `439`;
focused `ExtendedRatios|MetricsUmbrella` passed `20/20`; full CTest passed
`439/439`. A case-insensitive `warning:` scan over the configure and build
logs returned `rg` exit `1`, meaning zero matches.

### Fresh Homebrew LLVM 22 ASan+UBSan headless

Root: `/tmp/fincore-phase33-task142-llvm.qdAaFt`. The configure used the same
source, generator, type, local DataFrame, and headless testing options as the
Apple gate, but selected `/opt/homebrew/opt/llvm/bin/clang++` and set both
`FINCORE_ENABLE_ASAN=ON` and `FINCORE_ENABLE_UBSAN=ON`. Configure and
`cmake --build ... --parallel 4` exited `0`; Ninja completed `116/116`.
Homebrew Clang and `llvm-symbolizer` were both 22.1.8, and `ctest -N`
registered exactly `439`.

Full CTest ran as:

```sh
env ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1 \
    UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer \
  ctest --test-dir /tmp/fincore-phase33-task142-llvm.qdAaFt/build \
    --output-on-failure --parallel 4
```

It passed `439/439` in 45.14 seconds. A scan over configure, build, and CTest
logs for AddressSanitizer, UndefinedBehaviorSanitizer, LeakSanitizer,
sanitizer summaries, and `runtime error:` returned `rg` exit `1`, meaning no
diagnostic. Ninja's commands contain 35 address and 35 undefined sanitizer
flags; `ratios.cpp` compilation and the final `fincore_tests` link contain
both, and the test executable loads LLVM 22's
`libclang_rt.asan_osx_dynamic.dylib`.

The LLVM build emitted two non-sanitizer warning families. Upstream GoogleTest
`gtest-printers.h:524` emitted one `char8_t` to `char32_t`
`-Wcharacter-conversion`. Apple `ld` emitted 106 instances of the single
template `can't parse dwarf compilation unit info in <object>`, across project
and GoogleTest objects. Linking still exited `0` and the complete instrumented
suite passed; neither warning family is reported as a sanitizer finding.

### Fresh local ImGui/ImPlot compile-link gate

Root: `/tmp/fincore-phase33-task142-implot.pH3jUj`. The exact configure was:

```sh
cmake -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation \
  -B /tmp/fincore-phase33-task142-implot.pH3jUj/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DCMAKE_C_COMPILER=/usr/bin/cc \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON -DFINCORE_ENABLE_ASAN=OFF \
  -DFINCORE_ENABLE_UBSAN=OFF
```

CMake 4.0.2/AppleClang 17 configure and build exited `0`; Ninja completed
`130/130`. `ctest -N` registered exactly `440`, with
`ImPlotAdapterCompile` exactly once as test `#440`; full CTest passed
`440/440`. The only three build warnings were the known macOS OpenGL
deprecations for `glViewport`, `glClearColor`, and `glClear` in
`examples/implot_dashboard.cpp`.

The build-root `fincore_implot_demo` was checked as a regular executable and
was never invoked; no GUI was launched. It is absent from CTest registration.
This gate is compile/link and adapter-test evidence only, not plotting
appearance, interaction, or runtime acceptance.

### Fresh invalid-ImGui negative configure

Root: `/tmp/fincore-phase33-task142-invalid-imgui.kB0BAp`. A precondition
check proved
`/tmp/fincore-phase33-task142-invalid-imgui.kB0BAp/not-a-dear-imgui-checkout`
did not exist. A fresh Ninja/Debug configure used the local DataFrame and
ImPlot paths, `FINCORE_BUILD_IMPLOT=ON`, testing/demo OFF, and that missing
path as `FINCORE_IMGUI_SOURCE_DIR`. It exited exactly `1` at
`cmake/fincore_implot.cmake:47`. This whitespace-tolerant multiline PCRE
matched the complete physical-line-wrapped diagnostic and exact path, exit
`0`:

```text
FINCORE_IMGUI_SOURCE_DIR\s+must\s+name\s+a\s+Dear\s+ImGui\s+checkout\s+with\s+all\s+required\s+files;\s+received\s+'\Q/tmp/fincore-phase33-task142-invalid-imgui.kB0BAp/not-a-dear-imgui-checkout\E'
```

The captured diagnostic is:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-phase33-task142-invalid-imgui.kB0BAp/not-a-dear-imgui-checkout'
```

## Dependency provenance and exclusions

Every fresh cache points `FINCORE_DATAFRAME_SOURCE_DIR` at the local checkout
`/Users/yunjinqi/Documents/new_projects/DataFrame`, HEAD
`90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`, origin
`https://github.com/cloudQuant/DataFrame.git`. GoogleTest is the actual
official FetchContent checkout, origin
`https://github.com/google/googletest.git`, HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`, exact tag `v1.15.2`;
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` is empty and
`FETCHCONTENT_FULLY_DISCONNECTED=OFF`, so it is not presented as a local
override.

The graphics gate uses local ImGui HEAD
`46d39d56febc2a00bdd2270dc88c8a13f2a0441a`, origin
`https://github.com/ocornut/imgui.git`, and local cloudQuant ImPlot HEAD
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`, origin
`https://github.com/cloudQuant/implot.git`. CMake resolves Homebrew GLFW 3.4
from `/opt/homebrew/lib/cmake/glfw3`; the demo links
`/opt/homebrew/opt/glfw/lib/libglfw.3.dylib` (current version 3.4.0) and the
Apple SDK OpenGL framework. No
DataFrame, ImGui, or ImPlot source was fetched, cloned, fabricated, or copied
by these gates.

Acceptance covers only the native common-sense-ratio composer and its existing
metrics facade. It adds no new CMake target, dependency, alias, DataFrame
output, Python/pandas embedding, provider, network call, solver, plot source,
report, package, or GUI runtime. It does not implement `mar_ratio`,
`var_excess_return`, conditional ratios, weekly/monthly consecutive
statistics, EVT, market aliases, providers, reports, or plots. ImPlot evidence
is strictly compile/link and adapter-test evidence.
