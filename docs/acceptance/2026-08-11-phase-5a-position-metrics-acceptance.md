# Phase 5A numeric position metrics acceptance

## Scope and traceability

This bounded acceptance adds the native numeric core of Python
`fincore.metrics.positions`: allocation, long/short exposure, concentration,
and gross leverage. It is not a transaction, string/SID, report, GUI-runtime,
or whole-project fincore-parity claim.

- Task 33 implementation: `4d32d076240e12022959d237f2baabe6682f1298`
  (`feat: add native position metrics`).

## Accepted public contract

`<fincore/metrics.hpp>` exports the bounded position surface:

- A positions DataFrame selects every genuine `double` column in the
  DataFrame's deterministic insertion order. Non-`double` metadata is
  excluded. A missing numeric selection or a malformed numeric column is an
  input error.
- `get_percent_alloc` preserves the timestamp index and selected ordered
  numeric columns. Per-row sums skip NaN, and signed-infinity allocation
  results are replaced with NaN.
- `get_long_short_pos` returns the native two-column DataFrame `long`, then
  `short`. A named cash field is optional here: if present it is excluded, and
  if absent all selected numeric columns participate. NaN holdings contribute
  zero to the side totals.
- `gross_lev` requires a genuine named `double` cash field, returns one
  `gross_lev` column, and replaces signed-infinity results with NaN.
- `get_max_median_position_concentration` requires the same genuine cash
  field and returns exactly `max_long`, `median_long`, `median_short`, then
  `max_short`. Empty eligible long or short sides are NaN.

## Acceptance checklist

- [x] A public-only `MetricsUmbrella` consumer uses only
  `<fincore/metrics.hpp>` plus the existing public date helper, creates a
  multi-column positions frame, calls all four APIs, and asserts ordered
  schemas, representative indices, numeric values, and metadata exclusion.
- [x] The genuine Task 33 public-header RED remains the pre-implementation
  evidence; no accepted public header is removed to manufacture a new RED.
- [x] A fresh AppleClang headless configure, build, and full CTest run passes.
- [x] A separate fresh Homebrew LLVM 22 ASan+UBSan headless configure, build,
  and full CTest run passes without sanitizer diagnostics.
- [x] The default local ImGui/ImPlot adapter/demo compile-and-link CTest
  boundary passes without launching a GUI.
- [x] A fresh invalid `FINCORE_IMGUI_SOURCE_DIR` configure fails with the
  intended CMake diagnostic and does not fetch, clone, vendor, fabricate, or
  substitute dependencies.
- [x] Compiler versions, test counts, warnings, diagnostics, owned-path audit,
  and remaining scope are filled from completed evidence only.

## Fresh verification

All commands used CMake `4.0.2`, Ninja `1.12.1`, and source worktree
`/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation`.

### AppleClang headless core

Fresh root: `/tmp/fincore-task34-core.ax0dZ9`. `/usr/bin/c++ --version`
reported Apple clang `17.0.0` (`clang-1700.6.3.2`); CMake identified
`AppleClang 17.0.0.17000603` and used `/usr/bin/c++`.

~~~sh
cmake -S . -B /tmp/fincore-task34-core.ax0dZ9/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task34-core.ax0dZ9/build --parallel 4
ctest --test-dir /tmp/fincore-task34-core.ax0dZ9/build -N
ctest --test-dir /tmp/fincore-task34-core.ax0dZ9/build --output-on-failure
~~~

Every command exited `0`. `ctest -N` reported `Total Tests: 166`; the full
run passed `166/166` with `0` failures. The captured build output contained no
compiler warning diagnostic.

### Homebrew LLVM 22 ASan+UBSan headless core

Fresh root: `/tmp/fincore-task34-sanitize.n2Do46`.
`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake identified `Clang 22.1.8` and used Homebrew LLVM for both C
and C++. The configuration enables both ASan and UBSan.

~~~sh
cmake -S . -B /tmp/fincore-task34-sanitize.n2Do46/build -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
cmake --build /tmp/fincore-task34-sanitize.n2Do46/build --parallel 4
ctest --test-dir /tmp/fincore-task34-sanitize.n2Do46/build -N
ctest --test-dir /tmp/fincore-task34-sanitize.n2Do46/build --output-on-failure
~~~

Every command exited `0`. `ctest -N` reported `Total Tests: 166`; the full
run passed `166/166` with `0` failures. The build emitted one third-party
GoogleTest `-Wcharacter-conversion` warning at `gtest-printers.h:524` for an
implicit `char8_t` to `char32_t` conversion. It is neither a fincore warning
nor a sanitizer finding. No `AddressSanitizer`, `UndefinedBehaviorSanitizer`,
or `runtime error` diagnostic appeared in the completed build or CTest run.

### Default local ImGui/ImPlot adapter and demo compile boundary

Fresh root: `/tmp/fincore-task34-implot.qZ2lwQ`. This AppleClang configuration
intentionally passed neither `FINCORE_IMGUI_SOURCE_DIR` nor
`FINCORE_IMPLOT_SOURCE_DIR`; its generated cache resolved both supplied local
checkouts:

~~~text
FINCORE_IMGUI_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/imgui
FINCORE_IMPLOT_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/implot
~~~

~~~sh
cmake -S . -B /tmp/fincore-task34-implot.qZ2lwQ/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-task34-implot.qZ2lwQ/build --parallel 4
ctest --test-dir /tmp/fincore-task34-implot.qZ2lwQ/build -N
ctest --test-dir /tmp/fincore-task34-implot.qZ2lwQ/build \
  --output-on-failure --parallel 4
~~~

Every command exited `0`. The build linked `fincore_implot_demo`; `ctest -N`
listed `ImPlotAdapterCompile` as test `#167`, and the full run passed `167/167`
with `0` failures. The build emitted exactly three macOS OpenGL deprecation
warnings for `glViewport`, `glClearColor`, and `glClear`. No GUI executable was
launched: this is a compile/link plus headless CTest boundary, not an
interactive visual claim.

### Explicit invalid ImGui override

Fresh root: `/tmp/fincore-task34-invalid-imgui.xY5tC7`.

~~~sh
cmake -S . -B /tmp/fincore-task34-invalid-imgui.xY5tC7/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task34-invalid-imgui.xY5tC7/not-a-dear-imgui-checkout
~~~

The configure exited `1` at `cmake/fincore_implot.cmake:47` with:

~~~text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task34-invalid-imgui.xY5tC7/not-a-dear-imgui-checkout'
~~~

This expected negative diagnostic is independent from the successful default
path build. The verification commands did not fetch, clone, vendor, fabricate,
or substitute a graphics dependency; the negative configure stopped at the
ImGui source validation.

## Owned-path audit

Task 34 modifies only `include/fincore/metrics.hpp`,
`tests/unit/metrics_umbrella_test.cpp`, `README.md`, and this acceptance
record. `git diff --check` completed with no output before staging.

## Deferred scope

Top holdings; string/SID schemas; position extraction and stacking;
sector/style/cap/volume mappings; transactions, liquidation, round trips, and
reports; performance attribution; bootstrap/distribution statistics; Bayesian
and advanced-risk models; optimization; simulation; reports/tearsheets; a full
native facade; packaging/release; and interactive ImPlot visual smoke testing
remain separate work. This acceptance is not full fincore parity or project
completion.
