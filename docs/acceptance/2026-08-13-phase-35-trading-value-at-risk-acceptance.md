# Phase 35 native trading value at risk acceptance

## Candidate, route, and exact scope

This record accepts Plan A: pure native C++23 over the local cloudQuant
DataFrame, without embedding Python or pandas. Phase 35 implements Python
`fincore.metrics.risk.trading_value_at_risk` as the smallest genuine
dependency-closed public metric gap found after Phase 34.

The route audit first considered `mar_ratio`, then rejected it as the larger
slice because it adds drawdown and annualization composition, NaN-only
cleaning, no-drawdown ordering, invalid-frequency/override behavior, and a
final infinity clamp. The audit also confirmed that `loss_rate` is already
declared in `metrics/stats.hpp`, implemented in `metrics/stats.cpp`, exported
by `<fincore/metrics.hpp>`, and covered by statistics tests. `r_cubed`,
`kappa_three_ratio`, adjusted or conditional Sharpe, calendar-resampled
consecutive statistics, and EVT require additional alignment, moments,
partial moments, cleaning, resampling, or result/API policy.

The reviewed Phase 35 design and executable plan commit is
`a9d695e93d9f9c399bf633e4f825f7b1b98552f9` (`docs: plan phase 35 trading
value at risk`). It is the direct child of accepted Phase 34 trace
`ad621ca09454f1e7071ec227bc8694ab749ad7bb` and adds exactly:

- `docs/design/2026-08-13-phase-35-trading-value-at-risk-design.md`
- `docs/plans/2026-08-13-fincore-cpp-phase-35-trading-value-at-risk.md`

The initial design specification review found one Minor wording issue about
whether native ndarray/Series overloads would broaden the Python contract or
only the selected native API. The wording was corrected. The plan-quality
review found two Important execution gaps and one Minor command-clarity issue:
the RED did not yet guarantee both translation units, its zsh status capture
was unsafe, and quoted CTest/final-review sequencing needed to be explicit.
The same candidate was corrected to use Ninja `-k 0`, `red_exit`, exact quoted
commands, and fresh configure/build/registration/focused/full order. Final
independent specification and plan/executability re-reviews each returned
**PASS**, Critical/Important/Minor `0/0/0`, before the two-path commit.

Task 149 implementation commit
`e52d288022cd49494c14c669b669a61260dbef80` (`feat: add native trading value
at risk`) is the direct child of `a9d695e` and changes exactly:

- `include/fincore/metrics/risk.hpp`
- `src/metrics/risk.cpp`
- `tests/unit/extended_risk_test.cpp`
- `tests/unit/metrics_umbrella_test.cpp`

It contains 196 insertions and no deletion. The fresh independent Task 149
specification review returned **PASS**, Critical/Important/Minor `0/0/0`, and
confirmed the oracle, genuine dual RED, exact API/control flow, scope, and
fresh build/focused/full results. The separate fresh task-quality review also
returned **PASS**, Critical/Important/Minor `0/0/0`, with no surviving
requested mutant, lifetime/undefined-behavior/include problem, or scope drift.
It independently reran the current build, registration, focused, related, and
full gates and an additional LLVM 22 ASan+UBSan rebuild plus focused `34/34`.
No review-fix commit was required.

The Task 150 README/acceptance candidate remained entirely unstaged for a
fresh independent read-only document/gate review. Its initial Spec and
Task-Quality verdicts each found the same single Minor: the displayed LLVM
`ctest --test-dir` command omitted the actual `/build` suffix retained in the
raw log. No gate was rerun and no result was changed; the command transcription
alone was corrected. The same reviewer directly compared the corrected line
to the raw command and returned final **Spec PASS** and **Task-Quality PASS**,
each with Critical/Important/Minor `0/0/0` and no new finding. This record
intentionally contains no self-referential commit identifier.

## Python exports, exact native API, and source mapping

Python source checkout
`/Users/yunjinqi/Documents/new_projects/fincore` at
`1fd5f33e84bef2ec22e23eb9319f6e363084324b` defines
`trading_value_at_risk` in `fincore/metrics/risk.py`. It is exposed through
that risk module, the lazy `metrics.risk_module`, and the `Empyrical`
classmethod registry. It is not a flat top-level `fincore` symbol, is not in
the dual-returns registry, and is not consumed by the current reports or
analysis context.

The Python source signature is
`trading_value_at_risk(returns, period=None, sigma=2.0)`, but its body never
reads `period`. `<fincore/metrics/risk.hpp>` therefore declares exactly one
native overload without a misleading ignored parameter:

```cpp
double trading_value_at_risk(
    const dataframe& returns,
    double sigma = 2.0,
    std::string_view column = returns_column);
```

The last argument selects one genuine-double DataFrame column. The native API
adds no `period`, `frequency`, optional annualization, hidden period
validation, multi-column form, ndarray/Series overload, automatic-returns
form, or alias. A future Python adapter may preserve Python's call shape and
discard `period` before invoking this native leaf.

Existing `<fincore/metrics.hpp>` already includes `risk.hpp`,
`src/metrics/risk.cpp` was already compiled into the `fincore` target, and the
leaf/facade tests were already registered. No facade include, CMake entry,
source target, dependency, report, plot source, package, or GUI runtime was
added.

The native one-column DataFrame maps its raw selected-column span to the
NumPy-array source path. This matters because a NumPy array propagates raw NaN,
whereas a pandas Series can dispatch reductions that skip NaN. Phase 35 does
not reproduce that container-dependent pandas dispatch.

## Control flow, validation, and ownership

The implementation preserves this exact order:

1. Reject `column.find('\0')` with `fincore::input_error` before any
   DataFrame C-string boundary.
2. Resolve the selected column through `require_double_column(returns,
   column)`.
3. Return NaN when the selected raw span has fewer than two values.
4. Compute the returned mean stage once with `detail::mean(values)`.
5. Compute sample variance once with `detail::sample_variance(values)` and
   take its square root.
6. Return `average - sigma * standard_deviation` directly.

The accepted `sample_variance` helper performs its own mean pass; no helper was
changed merely to fuse those reductions. The definition does not use
`detail::finite_values`, `annual_volatility`, historical VaR, population
variance, annualization, a zero-standard-deviation guard, sigma domain guard,
finite-result helper, or private NumPy-compatible reduction.

`require_double_column` validates the exact bounded selector, column
presence, genuine-double type, and index/column length before the raw `<2`
shortcut. Consequently an empty selector, missing column, wrong type, or
malformed selected-column length remains `input_error` even for a short frame.
A legal bounded non-NUL-terminated `string_view` remains accepted, while an
embedded-NUL view cannot be truncated into a valid column name. A structurally
valid zero- or one-row genuine-double column returns NaN before `sigma` enters
arithmetic. The scalar result retains no input span, selector view, index, or
DataFrame storage.

## Raw NaN, infinity, and native arithmetic boundaries

The numerical contract is direct C++/IEEE arithmetic:

- sample variance uses denominator `n - 1`, not population `n`;
- a two-element constant series has exactly zero deviation and returns its
  mean for every finite `sigma`;
- negative and zero `sigma` values are valid, and NaN or either signed
  infinity is not domain-rejected;
- raw NaN is not filtered and propagates to NaN;
- raw signed infinity is not filtered and normally makes the variance path
  NaN;
- zero standard deviation multiplied by signed infinity remains NaN instead
  of receiving a special case;
- legitimate final negative or positive infinity is preserved rather than
  clamped; and
- overflow, cancellation, and reduction order follow the existing native
  `detail::mean` and `detail::sample_variance` helpers.

NumPy may use a different internal reduction order for extreme values. That is
an accepted native arithmetic boundary, not a reason to introduce a private
compatibility reduction. NumPy `RuntimeWarning` observations below are source
evidence only; C++ gains no warning channel.

## Mandatory Anaconda Python source oracle

Task 149 ran the plan's exact 19-case heredoc from the Python source checkout
using
`/Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base
python`, with `PYTHONDONTWRITEBYTECODE=1`. It exited `0` and printed:

```text
ordinary_default=-0.043665562901805; warnings=0; messages=-
ordinary_sigma_1=-0.018499448117569168; warnings=0; messages=-
ordinary_sigma_0=0.006666666666666665; warnings=0; messages=-
ordinary_sigma_neg3=0.08216501101937417; warnings=0; messages=-
ordinary_sigma_nan=nan; warnings=0; messages=-
ordinary_sigma_pos_inf=-inf; warnings=0; messages=-
ordinary_sigma_neg_inf=inf; warnings=0; messages=-
bad_period_ignored=-0.018499448117569168; warnings=0; messages=-
empty_bad_sigma=nan; warnings=0; messages=-
single_bad_sigma=nan; warnings=0; messages=-
flat_positive=0.1; warnings=0; messages=-
flat_positive_sigma_pos_inf=nan; warnings=1; messages=invalid value encountered in scalar multiply
flat_zero_sigma_pos_inf=nan; warnings=1; messages=invalid value encountered in scalar multiply
nan_mixed=nan; warnings=0; messages=-
pos_inf_mixed=nan; warnings=1; messages=invalid value encountered in subtract
neg_inf_mixed=nan; warnings=1; messages=invalid value encountered in subtract
both_inf=nan; warnings=2; messages=invalid value encountered in reduce | invalid value encountered in reduce
max_symmetric=-inf; warnings=1; messages=overflow encountered in multiply
max_symmetric_sigma_neg2=inf; warnings=1; messages=overflow encountered in multiply
```

The command, input mapping, source HEAD, and raw output are retained in
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-35-trading-value-at-risk/source-oracle.md`.
The invalid object-valued `period` is ignored on full-length input, and the
invalid object-valued `sigma` is not interpreted for empty or one-row input.
Native tests assert finite values or NaN/infinity classification and sign;
they do not claim that C++ emits Python warnings.

## Genuine RED, GREEN, and implementation review chain

Before either production file changed from Task 148 base `a9d695e`, both
registered consumers added the exact
`double (*)(const dataframe&, double, std::string_view)` function-pointer
`static_assert` and an actual one-argument default call. The leaf consumer kept
`<fincore/metrics/risk.hpp>` as its only fincore include, while the facade
consumer used its existing `<fincore/metrics.hpp>` public surface.

The exact RED command forced Ninja through both compilations:

```zsh
red_log=.superpowers/sdd/2026-08-13-fincore-cpp-phase-35-trading-value-at-risk/task-149-red-build.log
set +e
cmake --build build --target fincore_tests --parallel 4 -- -k 0 \
  >"$red_log" 2>&1
red_exit=$?
set -e
test "$red_exit" -ne 0
rg -n 'extended_risk_test\.cpp' "$red_log"
rg -n 'metrics_umbrella_test\.cpp' "$red_log"
rg -n 'trading_value_at_risk' "$red_log"
```

Recorded `red_exit` was `1`. Each translation unit failed at its signature and
default call, producing four instances only of `error: no member named
'trading_value_at_risk' in namespace 'fincore'`. No other compiler-error root
cause appeared, and no CTest ran after the failed build. The raw 42-line log
and structured audit are retained as `task-149-red-build.log` and
`task-149-red.md` in the ignored Phase 35 evidence directory.

The first minimal production build exited `0` in 11 Ninja steps and immediate
focused CTest passed `27/27`. The complete mutation-resistant matrix then
passed focused `34/34`, related `65/65`, and full `457/457` before the
four-path commit `e52d288`.

The final leaf/facade tests lock the exact signature and default use, ordinary
goldens, independent `ddof=1` versus `ddof=0`, finite/NaN/signed-infinity
sigma, typed short inputs, exact two-element constants, `zero * infinity`,
raw NaN and signed infinity with two finite survivors, symmetric `DBL_MAX`
signed-infinity results, named/default/bounded/embedded-NUL selectors,
structural ordering, and facade reachability. The independent specification
and task-quality PASS results and absence of a review-fix commit are recorded
in the scope section above.

## Current configured-tree gates

At exact implementation HEAD
`e52d288022cd49494c14c669b669a61260dbef80`, each current-tree command ran
separately and returned its direct status:

- `cmake --build build --parallel 4`: exit `0`, `ninja: no work to do.`;
- `ctest --test-dir build -N`: exit `0`, exactly `457` tests registered;
- focused `ExtendedRisk|MetricsUmbrella`: exit `0`, `34/34` passed in `0.20`
  seconds;
- related `Risk|Stats|Dataframe`: exit `0`, `65/65` passed in `0.34`
  seconds;
- full CTest: exit `0`, `457/457` passed in `2.75` seconds; and
- pre-document `git diff --check`: exit `0`, no output.

The exact commands, counts, elapsed CTest times, and boundary are retained in
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-35-trading-value-at-risk/task-150-current-gates.md`.
These are current configured-tree results only; they are not evidence for a
fresh compiler, sanitizer, graphics, or dependency-provenance claim.

## Fresh acceptance gates

### AppleClang 17 Debug headless

Root: `/tmp/fincore-phase35-task150-apple.84c9Id`, configured from exact HEAD
`e52d288`. A fresh Ninja/Debug configure used CMake `4.0.2`, Ninja `1.12.1`,
AppleClang `17.0.0.17000603`, testing ON, and ImPlot, the demo, ASan, and UBSan
all OFF. Configure exited `0`; build exited `0` after Ninja completed
`116/116` steps. `ctest -N` registered exactly `457` tests. Focused
`ExtendedRisk|MetricsUmbrella` passed `34/34` in `0.16` seconds, and full
CTest passed `457/457` in `2.10` seconds. A case-insensitive `warning:` scan
over the configure and build logs returned `rg` exit `1`, meaning no match.

An asynchronous interim audit of this same accepted root was superseded by
the completed audit; it was not a configure, build, registration, or test
failure and is not used as separate gate evidence.

### Homebrew LLVM 22 ASan+UBSan headless

Root: `/tmp/fincore-phase35-task150-llvm.fwWXeq`, configured from exact HEAD
`e52d288`. A fresh Ninja/Debug configure used CMake `4.0.2`, Ninja `1.12.1`,
Homebrew Clang `22.1.8`, `llvm-symbolizer` `22.1.8`, testing ON, both ASan and
UBSan ON, and ImPlot and the demo OFF. Configure exited `0`; build exited `0`
after `116/116` Ninja steps; and `ctest -N` registered exactly `457` tests.

The raw execution wrapper printed these exact settings and command before
exporting/executing them:

```text
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer
ctest --test-dir /tmp/fincore-phase35-task150-llvm.fwWXeq/build --output-on-failure --parallel 4
```

The complete instrumented suite passed `457/457` in `47.31` seconds. A scan
for AddressSanitizer, UndefinedBehaviorSanitizer, LeakSanitizer, sanitizer
summaries, and `runtime error:` returned `rg` exit `1`, meaning no diagnostic.
Ninja commands contain exactly 35 address-sanitizer and 35
undefined-sanitizer flag occurrences. An exact corrected `-MT` anchor audit
shows the `risk.cpp` compilation and final `fincore_tests` link each contain
both sanitizer flags exactly once. `otool -L` finds exactly one LLVM ASan
runtime, `libclang_rt.asan_osx_dynamic.dylib`.

The build emitted exactly one warning, in upstream GoogleTest
`gtest-printers.h:524`: a `char8_t` to `char32_t`
`-Wcharacter-conversion`. The project/other-warning scan found zero matches.
An earlier overbroad scanner and audit were retained as superseded evidence;
all instrumentation counts above use the corrected exact `-MT` anchor and are
not inferred from that overbroad result.

### Local ImGui/ImPlot compile-link-only gate

Root: `/tmp/fincore-phase35-task150-implot.rpSIG7`, configured from exact HEAD
`e52d288`. A fresh CMake `4.0.2`/Ninja `1.12.1`/AppleClang
`17.0.0.17000603` Debug configure used local DataFrame, ImGui, and ImPlot
checkouts with testing, the adapter, and the demo ON. Both configure and build
exited `0`; Ninja completed `130/130` steps. The CTest list command exited `0`
and registered exactly `458` tests: the `457` headless tests plus
`ImPlotAdapterCompile` exactly once. Full CTest passed `458/458` in `2.13`
seconds; the adapter ran exactly once.

The build-root `fincore_implot_demo` had zero CTest registration and runtime
references. It is a regular, non-symlink, executable arm64 Mach-O linked to
Homebrew GLFW `3.4.0` and the system OpenGL framework. The gate never invoked
the demo, a process audit found zero instances, and no GUI launched. This is
compile/link plus adapter-test evidence only, not plotting appearance,
interaction, or runtime acceptance.

The build emitted exactly three warnings from one known family: macOS OpenGL
deprecations for `glViewport`, `glClearColor`, and `glClear` in the demo
source. The other-warning scan found zero matches.

### Invalid-ImGui negative configure

Accepted root: `/tmp/fincore-phase35-task150-invalid-imgui.fbap7R`. The
dynamic `not-a-dear-imgui-checkout` path was absent before configure and
remained absent afterwards. A fresh AppleClang 17 Ninja/Debug configure used
local DataFrame and ImPlot, testing OFF, ImPlot ON, the demo OFF, both
sanitizers OFF, and that absent dynamic path as `FINCORE_IMGUI_SOURCE_DIR`.
Configure exited exactly `1` with one CMake error at
`cmake/fincore_implot.cmake:47`.

A whitespace-tolerant multiline exact-path scanner exited `0`, and the
dynamic path appeared exactly once in the raw configure log:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-phase35-task150-invalid-imgui.fbap7R/not-a-dear-imgui-checkout'
```

The first invalid-path preflight root
`/tmp/fincore-phase35-task150-invalid-imgui.nRaPx6` is explicitly excluded:
configure never ran there. Later comparison/count-format and shell-quoting
corrections changed only evidence-wrapper logic; the accepted root's raw
configure output already matched the required multiline diagnostic, so it was
not rerun and no positive claim depends on a corrected wrapper pretending to
be a new configure.

## Dependency provenance

The accepted Apple cache points `FINCORE_DATAFRAME_SOURCE_DIR` at the local
clean cloudQuant DataFrame checkout, HEAD
`90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`, origin
`https://github.com/cloudQuant/DataFrame.git`.

GoogleTest is the actual clean official FetchContent checkout, HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`, exact tag `v1.15.2`, origin
`https://github.com/google/googletest.git`. The cache has an empty
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` override and both
`FETCHCONTENT_FULLY_DISCONNECTED` and
`FETCHCONTENT_UPDATES_DISCONNECTED` OFF. The LLVM cache independently records
the same DataFrame/GoogleTest provenance. The graphics cache records those
same dependencies plus local clean ImGui HEAD
`46d39d56febc2a00bdd2270dc88c8a13f2a0441a`, origin
`https://github.com/ocornut/imgui.git`, and local clean cloudQuant ImPlot HEAD
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`, origin
`https://github.com/cloudQuant/implot.git`. The demo resolves and links
Homebrew GLFW `3.4.0` and the system OpenGL framework. No local DataFrame,
ImGui, or ImPlot source was fetched, cloned, fabricated, or copied by these
gates.

## Exclusions

Acceptance covers only the native one-column scalar
`trading_value_at_risk` leaf and its existing metrics-facade reachability. It
adds no ignored native `period`, frequency/annualization option, multi-column,
ndarray/Series, automatic-returns or DataFrame-returning overload, alias,
CMake target, dependency, embedded Python/pandas runtime, NumPy/SciPy
component, provider, network call, solver, plot source, report, package, or GUI
runtime.

This phase does not implement `mar_ratio`, `r_cubed`, `kappa_three_ratio`,
adjusted or conditional Sharpe, calendar-resampled consecutive statistics,
EVT, market aliases, providers, reports, plots, packaging/release work, or
Python container adapters. The ImPlot evidence in this record is strictly
compile/link plus adapter-test evidence, never plotting appearance,
interaction, or GUI runtime acceptance.
