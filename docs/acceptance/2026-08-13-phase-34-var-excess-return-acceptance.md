# Phase 34 native VaR excess return acceptance

## Candidate, route, and exact scope

This record accepts Plan A: pure native C++23 over the local cloudQuant
DataFrame, without an embedded Python/pandas runtime. Phase 34 implements
Python `fincore.metrics.risk.var_excess_return` as the smallest useful
dependency-closed public metric still missing after Phase 33. Its successful
path composes the already accepted public native `annualization_factor`,
`annual_return`, and `historical_value_at_risk` leaves in the existing risk
module.

The route audit explicitly checked the apparently smaller `loss_rate` route.
That symbol was already declared in `metrics/stats.hpp`, implemented in
`metrics/stats.cpp`, exported by the metrics facade, and covered by dedicated
statistics tests, so it was not parity debt. `mar_ratio`, adjusted or
conditional Sharpe, weekly/monthly consecutive statistics, and EVT require
additional arithmetic, cleaning, resampling, or API policy. Phase 33's
exclusion of `var_excess_return` was phase-local; Phase 34 selects that debt
without rewriting any Phase 33 document or commit.

The reviewed Phase 34 design and executable plan is
`7b3d5f346021b3f76e61649f7d2b199592caeb06` (`docs: plan phase 34 var excess
return`), whose exact two-path scope is:

- `docs/design/2026-08-13-phase-34-var-excess-return-design.md`
- `docs/plans/2026-08-13-fincore-cpp-phase-34-var-excess-return.md`

Task 145 implementation commit
`56c821b75be72493f7f4837fe2488d8bb8821d1f` (`feat: add native var excess
return`) is its direct child and changes exactly:

- `include/fincore/metrics/risk.hpp`
- `src/metrics/risk.cpp`
- `tests/unit/extended_risk_test.cpp`
- `tests/unit/metrics_umbrella_test.cpp`

The direct review-fix commit
`39e6dc35339dc7dfcbeda66141fcac1bbf3caff8` (`test: harden var excess return
zero var guard`) changes only `tests/unit/extended_risk_test.cpp`, adding nine
test lines and no production, CMake, facade-include, or dependency change.

The independent specification review returned **PASS**, Critical/Important/
Minor `0/0/0`. The initial independent task-quality review reported one
Important test-strength finding: the Python `[0, 0]` fixture still returns NaN
if the explicit zero-VaR guard is deleted. The test-only review fix added a
nondegenerate `{0, 0.1}` cutoff-zero case with positive annual return and
exactly zero public VaR. After that separate non-amended commit, task-quality
re-review returned **PASS**, Critical/Important/Minor `0/0/0`. The already
passing specification review covered the unchanged production contract.

At the Task 146 review handoff, the acceptance candidate consists only of
`README.md` and this new acceptance record. Both remain unstaged and
uncommitted until fresh independent document/gate review returns PASS. The
primary review returned specification **PASS** and task quality **PASS**, each
with Critical/Important/Minor `0/0/0`. A second independent review returned
specification **PASS** `0/0/0` and one Minor evidence-retention finding: the
LLVM raw directory had not captured the exact CTest environment. The complete
instrumented suite was rerun with a wrapper that prints the environment and
command before executing them; the resulting raw log closes that gap. Scoped
re-review then returned specification **PASS** and task quality **PASS**, each
with Critical/Important/Minor `0/0/0`, and no new finding. This section
intentionally does not state or anticipate a Task 146 commit SHA.

## Exact public API and source mapping

Python source checkout
`/Users/yunjinqi/Documents/new_projects/fincore` at
`1fd5f33e84bef2ec22e23eb9319f6e363084324b` defines the five-argument source
function in `fincore/metrics/risk.py`. `<fincore/metrics/risk.hpp>` now
declares exactly one native overload:

```cpp
double var_excess_return(
    const dataframe& returns,
    double cutoff = 0.05,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);
```

The native `frequency` enum replaces Python's free-form period string, and the
last argument selects one genuine-double DataFrame column. The Python API is
one-dimensional and scalar, so Phase 34 adds no multi-column or
array-returning overload. Existing `<fincore/metrics.hpp>` already includes
`risk.hpp`, `src/metrics/risk.cpp` was already part of the `fincore` target,
and both unit consumers were already registered. No facade include, CMake
entry, source target, or dependency changed.

Python calls `annual_return` and `value_at_risk`; native code maps those to
public `annual_return` and `historical_value_at_risk`. The separate public
`annualization_factor` call adapts the enum/optional native API to the numeric
factor accepted by native `annual_return`.

After native structural validation, the implementation preserves the source's
observable numerical call order:

1. Reject `column.find('\0')` before any DataFrame C-string boundary.
2. Resolve the selected column through public `require_double_column`.
3. Return NaN when the selected raw span has fewer than two rows.
4. Call public `annualization_factor(period, annualization)` exactly once.
5. Call public `annual_return(returns, factor, column)` exactly once.
6. Call public `historical_value_at_risk(returns, cutoff, column)` exactly
   once.
7. Return NaN for zero or NaN VaR, or for NaN annual return.
8. Otherwise return `(annualized_return - risk_free) / abs(value_at_risk)`
   directly.

No private compounding, annualization, percentile, or finite-input formula is
copied. The quotient is not passed through a finite-only clamp.

## Validation, ordering, and IEEE behavior

The embedded-NUL check precedes every DataFrame call, while the public column
helper validates the exact bounded selector, column presence, genuine-double
type, and index/column length before the raw `<2` shortcut. Consequently,
missing, empty-selector, wrong-typed, and short-column frames remain
`fincore::input_error` even when their data length is below two. A bounded
non-NUL-terminated selector remains supported.

For a structurally valid zero- or one-row double column, the raw-length
shortcut returns NaN before frequency, cutoff, risk-free, or annualization is
interpreted. For two or more rows, annualization is resolved before VaR: an
unknown frequency without an override throws `input_error` before an invalid
cutoff can return NaN. With an explicit annualization override, the enum is
ignored and an invalid cutoff still returns NaN.

The accepted public leaf policies define the remaining results:

- cutoff endpoints `0` and `1` are accepted, despite the Python docstring's
  informal `(0, 1)` wording;
- nonfinite or out-of-range cutoff returns NaN;
- NaN or either signed infinity in returns makes public historical VaR NaN,
  therefore the composed result is NaN;
- exactly zero VaR returns NaN, including the mutation-resistant case where
  annual return is positive;
- `risk_free` is not finite-validated, so positive infinity produces negative
  infinity and NaN propagates;
- a legitimate positive-infinite annual return or quotient is preserved; and
- the function owns no returned DataFrame or retained selector/input view.

An explicit annualization value bypasses frequency lookup and is delegated
unchanged to the accepted native `annual_return` leaf. On the four-row oracle
fixture, native values `0`, `-3`, NaN, positive infinity, and negative infinity
produce `0`, `-0.8982857935175003`, NaN, positive infinity, and
`-22.727272727272727`. Python instead raises `ZeroDivisionError` for `0` and
both signed infinities because its intermediate `num_years` calculation
divides by the annualization factor. Phase 34 preserves the established native
leaf contract rather than adding a new validation layer.

## Mandatory Anaconda Python source oracle

Task 145 ran the plan's exact heredoc from the Python source checkout with
`/Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base
python`. It exited `0`, emitted no warning, and printed:

```text
daily_default 649.7873043614145
weekly 22.994057986872573
explicit_7 2.2423926838665866
bad_period_explicit_7 2.2423926838665866
cutoff_0 -4.998700305323209
cutoff_1 -9.997400610646418
zero_var nan
nan_return nan
pos_inf_return nan
bad_cutoff nan
risk_free_pos_inf -inf
one_row_bad_everything nan
bad_period ValueError Period cannot be 'bad'. Can be 'daily', 'weekly', 'monthly', 'quarterly', 'yearly'.
bad_period_bad_cutoff ValueError Period cannot be 'bad'. Can be 'daily', 'weekly', 'monthly', 'quarterly', 'yearly'.
annualization 0.0 ZeroDivisionError float division by zero
annualization -3.0 -0.8982857935175003
annualization nan nan
annualization inf ZeroDivisionError float division by zero
annualization -inf ZeroDivisionError float division by zero
```

The complete command and output are retained in
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-34-var-excess-return/source-oracle.md`.
The native exception type for an unknown frequency is `fincore::input_error`,
and the explicit annualization divergences are the accepted boundary described
above.

## Genuine RED, GREEN, and independent review chain

Before either production file changed from Task 144 base `7b3d5f3`, the
existing leaf and facade consumers were edited. The leaf removed its direct
`<fincore/error.hpp>` include so `<fincore/metrics/risk.hpp>` was its only
fincore header; the facade consumer continued to use
`<fincore/metrics.hpp>`. Both added the exact six-parameter function-pointer
signature and a behavior call.

`cmake --build build --parallel 4` then exited `1`. The leaf translation unit
failed at two uses and the facade translation unit failed at two uses; every
diagnostic was `error: no member named 'var_excess_return' in namespace
'fincore'`. Neither declaration nor definition existed, and no CTest ran
after the failed build. The original raw compiler output and scope proof are
retained as `task-145-red-build.log` and `task-145-red.md` in the ignored
Phase 34 evidence directory.

The first minimal implementation build exited `0`, followed by focused
`17/17`. After the planned behavior matrix was complete, Task 145 passed
focused `25/25`, related `106/106`, and full `448/448`; its four-path commit
was `56c821b`. The independent quality review's zero-VaR mutation finding was
corrected only in the leaf test. Fresh review-fix verification passed focused
`26/26` and full `449/449`, followed by the one-path commit `39e6dc3` and final
task-quality PASS.

The final tests lock the exact signature/default call, Python finite goldens,
direct three-leaf parity, cutoff endpoints, ordering, native annualization
overrides, raw-short branches, independently zero VaR, invalid/nonfinite
cutoff and returns, IEEE risk-free/result infinities, named/default/bounded
selectors, embedded NUL, structural errors, and metrics-facade visibility.

## Task 146 current configured headless gate

Raw current-tree logs are retained under
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-34-var-excess-return/task-146-logs/current/`.
At exact implementation/fix HEAD `39e6dc3`:

- `cmake --build build --parallel 4`: exit `0`, no work;
- `ctest --test-dir build -N`: exactly `449` registered tests;
- focused `ExtendedRisk|MetricsUmbrella`: `26/26`, exit `0`;
- related plan regex `Risk|Returns|Yearly|Basic|PerfStats`: `107/107`, exit
  `0`;
- full CTest: `449/449`, exit `0`; and
- `git diff --check`: exit `0`, no output.

## Task 146 fresh gates

Raw fresh-gate logs are retained in the `apple-headless`, `llvm-sanitized`,
`implot`, and `invalid-imgui` subdirectories beside the current-tree logs.

### AppleClang 17 Debug headless

Root: `/tmp/fincore-phase34-task146-apple.bsNdNT`. A fresh Ninja configure used
`/usr/bin/c++`, Debug, the local DataFrame checkout, testing ON, ImPlot/demo
OFF, and both sanitizers OFF. CMake 4.3.2, Ninja
`1.13.0.git.kitware.jobserver-pipe-1`, and AppleClang
`17.0.0.17000603` (`clang-1700.6.3.2`) were recorded. Configure and build
exited `0`; Ninja completed `116/116`. `ctest -N` registered exactly `449`;
focused `ExtendedRisk|MetricsUmbrella` passed `26/26`, and full CTest passed
`449/449` in 3.36 seconds. A case-insensitive `warning:` scan over configure
and build logs returned `rg` exit `1`, meaning no match.

### Homebrew LLVM 22 ASan+UBSan headless

Root: `/tmp/fincore-phase34-task146-llvm.PyGizZ`. A fresh Ninja/Debug configure
used `/opt/homebrew/opt/llvm/bin/clang++`, the local DataFrame checkout,
testing ON, ImPlot/demo OFF, and both `FINCORE_ENABLE_ASAN=ON` and
`FINCORE_ENABLE_UBSAN=ON`. CMake 4.3.2, Ninja
`1.13.0.git.kitware.jobserver-pipe-1`, Homebrew Clang 22.1.8, and
`llvm-symbolizer` 22.1.8 were recorded. Configure and build exited `0`; Ninja
completed `116/116`, and `ctest -N` registered exactly `449`.

The initial full CTest passed `449/449` in 42.81 seconds. Independent document
review correctly found that the original raw log did not capture its exact
environment. An ignored review-evidence wrapper therefore printed
`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1`,
`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`,
`ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer`, and the
exact `ctest --test-dir ... --output-on-failure --parallel 4` command before
exporting and executing them in the same fresh build root. That rerun passed
`449/449` in 42.39 seconds. A scan over the rerun log for AddressSanitizer,
UndefinedBehaviorSanitizer, LeakSanitizer, sanitizer summaries, and `runtime
error:` returned `rg` exit `1`, meaning no diagnostic. Ninja commands contain
35 address- and 35 undefined-sanitizer flag occurrences; `risk.cpp`
compilation and the final `fincore_tests` link both contain the two flags. The
executable loads exactly one LLVM 22 `libclang_rt.asan_osx_dynamic.dylib`.

The LLVM build emitted two non-sanitizer warning families. Upstream GoogleTest
`gtest-printers.h:524` emitted one `char8_t` to `char32_t`
`-Wcharacter-conversion`. Apple `ld` emitted 106 instances of the single
template `can't parse dwarf compilation unit info in <object>`, across project
and GoogleTest objects. Linking still exited `0` and the complete instrumented
suite passed; neither warning family is a sanitizer diagnostic.

### Local ImGui/ImPlot compile-link-only gate

Root: `/tmp/fincore-phase34-task146-implot.A7cH1a`. A fresh Ninja/Debug
configure used `/usr/bin/c++` and `/usr/bin/cc`, the local DataFrame, ImGui,
and ImPlot checkouts, testing/ImPlot/demo ON, and both sanitizers OFF. CMake
4.0.2, Ninja 1.12.1, and AppleClang `17.0.0.17000603` were recorded. Configure
and build exited `0`; Ninja completed `130/130`.

`ctest -N` registered exactly `450`: the `449` headless tests plus
`ImPlotAdapterCompile` exactly once. Full CTest with `--parallel 4` passed
`450/450`; the adapter ran exactly once. The build emitted exactly three
warnings from one known family: macOS OpenGL deprecations for `glViewport`,
`glClearColor`, and `glClear` in `examples/implot_dashboard.cpp`; no other
warning matched.

The build-root `fincore_implot_demo` was a regular, non-symlink, executable
arm64 Mach-O. Its dynamic links included Homebrew GLFW 3.4 and the Apple SDK
OpenGL framework. It had zero CTest registration/runtime mentions and was
never invoked by this gate; a process scan found zero instances. No GUI was
launched. This is compile/link plus adapter-test evidence only, not plotting
appearance, interaction, or runtime acceptance.

A discarded preliminary root
`/tmp/fincore-phase34-task146-implot.ZXMrta` completed configure output but its
wrapper then failed while assigning zsh's read-only `status` variable. Its
logs explicitly mark `accepted_as_gate_evidence=0`; all positive claims above
come only from the fresh replacement root.

### Invalid-ImGui negative configure

Root: `/tmp/fincore-phase34-task146-invalid-imgui.GEuhdv`. The dynamic path
`not-a-dear-imgui-checkout` was absent before configure and remained absent
afterwards. A fresh Ninja/Debug AppleClang configure used local DataFrame and
ImPlot, testing/demo OFF, ImPlot ON, both sanitizers OFF, and that missing path
as `FINCORE_IMGUI_SOURCE_DIR`. It exited exactly `1` at
`cmake/fincore_implot.cmake:47`.

A whitespace-tolerant multiline PCRE matched the complete physically wrapped
diagnostic and exact dynamic path, exit `0`; the path appeared exactly once:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-phase34-task146-invalid-imgui.GEuhdv/not-a-dear-imgui-checkout'
```

## Dependency provenance

Every fresh cache points `FINCORE_DATAFRAME_SOURCE_DIR` at local cloudQuant
DataFrame HEAD `90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`, origin
`https://github.com/cloudQuant/DataFrame.git`. GoogleTest is the actual
official FetchContent checkout, origin
`https://github.com/google/googletest.git`, HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`, exact tag `v1.15.2`;
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` is empty and
both `FETCHCONTENT_FULLY_DISCONNECTED` and
`FETCHCONTENT_UPDATES_DISCONNECTED` are OFF, so it is not presented as a
local override.

The graphics gate uses local ImGui HEAD
`46d39d56febc2a00bdd2270dc88c8a13f2a0441a`, origin
`https://github.com/ocornut/imgui.git`, and local cloudQuant ImPlot HEAD
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`, origin
`https://github.com/cloudQuant/implot.git`. CMake resolves Homebrew GLFW 3.4
from `/opt/homebrew/lib/cmake/glfw3`; the demo links
`/opt/homebrew/opt/glfw/lib/libglfw.3.dylib` and the Apple SDK OpenGL framework.
No DataFrame, ImGui, or ImPlot source was fetched, cloned, fabricated, or
copied by these gates.

## Exclusions

Acceptance covers only the native one-column scalar `var_excess_return`
composer and its existing metrics facade. It adds no DataFrame-returning or
multi-column overload, alias, CMake target, dependency, embedded Python/pandas
runtime, NumPy/SciPy component, provider, network call, solver, plot source,
report, package, or GUI runtime.

This phase does not implement `mar_ratio`, adjusted or conditional Sharpe,
weekly/monthly consecutive statistics, EVT, market aliases, providers,
reports, or plots. ImPlot evidence is strictly compile/link and adapter-test
evidence; it is never plotting appearance, interaction, or runtime acceptance.
