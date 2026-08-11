# Phase 6B quadratic volume-share slippage penalty acceptance

## Scope and traceability

This bounded acceptance covers only the native deterministic quadratic
volume-share penalty for a prejoined transaction/bar-volume input. It does not
accept market-data joins, symbols/SIDs, liquidity or liquidation metrics,
other impact models, packaging/release, or a GUI runtime.

- Task 41 implementation:
  `da9387aa80a2586753ad51b4cbb45a8670500829`
  (`feat: add quadratic slippage penalty`).
- Task 41 signed-simulation coverage:
  `2c15648f26e7263a08ccfe7bc16e890f38c646d1`
  (`test: cover signed quadratic slippage`).

This initial Task 42 acceptance record intentionally does not self-reference
its own commit.

## Acceptance checklist

- [x] Retain Task 41's genuine missing-declaration public API RED; do not
  remove an accepted public header to fabricate another RED.
- [x] Build a public-only consumer that includes only `<fincore/metrics.hpp>`
  and `<fincore/calendar.hpp>`, calls `apply_slippage_penalty`, and checks the
  normalized three-day capacity fixture and one-column schema.
- [x] Check the Python-oracle values
  `[.9995, .9999375, .9999861111111111]` through that public consumer.
- [x] Pass a fresh AppleClang headless configure, build, and complete CTest.
- [x] Pass a separate fresh Homebrew LLVM 22 ASan+UBSan headless configure,
  build, and complete CTest without sanitizer findings.
- [x] Pass a fresh positive default-path ImGui+ImPlot+demo compile-and-link
  boundary, including registered `ImPlotAdapterCompile`, without running a GUI.
- [x] Observe the expected diagnostic from a fresh invalid
  `FINCORE_IMGUI_SOURCE_DIR` configure without obtaining or fabricating a
  dependency.
- [x] Audit changed and committed paths against Task 42 ownership.
- [x] Record compiler/tool versions, exact CTest totals, build warnings, and
  sanitizer diagnostics only from fresh completed commands.
- [x] Make no GUI execution or visual-validation claim.
- [x] Keep daily transaction/bar-data joins, market-data integration, and
  other deferred execution work out of this acceptance.

## Accepted public contract

- `<fincore/metrics.hpp>` re-exports the accepted transaction header. The
  Task 42 consumer has only `<fincore/calendar.hpp>` and
  `<fincore/metrics.hpp>` as fincore includes; it does not include
  `transactions.hpp` directly.
- Raw transaction/bar-data rows require distinct genuine `double` `amount`,
  `price`, and `volume` fields. They can be unordered, duplicate, and intraday
  because their quadratic penalties aggregate by UTC day. There is no
  transaction-to-market-data join in this API.
- For `m = simulate_starting_capital / backtest_starting_capital`, a row's
  penalty is
  `((abs(m * amount) / volume)^2) * impact * (price * abs(m * amount))`.
  NaN row contributions are skipped; signed infinities otherwise use ordinary
  IEEE arithmetic.
- The return input requires a requested genuine `double` column and strictly
  increasing unique normalized UTC days. The result is exactly one genuine
  `double` column with that requested name, indexed only by those normalized
  return days. Transaction-only days are not emitted.
- Wealth compounds a NaN return as zero only on the wealth path. Daily absent
  or NaN penalties become zero. The portfolio value is
  `(wealth * backtest_starting_capital) * m`; an exact zero becomes a NaN
  denominator, so a NaN adjusted result recovers the original return. Exact
  zero `backtest_starting_capital` is rejected; simulated capital and impact
  remain ordinary IEEE inputs.

## Fresh verification

All fresh commands used the source worktree
`/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation`,
CMake 4.0.2, Ninja 1.12.1, and the pre-existing local GoogleTest source at
`/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation/build/_deps/googletest-src`.
Each configuration supplied that exact path as
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` with `FETCHCONTENT_FULLY_DISCONNECTED=ON`.
No gate fetched, cloned, vendored, fabricated, or substituted a dependency.

### Task 41 genuine RED and Python oracle

Task 41's completed public-header RED was retained as prior evidence; Task 42
did not remove any accepted public declaration or header to invent another.
Before Task 41 added the API, its `cmake --build build --parallel 4` exited 1
while compiling `tests/unit/transactions_test.cpp` and reported both missing
`fincore::transaction_bar_volume_column` and missing
`fincore::apply_slippage_penalty` declarations. Its subsequent independent
Anaconda oracle used the mandated interpreter
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python` and completed four
cases within absolute error `1e-15`, including the capacity vector:

~~~text
equal-impact-1 [0.9995, 0.9999375, 0.9999861111111111]
scaled-impact-1 [0.95, 0.99375, 0.9986111111111111]
equal-impact-.1 [0.99995, 0.99999375, 0.9999986111111111]
unordered-duplicate [0.09981818181818182, 0.1999810606060606, nan, -1.0, 0.3]
ORACLE_OK cases=4
~~~

The fresh Task 42 public-only consumer is
`MetricsUmbrella.ExposesQuadraticVolumeShareSlippagePenalty`, registered as
test 130 in each headless run and test 130 of the ImPlot run. It constructs
returns `[1, 1, 1]` and prejoined amount/price/volume rows
`[100000, 100000, 100000]`, `[1, 1, 1]`, and
`[1000000, 2000000, 3000000]` on Jan 1--3 with equal 1000000 capital and
impact 1. It checks the normalized three-day UTC index, `shape().second == 1`,
one genuine `double` `returns` column, and exactly
`[.9995, .9999375, .9999861111111111]`.

### AppleClang headless core

`/usr/bin/c++ --version` reported Apple clang 17.0.0
(`clang-1700.6.3.2`), and CMake identified AppleClang 17.0.0.17000603. The
fresh root was `/tmp/fincore-task42-apple.3MHOex`:

~~~sh
cmake -S . -B /tmp/fincore-task42-apple.3MHOex/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task42-apple.3MHOex/build --parallel 4
ctest --test-dir /tmp/fincore-task42-apple.3MHOex/build --output-on-failure
~~~

Configure and the 55-step build exited 0 with no warnings. Complete CTest
passed 196/196 with zero failures. `ctest -N` contained no
`ImPlotAdapterCompile`, confirming the headless boundary.

### Homebrew LLVM 22 ASan+UBSan headless core

Both `/opt/homebrew/opt/llvm/bin/clang` and `clang++` reported Homebrew clang
22.1.8, and CMake identified Clang 22.1.8. The separate fresh root was
`/tmp/fincore-task42-llvm.bLq1ru`:

~~~sh
cmake -S . -B /tmp/fincore-task42-llvm.bLq1ru/build -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task42-llvm.bLq1ru/build --parallel 4
ctest --test-dir /tmp/fincore-task42-llvm.bLq1ru/build --output-on-failure
~~~

Configure and the fresh-root build completed successfully. CTest passed
196/196 with zero failures and no AddressSanitizer or
UndefinedBehaviorSanitizer diagnostic. The build emitted exactly one warning:
third-party GoogleTest `-Wcharacter-conversion` at `gtest-printers.h:524`.
No project-source warning was observed.

### Positive default ImGui, ImPlot, and demo compile boundary

The fresh positive root was `/tmp/fincore-task42-implot.UiSQYq`; no ImGui or
ImPlot source-path override was passed. Its cache resolved the defaults to
`/Users/yunjinqi/Documents/new_projects/imgui` and
`/Users/yunjinqi/Documents/new_projects/implot`:

~~~sh
cmake -S . -B /tmp/fincore-task42-implot.UiSQYq/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task42-implot.UiSQYq/build --parallel 4
ctest --test-dir /tmp/fincore-task42-implot.UiSQYq/build \
  --output-on-failure --parallel 4
~~~

Configure found OpenGL and exited 0. The 69-step build linked both
`fincore_implot_demo` and `tests/fincore_tests`. `ctest -N` registered
`ImPlotAdapterCompile` as test 197; the complete run passed 197/197 with zero
failures, including that compile contract. The demo was compiled and linked
only: no GUI executable was launched and no visual validation was performed.

The build emitted exactly three macOS OpenGL deprecation warnings in
`examples/implot_dashboard.cpp`: `glViewport`, `glClearColor`, and `glClear`.
No other warning was observed in this gate.

### Expected invalid ImGui override

The independent fresh negative root was
`/tmp/fincore-task42-invalid-imgui-confirm.DLp0yh`:

~~~sh
cmake -S . -B /tmp/fincore-task42-invalid-imgui-confirm.DLp0yh/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task42-invalid-imgui-confirm.DLp0yh/not-a-dear-imgui-checkout \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$PWD/build/_deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
~~~

The configure exited 1, as required, at `cmake/fincore_implot.cmake:47`:

~~~text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task42-invalid-imgui-confirm.DLp0yh/not-a-dear-imgui-checkout'
~~~

It stopped at this validation boundary; no dependency was fetched, cloned,
vendored, fabricated, or substituted.

## Owned-path audit

Before staging, `git status --short` identified only the three Task 42 owned
paths: `README.md`, `tests/unit/metrics_umbrella_test.cpp`, and this acceptance
record. The required ignored Task 42 report is outside the commit by design.
`git diff --name-only` contained the two tracked modifications; the acceptance
record was the sole untracked owned path. `git diff --check` produced no output.
The umbrella test's only fincore includes are the two permitted public headers.
The initial Task 42 commit stages those three explicit paths only.

## Deferred scope

Daily transaction/bar-data joins; symbols/SIDs and transaction mapping;
liquidity and liquidation metrics; external market-data providers; alternative
execution-cost and market-impact models; round trips; attribution; advanced
analytics; visualization runtime; packaging/release; and full Python
public-API parity remain separate work.
