# Phase 6A slippage-adjusted returns acceptance

## Scope and traceability

This bounded acceptance covers only the native basis-point
`adjust_returns_for_slippage` return adjustment. Future market-impact,
liquidity, liquidation, and other execution-cost models remain deferred. It is
not a whole-project fincore-parity, packaging/release, or GUI-runtime claim.

- Task 38 implementation:
  `e49befe52581f719a426e28c4462b2efbfa9e130`
  (`feat: add native slippage-adjusted returns`).
- Task 38 accepted fill-value correction:
  `b87266bf20eff1e985988d1c8c1bdb443c4d4870`
  (`fix: preserve slippage fill-value semantics`).

This initial Task 39 acceptance record intentionally does not self-reference
its own commit.

## Acceptance checklist

- [x] Retain Task 38's genuine missing-declaration public API RED; do not
  remove an accepted public header to fabricate another RED.
- [x] Build a public consumer that includes only `<fincore/metrics.hpp>` and
  `<fincore/calendar.hpp>`, calls `adjust_returns_for_slippage`, and checks the
  one-column schema plus normalized Jan 2--6 UTC index.
- [x] Check the Python-oracle values `[.0999, 0, 0, 0, 0]` through that public
  umbrella consumer.
- [x] Pass a fresh AppleClang headless configure, build, and complete CTest.
- [x] Pass a separate fresh Homebrew LLVM 22 ASan+UBSan headless configure,
  build, and complete CTest without sanitizer findings.
- [x] Pass a fresh positive default-path ImGui+ImPlot+demo compile-and-link
  boundary, including registered `ImPlotAdapterCompile`, without running a GUI.
- [x] Observe the expected diagnostic from a fresh invalid
  `FINCORE_IMGUI_SOURCE_DIR` configure without obtaining or fabricating a
  dependency.
- [x] Audit changed and committed paths against Task 39 ownership.
- [x] Record compiler/tool versions, exact CTest totals, build warnings, and
  sanitizer diagnostics only from fresh completed commands.
- [x] Make no GUI execution or visual-validation claim.
- [x] Keep future market-impact, liquidity, liquidation, and other
  execution-cost models deferred.

## Accepted public contract

- `<fincore/metrics.hpp>` re-exports the accepted transaction module, so
  consumers call `adjust_returns_for_slippage` without directly including
  `metrics/transactions.hpp`. `calendar.hpp` remains the separate public UTC
  fixture helper.
- The result has exactly one `double` column with the requested return-column
  name. Its index is the sorted union of normalized return, position, and
  aggregated transaction UTC days. No input date is forward-filled.
- Return and position rows normalize to UTC midnight and must be strictly
  increasing and unique after normalization. Raw transactions retain their
  established unordered, duplicate, and intraday daily aggregation behavior.
- Portfolio value sums every genuine `double` position column while skipping
  NaN, and does not require a named cash column. The transaction penalty is
  daily `txn_volume * (slippage_bps / 10000)`.
- Adjusted PnL follows pandas one-sided `fill_value=0` addition semantics; if
  both PnL and transaction cost are missing, it stays NaN. Exact zero PnL is
  changed to a NaN denominator before division. Final fill replaces NaN only
  with `0.0`, preserving computed signed infinity.
- `slippage_bps` is ordinary IEEE input and is neither validated nor clamped.
  Phase 6A does not add market-impact, liquidity, liquidation, or other
  execution-cost modeling.

## Fresh verification

All commands used the source worktree
`/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation`,
CMake 4.0.2, Ninja 1.12.1, and the pre-existing local GoogleTest source at
`/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation/build/_deps/googletest-src`.
Each configuration set `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` to that path and
`FETCHCONTENT_FULLY_DISCONNECTED=ON`. No gate fetched, cloned, vendored,
fabricated, or substituted a dependency.

### Task 38 genuine RED and public umbrella oracle

Before the Task 38 implementation, this command completed with exit 1 during
compilation of `tests/unit/transactions_test.cpp`:

~~~sh
cmake --build build --parallel 4 && \
  ctest --test-dir build -R Transactions --output-on-failure
~~~

The compiler reported `no member named 'adjust_returns_for_slippage' in
namespace 'fincore'` at the public transaction-header call sites. Task 39 did
not remove an accepted declaration or header to manufacture a second RED.

The Task 39 `MetricsUmbrella.ExposesSlippageAdjustedReturns` consumer includes
only `<fincore/calendar.hpp>` and `<fincore/metrics.hpp>` from fincore. It
constructs the Jan 2--6 fixture, calls the new API, and verifies one `returns`
column, UTC-midnight index `[Jan 2, Jan 3, Jan 4, Jan 5, Jan 6]`, and the
Python-oracle values `[.0999, 0, 0, 0, 0]`. It is registered as test 129 in
the fresh configurations below.

### AppleClang headless core

`/usr/bin/c++ --version` reported Apple clang 17.0.0
(`clang-1700.6.3.2`), and CMake identified AppleClang
17.0.0.17000603. The final clean root was
`/tmp/fincore-task39-apple-final.2qjObB`:

~~~sh
cmake -S . -B /tmp/fincore-task39-apple-final.2qjObB/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task39-apple-final.2qjObB/build --parallel 4
ctest --test-dir /tmp/fincore-task39-apple-final.2qjObB/build \
  --output-on-failure
~~~

Configure, the full 55-step build, and CTest each exited 0. CTest passed
187/187 with zero failures. `ctest -N` listed 187 tests and no
`ImPlotAdapterCompile`, confirming the headless boundary. The configure,
build, and CTest emitted no warnings.

### Homebrew LLVM 22 ASan+UBSan headless core

Both Homebrew C and C++ compiler version commands reported clang 22.1.8; CMake
identified Clang 22.1.8. The separate clean root was
`/tmp/fincore-task39-llvm.QX8aeO`:

~~~sh
cmake -S . -B /tmp/fincore-task39-llvm.QX8aeO/build -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task39-llvm.QX8aeO/build --parallel 4
ctest --test-dir /tmp/fincore-task39-llvm.QX8aeO/build --output-on-failure
~~~

Configure, the full 55-step build, and CTest each exited 0. CTest passed
187/187 with zero failures and no AddressSanitizer or UndefinedBehaviorSanitizer
diagnostic. The build emitted exactly one third-party GoogleTest
`-Wcharacter-conversion` warning from `gtest-printers.h:524`; it is not a
sanitizer finding. No project-source warning was observed.

### Positive default ImGui, ImPlot, and demo compile boundary

The positive clean root was `/tmp/fincore-task39-implot.TyNi6H`. The command
did not override `FINCORE_IMGUI_SOURCE_DIR` or `FINCORE_IMPLOT_SOURCE_DIR`:

~~~sh
cmake -S . -B /tmp/fincore-task39-implot.TyNi6H/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task39-implot.TyNi6H/build --parallel 4
ctest --test-dir /tmp/fincore-task39-implot.TyNi6H/build -N
ctest --test-dir /tmp/fincore-task39-implot.TyNi6H/build \
  --output-on-failure --parallel 4
~~~

The cache resolved the defaults to
`/Users/yunjinqi/Documents/new_projects/imgui` and
`/Users/yunjinqi/Documents/new_projects/implot`. Configure exited 0, found
OpenGL, and the 69-step build exited 0 after linking both
`fincore_implot_demo` and `tests/fincore_tests`. `ctest -N` registered
`ImPlotAdapterCompile` as test 188, and the complete run passed 188/188 with
zero failures, including that compile contract. The demo was compiled and
linked only; no executable GUI or visual validation was run.

The build emitted exactly three project-demo warnings: macOS deprecations for
`glViewport`, `glClearColor`, and `glClear` in `implot_dashboard.cpp`. No other
warning was observed in this gate.

### Expected invalid ImGui override

The negative root was `/tmp/fincore-task39-invalid-imgui.ek7qzR`:

~~~sh
cmake -S . -B /tmp/fincore-task39-invalid-imgui.ek7qzR/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task39-invalid-imgui.ek7qzR/not-a-dear-imgui-checkout \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
~~~

Configure exited 1, as required, at `cmake/fincore_implot.cmake:47`:

~~~text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task39-invalid-imgui.ek7qzR/not-a-dear-imgui-checkout'
~~~

Configuration stopped at that validation boundary; no dependency was fetched,
cloned, vendored, fabricated, or substituted.

### Owned-path audit

Before staging, `git status --short`, `git diff --name-only`, and
`git ls-files --others --exclude-standard` identified only the three Task 39
owned paths: `tests/unit/metrics_umbrella_test.cpp`, `README.md`, and this
acceptance record. The test file's only fincore includes are the two permitted
public headers. `git diff --check` produced no output. The initial acceptance
commit stages only those explicit paths and intentionally does not include
the Task 39 evidence report under `.superpowers/sdd`.

## Deferred scope

Future market-impact, liquidity, liquidation, and other execution-cost models;
round trips and attribution; advanced analytics; visualization runtime;
packaging/release; and full Python public-API parity remain separate work.
