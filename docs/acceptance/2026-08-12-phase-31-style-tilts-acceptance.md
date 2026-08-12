# Phase 31 native rolling style tilts acceptance

## Candidate and exact scope

This record accepts Plan A: a pure native C++23 implementation over the local
cloudQuant DataFrame, without an embedded Python/pandas runtime. The accepted
Task 133 implementation is `9c4f81e` (`feat: add native rolling style tilts`),
whose direct parent is the reviewed Task 132 correction `17838e0`. Its exact
five-path scope is:

- `include/fincore/attribution/style.hpp`
- `src/attribution/style.cpp`
- `tests/CMakeLists.txt`
- `tests/unit/style_tilts_test.cpp`
- `tests/unit/attribution_style_tilts_umbrella_test.cpp`

The Phase 31 design/plan commit is `1519997`; `17838e0` is its one-file
design-wording correction. Independent Task 133 specification and
task-quality reviews both returned **PASS**, with Critical/Important/Minor
`0/0/0`; there was no implementation review-fix commit.

Task 134 changes only `README.md` and this acceptance record. These two files
remained unstaged and uncommitted while the required independent Task 134
document/gate review ran. That review returned **Spec PASS / Task-Quality
PASS**, with Critical/Important/Minor `0/0/0`; the two-file candidate was then
accepted without a review-fix commit.

## Exact public API and source mapping

The existing `<fincore/attribution.hpp>` façade already includes the style
leaf, which now declares exactly:

```cpp
[[nodiscard]] dataframe calculate_style_tilts(
    const dataframe& asset_returns,
    std::size_t window = 252U);
```

Python's `factor_returns` parameter is never read; native code therefore does
not expose a misleading dead argument. Compile-time public tests prove the
default call and reject both two- and three-argument factor-DataFrame call
expressions.

For `T` rows, nonempty work uses
`effective_window = min(window,T-1)`. Output row position corresponding to
input `t` uses only `[0,t)`, never the current row. Its raw index is an owned
copy of `[effective_window,T)`, including duplicate or nonmonotonic labels.
Unlike Python's pre-created frame plus `.loc[label]` writes, native output is
written positionally, so duplicate labels never broadcast or overwrite other
positions.

Columns are genuine doubles in exact Python insertion order:

1. all asset `{asset}_large,{asset}_small` pairs;
2. all `{asset}_winner,{asset}_loser` pairs;
3. all `{asset}_value,{asset}_growth` pairs; and
4. all `{asset}_high_vol,{asset}_low_vol` pairs.

There are exactly `8*N` columns and no hidden identity or metadata column.

## Formula, empty, validation, and ownership boundary

For every excluded-history position:

- size average-ranks the latest earlier row, ascending and one-based with
  exact ties averaged; `large` is rank `<=N/3`, `small` is rank `>2N/3`;
- momentum compounds the last `min(window,t)` earlier returns with checked
  `product(1+r)-1`; `winner` is strictly positive and zero is `loser`;
- value/growth average-ranks that cumulative return; `value` is rank `<=N/2`
  and `growth` is rank `>N/2`; and
- volatility uses the last `min(60,t)` earlier values, checked Welford sample
  standard deviation (`ddof=1`), and the linear median across assets; equality
  belongs to `high_vol`.

Finite equal-length histories make `ddof=0` and `ddof=1` a common positive
cross-sectional rescaling, so the binary public output cannot distinguish
that mutant. The exact `ddof=1` contract is closed by source-derived design,
the checked implementation formula, and both independent reviews rather than
an overstated binary golden.

An empty-shaped input, `window<2`, or fewer than three observations returns a
fresh canonical `0x0` DataFrame before nonempty validation. Otherwise the
frame must have only exact-index-length, finite genuine-double columns with
nonempty, embedded-NUL-free, distinct asset names. All eight derived names
must fit cloudQuant DataFrame's fixed name storage with strict room for the
terminator and be distinct before loading. Counts, cells, index copies,
products, variance intermediates, medians, and allocations are checked;
public failures become `fincore::input_error`. The result retains no input
views and survives source mutation and destruction.

## Mandatory Python source oracle

The plan command ran from `/Users/yunjinqi/Documents/new_projects/fincore`
through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python` and exited
`0`. For raw index `[42,-7,100,3,55]`, `window=3`, and:

```text
A = [0, .25, -.5, 0, .75]
B = [0, 0, -.5, -.5, .75]
C = [0, -.5, 1, .25, .75]
```

Python returned index `[3,55]`, columns:

```text
A_large,A_small,B_large,B_small,C_large,C_small,
A_winner,A_loser,B_winner,B_loser,C_winner,C_loser,
A_value,A_growth,B_value,B_growth,C_value,C_growth,
A_high_vol,A_low_vol,B_high_vol,B_low_vol,C_high_vol,C_low_vol
```

and rows:

```text
[0,0,0,0,0,1, 0,1,0,1,0,1, 0,1,1,0,0,1, 1,0,0,1,1,0]
[0,0,1,0,0,1, 0,1,0,1,1,0, 0,1,1,0,0,1, 1,0,0,1,1,0]
```

It also returned `(0,0)` for `window=0`, `window=1`, a two-row history, and
an empty frame. The complete retained record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-31-style-tilts/source-oracle.md`.

## Genuine RED, GREEN, and independent reviews

Both new consumers and their CMake registrations existed before the
production declaration. The leaf consumer included only
`<fincore/attribution/style.hpp>` among fincore headers; the façade consumer
included only `<fincore/attribution.hpp>`. `cmake --build build --parallel 4`
exited `1`; both failed only because `fincore::calculate_style_tilts` did not
exist. No CTest ran after the failed build, so no stale binary is cited.

Final Task 133 GREEN was focused `12/12`, related `61/61`, and full headless
CTest `419/419`; diff checks were clean. The initial focused implementation
run exposed four incorrectly transcribed new volatility expectations. Direct
Python output and C++ agreed; only the new test fixture was corrected. The
independent specification review and the subsequent independent code/task-
quality review each concluded PASS with C/I/M `0/0/0`.

## Task 134 current and fresh gates

Raw current logs are under
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-31-style-tilts/task-134-logs/current/`.
The fresh roots below retain their configure, build, registration, CTest, and
scanner logs.

### Current configured tree

- `cmake --build build --parallel 4`: exit `0`, no work.
- `ctest --test-dir build -N`: exactly `419` tests.
- focused `StyleTilts|AttributionStyleTiltsUmbrella`: `12/12`, exit `0`.
- full CTest: `419/419`, exit `0`.
- `git diff --check`: exit `0` before the documentation edit.

### Fresh AppleClang 17 Debug headless

Root: `/tmp/fincore-phase31-task134-apple.2RI7NN`. The explicit configure used
Ninja, `/usr/bin/c++`, Debug, the local DataFrame, testing ON, ImPlot/demo OFF,
and ASan/UBSan OFF. Configure/build exited `0` (`114/114`); `ctest -N`
registered exactly `419`; focused passed `12/12`; full passed `419/419`.
Scanning configure/build logs found no warning.

### Fresh Homebrew LLVM 22 ASan+UBSan headless

Root: `/tmp/fincore-phase31-task134-llvm.TG7hgc`. The explicit configure used
`/opt/homebrew/opt/llvm/bin/clang++` 22.1.8, Debug, Ninja, local DataFrame,
testing ON, ImPlot/demo OFF, and both sanitizer options ON. Configure/build
exited `0`; `ctest -N` registered exactly `419`; the full run with ASan/UBSan
environment and LLVM symbolizer passed `419/419`.

The raw CTest scan found no AddressSanitizer, UndefinedBehaviorSanitizer,
LeakSanitizer, sanitizer-summary, or runtime-error diagnostic. Compile commands
contain 34 address and 34 undefined flags; the final binary loads LLVM 22's
`libclang_rt.asan_osx_dynamic.dylib`. The sole build warning is upstream
GoogleTest `gtest-printers.h:524` `char8_t` to `char32_t`
`-Wcharacter-conversion`, not fincore or a sanitizer finding.

### Fresh local ImGui/ImPlot compile-link gate

Root: `/tmp/fincore-phase31-task134-implot.n7rMzI`. AppleClang 17 Debug/Ninja
used testing, ImPlot, and demo all ON, plus the local DataFrame, ImGui, and
ImPlot paths. Configure/build exited `0` (`128/128`); `ctest -N` registered
exactly `420`, including `#420 ImPlotAdapterCompile`; full CTest passed
`420/420`.

The build-root `fincore_implot_demo` is a non-symlink regular executable,
Mach-O arm64, but it is not CTest-registered and was never invoked. No GUI was
launched. The only warnings are the three known macOS OpenGL deprecations in
`examples/implot_dashboard.cpp`: `glViewport`, `glClearColor`, and `glClear`.

### Fresh invalid-ImGui negative configure

Root: `/tmp/fincore-phase31-task134-invalid-imgui.QFvFl9`. The explicit
nonexistent `FINCORE_IMGUI_SOURCE_DIR` precondition passed; configure with
ImPlot ON and testing OFF exited exactly `1` at
`cmake/fincore_implot.cmake:47`. A multiline whitespace-tolerant
`rg -U -P` pattern using `[[:space:]]+` matched the complete
`FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received ...` diagnostic and exact missing path, exit `0`.

## Dependency provenance and exclusions

Every fresh cache uses local DataFrame
`/Users/yunjinqi/Documents/new_projects/DataFrame`, HEAD
`90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`, origin
`https://github.com/cloudQuant/DataFrame.git`. GoogleTest is the official
FetchContent checkout `https://github.com/google/googletest.git`, HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`, exact tag `v1.15.2`; it is not a
local dependency. The graphics gate used local ImGui HEAD
`46d39d56febc2a00bdd2270dc88c8a13f2a0441a` and local ImPlot HEAD
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`.

Acceptance covers only deterministic native rolling style tilts and its
public façade. It does not add regression attribution, factor fetching or a
provider, Python runtime embedding, SciPy or another solver, plots, reports,
packaging/release work, or GUI runtime behavior. ImPlot evidence is
compile/link and adapter-test evidence only.
