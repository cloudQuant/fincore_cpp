# Phase 29 native Style Analysis core acceptance

## Accepted leaf chain and scope

Task 125 begins at implementation commit
`9ff78501763a9d4090005df018002928aa4fb54f`
(`feat: add native style analysis core`). Its exact five-path scope is:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `include/fincore/attribution/style.hpp`
- `src/attribution/style.cpp`
- `tests/unit/style_analysis_test.cpp`

The independent specification review returned **PASS** with no
Critical/Important/Minor finding. The initial quality review found two Minor
test-coverage gaps—not a production defect—around defaults and rolling-window
truncation. Direct follow-up
`9b7afab1efbacda8e2108ee26096e467a124f6bf`
(`test: harden style analysis defaults and volatility`) changes only
`tests/unit/style_analysis_test.cpp`; its independent re-review returned
**Task-Quality PASS**, Critical/Important/Minor `0/0/0`.

Task 126 adds no style implementation or numerical change. Its tracked scope
is limited to README, the attribution façade, CTest registration, a façade-only
consumer, and this acceptance record. This document intentionally does not
name or anticipate Task 126's SHA; Task 127 records that trace later.

## Public contract and pandas-to-DataFrame mapping

`<fincore/attribution.hpp>` gains exactly one production façade export:

```cpp
#include <fincore/attribution/style.hpp>
```

It exposes the already accepted leaf:

```cpp
struct style_analysis_options {
    std::span<const double> market_caps{};
    std::span<const double> book_to_price{};
    std::span<const double> value_scores{};
    std::size_t momentum_window{252U};
    std::array<double, 2U> size_quantiles{.5, .5};
};

struct style_analysis_result {
    dataframe exposures;
    dataframe returns_by_style;
    dataframe overall_returns;
};

[[nodiscard]] style_analysis_result style_analysis(
    const dataframe& asset_returns,
    const style_analysis_options& options = {});
```

Python accepts a time-by-asset pandas DataFrame and labeled optional Series.
The native input is likewise time-by-asset; every genuine `double` column is
an asset in the stable DataFrame column order. Because the native DataFrame
index is timestamp-valued rather than string-valued, optional market caps,
book-to-price, and value scores intentionally map positionally to that asset
order. A provided span must have exactly one finite value per asset, and value
scores take precedence over book-to-price.

The three outputs are freshly materialized and value-owning:

- `exposures`: ordinal timestamp index `0..asset_count-1`, genuine string
  `asset`, then ordered genuine-double style weights;
- `returns_by_style`: ordinal index `0..style_count-1`, genuine string `style`,
  and genuine double `return`;
- `overall_returns`: exact copied raw input index and one genuine double
  `return` column.

When caps are supplied, style order is `large`, `mid`, `small`, `winner`,
`loser`, `value`, `growth`, `high`, `low`. Without caps, `equal_weight`
replaces the three size styles. Size masks use the two linear percentiles;
momentum compounds the last `min(momentum_window,row_count)` observations;
value uses a cross-sectional median; volatility uses trailing width at most
`60`, minimum `20`, population (`ddof=0`) standard deviation. All nonnegative
masks normalize to one when possible.

A short history has no defined rolling volatility. Its `high` and `low`
weights therefore remain zero and its corresponding style summary returns are
source-compatible `NaN`. This is the only accepted generated nonfinite output.
Native input returns and optional characteristics must be finite; empty,
mixed/non-double, malformed, invalid-name, mismatched-span, invalid-quantile,
nonfinite-arithmetic, allocation, and representability boundaries raise
`fincore::input_error`.

## Python source oracle and leaf evidence

Task 125 ran the exact plan oracle from
`/Users/yunjinqi/Documents/new_projects/fincore` with the mandated Anaconda
interpreter:

```sh
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '...'
```

The raw output is retained in
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-29-style-analysis-core/python-oracle.log`.
For the 60-row `ALPHA/BETA/GAMMA` fixture, caps `{100,40,10}`, value scores
`{.2,.8,.5}`, momentum `5`, and quantiles `{.3,.7}`, Python yields nine styles
in the order above. Representative exact results include `large={1,0,0}`,
`winner={0,.5,.5}`, large summary return `.0006666666666666668`, and low
summary return `.0007083333333333334`. Native tests assert every exposure and
summary value and all 60 equal-weight overall returns.

The leaf-only public consumer was registered before
`<fincore/attribution/style.hpp>` existed. Its build exited `1` solely on that
missing header and no CTest was run afterward. Initial GREEN passed Style
Analysis `6/6`, related regression `36/36`, and full CTest `394/394`.
Test-only hardening then locked the omitted defaults, tied size boundary, and
the 61-row/60-window volatility distinction. Final Task 125 evidence is Style
Analysis `8/8`, related `38/38`, and full CTest `396/396`.

## Façade-only RED/GREEN

`tests/unit/attribution_style_analysis_umbrella_test.cpp` includes only:

```cpp
#include <fincore/attribution.hpp>
```

Before the façade leaf include was added, `cmake --build build --parallel 4`
exited `1` solely because `style_analysis_result`, `style_analysis_options`,
`style_analysis`, and the Style Analysis constants were not visible through
the façade. To preserve raw rather than narrative-only evidence, the final
consumer was also registered in an isolated detached worktree at parent
`9b7afab1efbacda8e2108ee26096e467a124f6bf`, with the façade export still
absent. Fresh configure exited `0`; `cmake --build ... --parallel 4` exited
`1` at that consumer with only missing Style Analysis symbols. Its complete
compiler tee is retained at
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-29-style-analysis-core/task-126-red-build.raw.log`
with status `1`. No CTest was run after either failed build.

After the one-line export, the façade consumer asserts the exact function
pointer and one-argument default-call types, constants, ordinal indexes,
original asset names, exact total column counts/types/order, representative
source exposure and summary endpoints, raw duplicate/nonmonotonic index
retention, and the no-caps default surface. Current raw logs are retained at
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-29-style-analysis-core/task-126-logs/`:

| Command | Result |
| --- | --- |
| `cmake --build build --parallel 4` | exit `0` |
| `ctest --test-dir build -R 'StyleAnalysis|AttributionStyleAnalysisUmbrella' --output-on-failure` | `9/9`, exit `0` |
| `ctest --test-dir build --output-on-failure` | `397/397`, exit `0` |
| `git diff --check` | exit `0` |

## Fresh compiler and configuration gates

Every gate below used an independent `/tmp` root and did not launch the GUI.

### AppleClang Debug, headless

Raw root: `/tmp/fincore-phase29-task126-apple-gates.FiWQcR`.

```sh
/usr/bin/env CC=/usr/bin/clang CXX=/usr/bin/clang++ cmake \
  -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation \
  -B /tmp/fincore-phase29-task126-apple-gates.FiWQcR/apple-debug-headless \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame
cmake --build \
  /tmp/fincore-phase29-task126-apple-gates.FiWQcR/apple-debug-headless \
  --parallel 4
ctest --test-dir \
  /tmp/fincore-phase29-task126-apple-gates.FiWQcR/apple-debug-headless -N
ctest --test-dir \
  /tmp/fincore-phase29-task126-apple-gates.FiWQcR/apple-debug-headless \
  --output-on-failure
```

AppleClang was `17.0.0.17000603` (`/usr/bin/clang++` reports Apple clang
`17.0.0 (clang-1700.6.3.2)`). Configure and build exited `0`; `ctest -N`
registered exactly `397` tests and full CTest passed `397/397`, all exit `0`.
The focused Style Analysis set contains `9` tests. The captured configure,
build, registration, and CTest logs contain no warning match. ImPlot was OFF,
`ImPlotAdapterCompile` was absent, and no GUI target was run.

### Homebrew LLVM 22 Debug with ASan and UBSan

Raw root: `/tmp/fincore-phase29-task126-llvm.f0D5Mb`.

```sh
cmake \
  -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation \
  -B /tmp/fincore-phase29-task126-llvm.f0D5Mb/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_BUILD_IMPLOT_DEMO=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
cmake --build /tmp/fincore-phase29-task126-llvm.f0D5Mb/build \
  --parallel 4
ctest --test-dir /tmp/fincore-phase29-task126-llvm.f0D5Mb/build -N
env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer \
  ctest --test-dir /tmp/fincore-phase29-task126-llvm.f0D5Mb/build \
  --output-on-failure --parallel 4
```

Homebrew Clang was `22.1.8`. Configure and the `110/110` build steps exited
`0`; `ctest -N` registered exactly `397` tests, and full CTest passed
`397/397` in `37.76s`, all exit `0`. Compile commands contain both
`-fsanitize=address` and `-fsanitize=undefined`; the final test link contains
both flags, and `otool -L` shows the LLVM 22 ASan runtime. The retained runtime
also exports the referenced UBSan handlers. Scanning the full CTest log for
ASan, UBSan, LeakSanitizer, `runtime error`, and sanitizer summaries found no
match. The only warning is the upstream GoogleTest
`gtest-printers.h:524` `char8_t`-to-`char32_t`
`-Wcharacter-conversion` warning; it is neither fincore code nor a sanitizer
diagnostic. SHA-256 values for `configure.log`, `build.log`, `ctest-N.log`, and
`ctest.log` are respectively
`cee04031e2e0925174fd49c965214e1a91af879a770dea80cee23e0afcadb349`,
`502da01204e974a64da8bec85d9b155eb4bcd5c411e9442ae461da7ab8699db9`,
`a3117ad6d59f9e5e03cf2d6cb0d4d65f341ac76a34a59e9e48a44a582056e4d7`,
and `21f8961cc584ae9a8569b127f437ec67a5989dfcacedbff5eeb13ce698d3b612`.

### Local ImGui/ImPlot compile and link only

Raw root: `/tmp/fincore-phase29-task126-implot.e6oDwV`.

```sh
cmake \
  -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation \
  -B /tmp/fincore-phase29-task126-implot.e6oDwV/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_ENABLE_ASAN=OFF -DFINCORE_ENABLE_UBSAN=OFF
cmake --build /tmp/fincore-phase29-task126-implot.e6oDwV/build \
  --parallel 4
ctest --test-dir /tmp/fincore-phase29-task126-implot.e6oDwV/build -N
ctest --test-dir /tmp/fincore-phase29-task126-implot.e6oDwV/build \
  --output-on-failure --parallel 4
test -f /tmp/fincore-phase29-task126-implot.e6oDwV/build/fincore_implot_demo
test -x /tmp/fincore-phase29-task126-implot.e6oDwV/build/fincore_implot_demo
```

AppleClang was `17.0.0`. Configure and the `124/124` build steps exited `0`;
`ctest -N` registered exactly `398` tests, including
`#398 ImPlotAdapterCompile`, and full CTest passed `398/398`, all exit `0`.
The cache and compile commands prove that the local ImGui and ImPlot checkouts
above supplied their core and backend sources. The build-root
`fincore_implot_demo` is an executable arm64 Mach-O file, but neither CTest nor
this gate invoked it, so no GUI was launched. The only build warnings are the
three known macOS OpenGL deprecations for `glViewport`, `glClearColor`, and
`glClear` in `examples/implot_dashboard.cpp`. SHA-256 values for
`configure.log`, `build.log`, `ctest-N.log`, and `ctest.log` are respectively
`3c3a9887bf4d37d01f3c15ebd1368114549fb560c02020c95db22387ed66746f`,
`c0350aa3faa92c4c5c01755eecd1992a9e41fc699b59162fda27c80a3e5a4a86`,
`6dd0657d5152148e19f134f77b2ee027d6f2afc0e8aa064393e249f99da00fe8`,
and `6dd7b912753a127cf8e0cd49d5f24a1da849eeabd2c81dd0d7e7ce3d5e787942`.

### Invalid ImGui negative configuration

Raw root: `/tmp/fincore-phase29-task126-apple-gates.FiWQcR`.

```sh
/usr/bin/env CC=/usr/bin/clang CXX=/usr/bin/clang++ cmake \
  -S /Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation \
  -B /tmp/fincore-phase29-task126-apple-gates.FiWQcR/invalid-imgui-configure \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase29-task126-apple-gates.FiWQcR/explicitly-nonexistent-imgui-checkout
rg -U --pcre2 \
  'FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all\s+required\s+files; received\s+' \
  /tmp/fincore-phase29-task126-apple-gates.FiWQcR/invalid-imgui.configure.log
```

Configure exited `1`, as required. The byte-accurate, single-backslash,
whitespace-tolerant multiline PCRE exited `0` and matched the complete
`FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received ...` diagnostic. No production CMake file was changed.

## GoogleTest FetchContent provenance

The test build declares GoogleTest through FetchContent at the official
`https://github.com/google/googletest.git` remote with tag `v1.15.2`.
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` was empty and
`FETCHCONTENT_FULLY_DISCONNECTED=OFF`, so this is not a local source override.
The successful LLVM gate's actual `_deps/googletest-src` checkout has HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`, exactly tagged `v1.15.2`;
`build.ninja` compiles that checkout and links its generated `gtest_main` and
`gtest` libraries. DataFrame remains the explicit local checkout at
`/Users/yunjinqi/Documents/new_projects/DataFrame` (HEAD
`90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`, cloudQuant origin), while the
positive graphics gate uses the explicit local ImGui and ImPlot paths. The
local ImPlot checkout is HEAD
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4` with cloudQuant origin.

## Independent review and exclusions

Fresh independent read-only review concluded **Spec PASS / Task-Quality
PASS**, with Critical/Important/Minor `0/0/0`. The exact five-path Task 126
candidate matches the Phase 29 plan and design: it adds only the one-leaf
attribution façade export, registers a façade-only consumer, and accurately
records scope, contract, gates, provenance, and exclusions. The repaired
detached-parent RED evidence is genuine: configure exited `0`, the final
consumer build exited `1` only on Style Analysis symbols absent from the
façade, and no CTest followed the failed build. Current and fresh compiler,
sanitizer, ImPlot compile/link-only, invalid-ImGui, and FetchContent evidence
support the documented claims.

This acceptance is limited to deterministic `style_analysis` and its
attribution-façade export. It does not add `calculate_style_tilts`,
`calculate_regression_attribution`, `analyze_performance_by_style`,
`fetch_style_factors`, remote providers, SciPy optimization, reports,
packaging/release work, full pandas display parity, or GUI runtime validation.
ImPlot evidence is compile/link and CTest registration only; the demo is never
executed.
