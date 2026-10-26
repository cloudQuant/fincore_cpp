# Phase 36 native R-cubed acceptance

## Candidate, route, and exact scope

This record accepts Plan A: pure native C++23 over the local cloudQuant
DataFrame, without embedding Python or pandas. Phase 36 implements `r_cubed`
as the smallest remaining **Empyrical-registered, non-alias metric
calculation whose numerical leaf is already native**.

That is a narrow route claim, not a claim that `r_cubed` is the smallest
absent Python name or utility anywhere in the package. The route audit found:

- `stock_market_correlation`, `bond_market_correlation`, and
  `futures_market_correlation` are compatibility names for correlation logic
  already represented by native `safe_correlation`;
- `annualized_cumulative_return`, `cum_returns`, `cum_returns_final`,
  `value_at_risk`, and cross-module stats wrappers similarly map to existing
  canonical native symbols; and
- `simulation.base.annualize` is a smaller standalone utility, but it is
  neither exported by the simulation facade nor registered on `Empyrical`.

The remaining non-alias metric ranking put `r_cubed` ahead of
`kappa_three_ratio`, adjusted Sharpe, `mar_ratio`, and conditional Sharpe.
Those alternatives require new partial-moment, sample-count, clamp,
drawdown/annualization, percentile, tail-subset, or nonfinite policies.
Weekly/monthly consecutive metrics still require pandas resampling semantics,
and Hill EVT requires a new result/API/source surface plus threshold policy.

The reviewed Phase 36 design and executable plan commit is
`1bfadde5f0e18905a2ccc25c566f22a6ebdb55f8` (`docs: plan phase 36 r cubed`).
It is the direct child of accepted Phase 35 trace
`879a5eb21b17ad3a432875e804d00ac339d8e43d` and adds exactly:

- `docs/design/2026-08-13-phase-36-r-cubed-design.md`
- `docs/plans/2026-08-13-fincore-cpp-phase-36-r-cubed.md`

The initial design specification review found one Important missing boundary
and two Minor wording gaps: the test plan did not yet require valid typed-
empty or maximum-finite overflow inputs, native infinity parity was too exact,
and Python registry metadata could be confused with actual Empyrical dispatch.
The plan-quality review additionally found one Important mutation gap: no
finite endpoint-rounding fixture killed a `[-1, 1]` clamp. The same two-file
candidate was corrected to add those cases and distinguish registry metadata
from explicit Empyrical reachability. Final independent specification and
plan-quality re-reviews each returned **PASS**, Critical/Important/Minor
`0/0/0`, before the two-path commit.

Task 153 implementation commit
`4df362c5ab9a78632101eeaeebfc27c2ea1c2a1d` (`feat: add native r cubed`) is
the direct child of `1bfadde` and changes exactly:

- `include/fincore/metrics/stats.hpp`
- `src/metrics/stats.cpp`
- `tests/unit/stats_test.cpp`
- `tests/unit/metrics_umbrella_test.cpp`

It contains 317 insertions and one deletion. The fresh independent Task 153
specification review returned **PASS**, Critical/Important/Minor `0/0/0`,
after checking the source oracle, genuine dual-consumer RED, API/control flow,
scope, and fresh current-tree gates. The separate task-quality review returned
**PASS**, Critical/Important/Minor `0/0/0`, after its own source/test/mutation
audit, current build/registration/focused/related/full gates, and LLVM 22
ASan+UBSan focused verification. No review-fix commit was required.

The acceptance candidate contains only `README.md` and this new acceptance
record.

Its initial independent document/gate review reported
Critical/Important/Minor `0/1/1`: the graphics/invalid evidence omitted exact
invoked command strings, and one sentence conflated the native API with the
flat top-level Python export boundary. The accepted command manifest and
separate Python-export wording closed both findings without rerunning a gate
or changing a result. Final independent Spec and Task-Quality re-reviews each
returned **PASS**, Critical/Important/Minor `0/0/0`.

## Python exports, exact native API, and source mapping

Python source checkout `/Users/yunjinqi/Documents/new_projects/fincore` at
`1fd5f33e84bef2ec22e23eb9319f6e363084324b` defines `r_cubed` in
`fincore/metrics/stats.py` and lists it in that module's `__all__`.
`DUAL_RETURNS_FACTOR_REGISTRY` also lists the name as metadata, but the current
`Empyrical` implementation does not consume that registry. Its actual facade
reachability comes from an explicit `r_cubed` dual method that fills both
stored inputs. The symbol is not a flat top-level `fincore` export and is not
consumed by the current reports or analysis context.

Python has the two-input signature `r_cubed(returns, factor_returns)`. The
native DataFrame core declares exactly one overload:

```cpp
double r_cubed(
    const dataframe& returns,
    const dataframe& benchmark,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

The two inputs, two bounded column selectors, and scalar result use existing
native types. A future Python ndarray adapter may synthesize positional
indexes; the native API itself adds no result struct, selector object,
automatic-input API, array/Series overload, or compatibility alias. The phase
also adds no flat top-level Python fincore export.

Existing `<fincore/metrics.hpp>` already includes `stats.hpp`,
`src/metrics/stats.cpp` was already compiled into the `fincore` target, and
the leaf/facade consumers were already registered. No facade include, CMake
entry, source target, dependency, report, plot source, package, or GUI runtime
was added.

## Signed formula, control flow, validation, and ownership

The source calculation is signed: `correlation**3`. A negative correlation
must produce a negative cube; this is not R-squared, CAPM R-squared, absolute
correlation, or a nonnegative score. The ordinary mathematical range is
`[-1, 1]`, subject to floating-point endpoint rounding. Phase 36 deliberately
adds no clamp.

The native definition preserves this exact order:

1. Reject an embedded NUL in `returns_column_name` with
   `fincore::input_error` before any DataFrame call.
2. Reject an embedded NUL in `benchmark_column_name` with a distinct
   `fincore::input_error`, also before any DataFrame call.
3. Invoke public `safe_correlation` exactly once with the two frames and exact
   selectors.
4. Return NaN when that leaf returns NaN.
5. Otherwise return
   `correlation * correlation * correlation` directly.

The wrapper copies no alignment, filtering, mean, variance, covariance, or
Pearson formula. It does not delegate through `capm_r_squared`, square or take
an absolute value, use `std::pow`, or clamp the result.

The explicit NUL checks prevent a bounded selector such as
`"strategy\0suffix"` from being truncated at a downstream C-string boundary
to valid `"strategy"`. Legal bounded non-NUL-terminated views remain accepted.
After both wrapper checks, the public leaf retains parameter order: returns-
side empty/missing/wrong-type/malformed structure is observed before the
benchmark-side equivalent. Structurally valid typed-empty inputs reach the
leaf's numerical NaN result rather than being rejected by a wrapper shortcut.

The returned scalar owns its value and retains no input span, selector view,
index, or DataFrame storage.

## Timestamp alignment and finite-value compatibility boundary

The public `safe_correlation` leaf remains the sole owner of numerical input
policy. It validates both selected genuine-double columns and frame shapes,
aligns common timestamps in left-input order, retains pairs only when both
values are finite, and returns NaN for fewer than two pairs, invalid moments,
population standard deviations below `1e-15`, or overflow. Unique
nonmonotonic indexes are therefore aligned by timestamp key rather than raw
position. Duplicate timestamps inherit the existing native alignment policy;
this phase does not attempt pandas duplicate-index broadcast/reindex parity.

For unique pandas indexes, Python's outer alignment followed by pairwise NaN
masking agrees with common-timestamp intersection. The compatibility boundary
is signed infinity: Python removes NaN pairs but leaves infinity for NumPy's
correlation arithmetic, which returns NaN and may emit `RuntimeWarning`.
Native `safe_correlation` already filters every nonfinite pair. Phase 36
inherits that accepted policy instead of adding a third filtering path.

For returns `[0.01, +inf, -0.02, 0.03]` and benchmark
`[0.005, 0.1, -0.01, -0.01]`, Python returns NaN with one
`invalid value encountered in subtract` warning. Native filtering keeps three
finite pairs, producing correlation near `0.1147078669352809` and R-cubed
near `0.0015093140386221175`. Replacing positive infinity with negative
infinity gives the same native result. NumPy warnings are source evidence
only; the C++ scalar API gains no warning channel.

The finite identical two-point fixture `{-10.0, -0.3}` also documents native
rounding: on both acceptance compilers, `safe_correlation` is approximately
`1.0000000000000002` and its direct cube approximately
`1.0000000000000007`. Both are strictly greater than one. This is evidence
for the direct, unclamped product, not a range defect.

## Mandatory Anaconda Python source oracle

Task 153 ran the plan's exact 14-case heredoc from the Python source checkout
using the user's Anaconda base environment through the required `conda run`
command, with `PYTHONDONTWRITEBYTECODE=1`. The exact runtime was
`/Users/yunjinqi/opt/anaconda3/bin/conda`, and the arguments were
`run --no-capture-output -n base python`. It exited `0` and printed:

```text
ordinary_positive=0.9407744656599973; warnings=0; messages=-
ordinary_negative=-0.9407744656599973; warnings=0; messages=-
perfect_positive=1.0; warnings=0; messages=-
perfect_negative=-1.0; warnings=0; messages=-
pairwise_nan=1.0; warnings=0; messages=-
constant=nan; warnings=0; messages=-
tiny_variation=nan; warnings=0; messages=-
empty=nan; warnings=0; messages=-
one_pair=nan; warnings=0; messages=-
positive_inf_pair=nan; warnings=1; messages=invalid value encountered in subtract
negative_inf_pair=nan; warnings=1; messages=invalid value encountered in subtract
huge_pair=nan; warnings=1; messages=overflow encountered in multiply
misaligned_series=-0.9999999999999987; warnings=0; messages=-
one_common_series=nan; warnings=0; messages=-
```

The command, source HEAD, cases, warnings, and raw output are retained in
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-36-r-cubed/source-oracle.md`.
Native tests use matching finite cases, independently lock timestamp-key
alignment, and separately lock the intentional infinity divergence. They do
not assert Python warnings from C++.

## Genuine RED, GREEN, and implementation review chain

Before either production path changed from Task 152 base `1bfadde`, both
registered consumers added the exact
`double (*)(const dataframe&, const dataframe&, std::string_view,
std::string_view)` function-pointer `static_assert` and an actual two-argument
default-column call. The leaf consumer simultaneously removed its direct
`<fincore/error.hpp>` include, keeping `<fincore/metrics/stats.hpp>` as its
only fincore include. The facade consumer used its existing
`<fincore/metrics.hpp>` public surface.

The RED forced Ninja to continue through both consumer compilations:

```zsh
red_log=.superpowers/sdd/2026-08-13-fincore-cpp-phase-36-r-cubed/task-153-red-build.log
set +e
cmake --build build --target fincore_tests --parallel 4 -- -k 0 \
  >"$red_log" 2>&1
red_exit=$?
set -e
test "$red_exit" -ne 0
rg -n 'stats_test\.cpp' "$red_log"
rg -n 'metrics_umbrella_test\.cpp' "$red_log"
rg -n 'r_cubed' "$red_log"
```

Recorded `red_exit` was `1`. `stats_test.cpp` reported 19 missing-symbol
errors before Clang's consequent error-limit marker, while
`metrics_umbrella_test.cpp` reported two. Every substantive compiler
diagnostic had the same root cause: `no member named 'r_cubed' in namespace
'fincore'`. No CTest ran after that failed build. The raw log and structured
audit are retained as `task-153-red-build.log` and `task-153-red.md` in the
ignored Phase 36 evidence directory.

The first minimal production build exited `0`, and the exact focused selection
passed `29/29`. The final mutation-resistant matrix locks both-TU signature
and default compilation, signed ordinary/perfect goldens, public-leaf cube
parity, timestamp-key rather than positional alignment, pairwise NaN, sample
and variance boundaries, typed-empty and maximum-finite overflow behavior,
signed-infinity divergence, the unclamped endpoint overshoot, named/bounded
selectors, both pre-DataFrame NUL branches and their parameter order, both
sides' structural order, scalar ownership, and facade reachability.

The independent Task 153 PASS results and absence of a review-fix commit are
recorded in the exact-scope section above.

## Current configured-tree gates

At exact implementation HEAD
`4df362c5ab9a78632101eeaeebfc27c2ea1c2a1d`, each current-tree command ran
separately and returned its direct status:

- `cmake --build build --parallel 4`: exit `0`, `ninja: no work to do.`;
- `ctest --test-dir build -N`: exit `0`, exactly `467` tests registered;
- focused `Stats|MetricsUmbrella`: exit `0`, `29/29` passed in `0.25`
  seconds;
- related `Dataframe|Stats|AlphaBeta|Timing|Rolling|Yearly`: exit `0`,
  `97/97` passed in `0.75` seconds;
- full CTest: exit `0`, `467/467` passed in `3.26` seconds;
- pre-document `git diff --check`: exit `0`, no output; and
- pre-document `git diff --cached --check`: exit `0`, no output and no staged
  content.

The raw logs are retained as `task-154-current-*.log` in the ignored Phase 36
evidence directory. These are current configured-tree results only; they are
not evidence for a fresh compiler, sanitizer, graphics, or dependency-
provenance claim.

## Fresh acceptance gates

### AppleClang 17 Debug headless

Root: `/tmp/fincore-phase36-task154-apple.gdDIDQ`, configured from exact HEAD
`4df362c`. A fresh Ninja/Debug configure used CMake `4.0.2`, Ninja `1.12.1`,
AppleClang `17.0.0.17000603`, testing ON, and ImPlot, the demo, ASan, and UBSan
all OFF. Configure exited `0`; build exited `0` after `116/116` Ninja steps;
and `ctest -N` registered exactly `467` tests. Focused
`Stats|MetricsUmbrella` passed `29/29` in `0.18` seconds, and full CTest passed
`467/467` in `3.35` seconds. A case-insensitive configure/build `warning:`
scan found no match.

### Homebrew LLVM 22 ASan+UBSan headless

Root: `/tmp/fincore-phase36-task154-llvm.wmBg2O`, configured from exact HEAD
`4df362c`. A fresh Ninja/Debug configure used CMake `4.0.2`, Ninja `1.12.1`,
Homebrew Clang `22.1.8`, `llvm-symbolizer` `22.1.8`, testing ON, ASan and UBSan
ON, and ImPlot and the demo OFF. Configure exited `0`; build exited `0` after
`116/116` Ninja steps; and `ctest -N` registered exactly `467` tests.

The complete instrumented suite ran under these strict settings:

```text
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer
ctest --test-dir /tmp/fincore-phase36-task154-llvm.wmBg2O/build \
  --output-on-failure --parallel 4
```

It passed `467/467` in `43.84` seconds. The sanitizer diagnostic scan found no
AddressSanitizer, UndefinedBehaviorSanitizer, LeakSanitizer, sanitizer
summary, or `runtime error:` match. Ninja commands contain exactly 35
address-sanitizer and 35 undefined-sanitizer flag occurrences. Exact command
anchors show the `stats.cpp` compile and final `fincore_tests` link each carry
both flags exactly once. `otool -L` finds exactly one LLVM ASan runtime, and a
separate DYLD-print-libraries execution of the Phase 36 ordinary signed
R-cubed test confirms that exact runtime was loaded once and the test passed
`1/1`. An `nm` audit additionally finds 13 UBSan symbol lines in the
instrumented executable.

The LLVM build emitted exactly one warning: upstream GoogleTest
`gtest-printers.h:524` reports a `char8_t` to `char32_t`
`-Wcharacter-conversion`. The project/other-warning scan found no match.

### Local ImGui/ImPlot compile-link-only gate

Root: `/tmp/fincore-phase36-task154-implot.houYwj`, configured from exact HEAD
`4df362c`. A fresh CMake `4.0.2`/Ninja `1.12.1`/AppleClang
`17.0.0.17000603` Debug configure used local DataFrame, ImGui, and ImPlot
checkouts with testing, the adapter, and the demo ON; ASan and UBSan were OFF.
Configure exited `0`; build exited `0` after `130/130` Ninja steps.

The executor recovered the exact accepted invocation strings after the runs
and retained them in `task-154-exact-command-manifest.md`; this repaired a
documentation omission and did not rerun a gate. The positive configure,
build, verbose registration, and full-test commands were:

```sh
cmake -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation -B /tmp/fincore-phase36-task154-implot.houYwj/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_ENABLE_ASAN=OFF \
  -DFINCORE_ENABLE_UBSAN=OFF
cmake --build /tmp/fincore-phase36-task154-implot.houYwj/build --parallel 4
ctest --test-dir /tmp/fincore-phase36-task154-implot.houYwj/build -N -V
ctest --test-dir /tmp/fincore-phase36-task154-implot.houYwj/build --output-on-failure --parallel 4
```

Verbose `ctest -N` exited `0` and registered exactly `468` tests: the `467`
headless tests plus `ImPlotAdapterCompile` exactly once as test `#468`. Full
CTest exited `0` and passed `468/468` in `3.53` seconds; the adapter started
and passed exactly once.

The build-root `fincore_implot_demo` is a regular, non-symlink, executable
arm64 Mach-O. It links Homebrew GLFW, current version `3.4.0`, the system
OpenGL framework, libc++, and libSystem. Both registration and full-test logs
contain zero demo references, and an exact process-name audit found no demo
process. The demo was never executed and no GUI launched. This is only
compile/link plus adapter-test evidence, not plotting appearance, interaction,
or runtime acceptance.

The build emitted exactly three warnings, all in the project demo and all from
the known macOS OpenGL-deprecation family: one each for `glViewport`,
`glClearColor`, and `glClear`. The other-warning scan found no match.

### Invalid-ImGui negative configure

Accepted root: `/tmp/fincore-phase36-task154-invalid-imgui.HM0zzs`. Its
dynamic `not-a-dear-imgui-checkout` path was absent before configure and
remained absent afterwards. A fresh AppleClang 17 Ninja/Debug configure used
local DataFrame and ImPlot, testing and the demo OFF, ImPlot ON, both
sanitizers OFF, and the absent dynamic path as `FINCORE_IMGUI_SOURCE_DIR`.
Configure exited exactly `1`.

After first requiring the dynamic target to be absent with
`test ! -e /tmp/fincore-phase36-task154-invalid-imgui.HM0zzs/not-a-dear-imgui-checkout`,
the executor invoked this exact configure command:

```sh
cmake -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation -B /tmp/fincore-phase36-task154-invalid-imgui.HM0zzs/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase36-task154-invalid-imgui.HM0zzs/not-a-dear-imgui-checkout \
  -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=OFF \
  -DFINCORE_ENABLE_ASAN=OFF \
  -DFINCORE_ENABLE_UBSAN=OFF
```

`cmake/fincore_implot.cmake:47` occurred once. A whitespace-normalized scanner
matched the complete multiline diagnostic and exact dynamic path once, while
the warning scan found no match:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-phase36-task154-invalid-imgui.HM0zzs/not-a-dear-imgui-checkout'
```

The first unique positive and negative roots were accepted directly; no
discarded or corrected preflight root is being presented as acceptance
evidence.

## Dependency provenance

The Apple, LLVM, and ImPlot caches point `FINCORE_DATAFRAME_SOURCE_DIR` at the
local clean cloudQuant DataFrame checkout, HEAD
`90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`, origin
`https://github.com/cloudQuant/DataFrame.git`. The ImPlot cache separately
points
`FINCORE_IMGUI_SOURCE_DIR` at the local clean ImGui checkout, HEAD
`46d39d56febc2a00bdd2270dc88c8a13f2a0441a`, origin
`https://github.com/ocornut/imgui.git`, and `FINCORE_IMPLOT_SOURCE_DIR` at the
local clean cloudQuant ImPlot checkout, HEAD
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`, origin
`https://github.com/cloudQuant/implot.git`.

GoogleTest is the actual clean official FetchContent checkout in the Apple,
LLVM, and ImPlot positive build roots, HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`, exact tag `v1.15.2`, origin
`https://github.com/google/googletest.git`. The cache has an empty
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` override and both
`FETCHCONTENT_FULLY_DISCONNECTED` and `FETCHCONTENT_UPDATES_DISCONNECTED` OFF.
The Apple and LLVM roots use distinct fresh `_deps/googletest-src` checkouts
at that same exact tag and origin. The graphics cache also records Ninja,
`/usr/bin/c++`, the exact project source, all gate options and checkout paths,
GLFW CMake package directory
`/opt/homebrew/lib/cmake/glfw3`, and the system OpenGL framework. The linked
Homebrew GLFW reports `3.4.0`. No local DataFrame, ImGui, or ImPlot source was
fetched, cloned, fabricated, or copied by this gate.

Raw fresh evidence is retained under the ignored Phase 36 SDD directory:
`task-154-core-gates.md` plus `task-154-logs/` cover Apple and LLVM;
`task-154-implot-*` covers graphics, artifacts, warnings, and provenance; and
`task-154-invalid-imgui-*` covers the negative configure and exact diagnostic.
The post-run `task-154-exact-command-manifest.md` preserves the executor's
exact graphics and invalid-ImGui command testimony alongside those raw logs
and captured status files.

## Exclusions

Acceptance covers only the native two-DataFrame scalar `r_cubed` leaf and its
existing metrics-facade reachability. It adds no result struct, helper,
automatic-input, ndarray/Series, multi-column, DataFrame-returning or alias
overload, new CMake target, dependency, embedded Python/pandas runtime,
NumPy/SciPy component, provider, network call, solver, plot source, report,
package, or GUI runtime.

This phase does not implement market-correlation compatibility aliases,
`simulation.base.annualize`, `kappa_three_ratio`, adjusted or conditional
Sharpe, `mar_ratio`, calendar-resampled consecutive statistics, EVT,
providers, reports, plots, packaging/release work, or Python container
adapters. ImPlot acceptance remains strictly compile/link plus adapter-test
evidence; no plotting appearance, interaction, or GUI runtime is accepted.

## Non-self-referential traceability

Task 152 is commit `1bfadde5f0e18905a2ccc25c566f22a6ebdb55f8`, the direct
child of accepted Phase 35 trace
`879a5eb21b17ad3a432875e804d00ac339d8e43d`. It adds exactly the Phase 36
design and executable plan:

- `docs/design/2026-08-13-phase-36-r-cubed-design.md`
- `docs/plans/2026-08-13-fincore-cpp-phase-36-r-cubed.md`

Task 153 is commit `4df362c5ab9a78632101eeaeebfc27c2ea1c2a1d`, the direct
child of Task 152. Its complete implementation chain required no review-fix
commit and changes exactly:

- `include/fincore/metrics/stats.hpp`
- `src/metrics/stats.cpp`
- `tests/unit/stats_test.cpp`
- `tests/unit/metrics_umbrella_test.cpp`

Task 154 is commit `7a116466434ae5e6d0040644d43901201d1405bc`, the direct
child of Task 153, and changes exactly `README.md` plus this acceptance record.

Task 155 is documentation-only and does not amend Task 152, Task 153, Task
154, or any other prior commit. It neither states nor anticipates its own
commit SHA.
