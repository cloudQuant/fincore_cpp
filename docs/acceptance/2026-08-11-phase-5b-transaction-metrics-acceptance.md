# Phase 5B transaction metrics acceptance

## Scope and traceability

This bounded acceptance publishes native daily transaction-volume aggregation
and turnover metrics from Python fincore's transaction analysis. It is not a
transaction mapping, symbol/SID, bar-data, liquidity, slippage, round-trip,
attribution, GUI-runtime, or whole-project fincore-parity claim.

- Task 35 implementation: 87472e42bfa6802e0275af1bf0c3b3058a4f5f23
  (feat: add native transaction turnover metrics).
- Task 35 accepted boundary correction:
  12ec9a0368ea4dd54b6f6669f2118a204947370d
  (test: cover transaction turnover boundaries).
- Task 36 initial acceptance evidence:
  5da9f1608a776153e9fcca1059ea0fb064266fd5
  (docs: accept phase 5B transaction metrics). This docs-only trace commit
  intentionally neither self-references nor amends that initial acceptance.

## Acceptance checklist

- [x] A public-only MetricsUmbrella consumer uses only
  <fincore/metrics.hpp> plus the existing public calendar helper, creates
  duplicate/intraday raw transactions and daily positions, calls daily volume
  plus both turnover denominator enums, and checks schemas, UTC-midnight
  indices, representative values, and a transaction-only union day.
- [x] The genuine Task 35 public-header RED remains pre-implementation
  evidence; no accepted public header is removed to fabricate a new RED.
- [x] A fresh AppleClang headless configure, build, and full CTest run passes.
- [x] A separate fresh Homebrew LLVM 22 ASan+UBSan headless configure, build,
  and full CTest run passes without sanitizer diagnostics.
- [x] The default local ImGui/ImPlot adapter/demo compile-and-link CTest
  boundary passes without launching a GUI.
- [x] A fresh invalid FINCORE_IMGUI_SOURCE_DIR configure fails with the
  intended CMake diagnostic and does not fetch, clone, vendor, fabricate, or
  substitute dependencies.
- [x] Compiler versions, exact test counts, warnings, sanitizer diagnostics,
  owned-path audit, and deferred scope are filled only from completed
  verification.

## Accepted public contract

- The public umbrella header exports transactions.hpp, including
  get_txn_vol, get_turnover, and turnover_denominator.
- Raw transactions require distinct genuine double amount and price columns,
  each matched to the transaction index. Any duplicate or intraday timestamp
  is normalized to UTC midnight, then its contribution is accumulated into a
  chronologically sorted daily row.
- Daily output has exactly the double columns txn_volume then txn_shares.
  Shares are absolute amounts; volume is absolute amount multiplied by the
  signed price. A NaN contribution is skipped: a NaN amount contributes
  neither field, while a NaN price retains its valid share contribution but
  contributes no volume. Signed infinity remains ordinary IEEE arithmetic in
  daily aggregation.
- Turnover output has exactly one double column, turnover, on the sorted union
  of normalized transaction and position days. Position dates must be strictly
  increasing and unique after UTC-day normalization; raw transaction rows do
  not have that restriction.
- actual_gross_book requires a genuine named double cash position field.
  Cash is excluded from gross exposure. The first denominator is half the
  first gross book, and every later denominator is the average of the previous
  and current gross books. portfolio_value has no cash requirement and instead
  uses each row's sum across all genuine double position fields.
- Missing denominators and any other nonfinite final turnover result are
  emitted as 0.0. This final sanitation does not alter an infinite daily
  transaction-volume aggregate.

## Fresh verification

All commands used source worktree
/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation,
CMake 4.0.2, Ninja 1.12.1, and the pre-existing local GoogleTest source at
/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation/build/_deps/googletest-src.
Each fresh configuration set FETCHCONTENT_SOURCE_DIR_GOOGLETEST to that local
source and FETCHCONTENT_FULLY_DISCONNECTED to ON, so these gates did not
request a network fetch, clone, vendoring operation, fabricated dependency, or
dependency substitution.

### Task 35 RED evidence

Before Task 35 production code existed, its newly registered public consumer
included only fincore/error.hpp, fincore/calendar.hpp, and
fincore/metrics/transactions.hpp. The command cmake --build build --parallel 4
exited nonzero with:

~~~text
tests/unit/transactions_test.cpp:3:10: fatal error:
'fincore/metrics/transactions.hpp' file not found
~~~

That is the retained genuine public-header RED. Task 36 does not remove the
accepted header to manufacture another RED.

### AppleClang headless core

Fresh root: /tmp/fincore-task36-apple.Vpes3U. /usr/bin/c++ --version reported
Apple clang 17.0.0 (clang-1700.6.3.2), and CMake identified AppleClang
17.0.0.17000603.

~~~sh
cmake -S . -B /tmp/fincore-task36-apple.Vpes3U/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation/build/_deps/googletest-src \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task36-apple.Vpes3U/build --parallel 4
ctest --test-dir /tmp/fincore-task36-apple.Vpes3U/build -N
ctest --test-dir /tmp/fincore-task36-apple.Vpes3U/build --output-on-failure
~~~

Every command exited 0. CTest -N reported Total Tests: 179 and the full run
passed 179/179 with zero failures. The captured AppleClang build output
contained no compiler warning diagnostic.

### Homebrew LLVM 22 ASan and UBSan headless core

Fresh root: /tmp/fincore-task36-llvm.4EEBSA.
/opt/homebrew/opt/llvm/bin/clang++ --version reported Homebrew clang 22.1.8;
CMake identified Clang 22.1.8 and used Homebrew LLVM for both C and C++.

~~~sh
cmake -S . -B /tmp/fincore-task36-llvm.4EEBSA/build -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation/build/_deps/googletest-src \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task36-llvm.4EEBSA/build --parallel 4
ctest --test-dir /tmp/fincore-task36-llvm.4EEBSA/build -N
ctest --test-dir /tmp/fincore-task36-llvm.4EEBSA/build --output-on-failure
~~~

Every command exited 0. CTest -N reported Total Tests: 179 and the full run
passed 179/179 with zero failures. The build emitted one third-party
GoogleTest warning at gtest-printers.h:524: implicit char8_t to char32_t
conversion under -Wcharacter-conversion. It is not a fincore warning. No
AddressSanitizer, UndefinedBehaviorSanitizer, or runtime error diagnostic
appeared in the completed build or CTest run.

### Default local ImGui and ImPlot adapter/demo compile boundary

Fresh root: /tmp/fincore-task36-implot.TtMgU7. This AppleClang configuration
passed neither FINCORE_IMGUI_SOURCE_DIR nor FINCORE_IMPLOT_SOURCE_DIR. Its
cache resolved the supplied local defaults:

~~~text
FINCORE_IMGUI_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/imgui
FINCORE_IMPLOT_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/implot
~~~

~~~sh
cmake -S . -B /tmp/fincore-task36-implot.TtMgU7/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation/build/_deps/googletest-src \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build /tmp/fincore-task36-implot.TtMgU7/build --parallel 4
ctest --test-dir /tmp/fincore-task36-implot.TtMgU7/build -N
ctest --test-dir /tmp/fincore-task36-implot.TtMgU7/build \
  --output-on-failure --parallel 4
~~~

Every command exited 0. The build linked fincore_implot_demo, CTest -N listed
ImPlotAdapterCompile as test 180, and the full run passed 180/180 with zero
failures. The build emitted exactly three macOS OpenGL deprecation warnings
for glViewport, glClearColor, and glClear. No GUI executable was launched:
this is a compile/link and headless CTest boundary, not an interactive visual
validation claim.

### Explicit invalid ImGui override

Fresh root: /tmp/fincore-task36-invalid-imgui.9SG0mH.

~~~sh
cmake -S . -B /tmp/fincore-task36-invalid-imgui.9SG0mH/build -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task36-invalid-imgui.9SG0mH/not-a-dear-imgui-checkout \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation/build/_deps/googletest-src \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
~~~

The configure exited 1 at cmake/fincore_implot.cmake:47 with:

~~~text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task36-invalid-imgui.9SG0mH/not-a-dear-imgui-checkout'
~~~

This expected negative diagnostic is independent from the successful default
local ImGui/ImPlot build. It stopped during the source-path validation and did
not fetch, clone, vendor, fabricate, or substitute a graphics dependency.

## Owned-path audit

Task 36 modifies only include/fincore/metrics.hpp,
tests/unit/metrics_umbrella_test.cpp, README.md, and this acceptance record.
The completed pre-stage git diff --check had no output, and git diff
--name-only reported exactly those four paths. The ignored task report is
.superpowers/sdd/2026-08-11-fincore-cpp-phase-5b-transaction-metrics/task-36-report.md;
the local .superpowers/sdd/.gitignore rule excludes it from staging.

## Deferred scope

Transaction mapping and symbols/SIDs; daily bar joins; liquidity,
liquidation, and slippage; round trips and attribution; performance
attribution; bootstrap/distribution statistics; Bayesian and advanced-risk
models; optimization; simulation; reports/tearsheets; a full native facade;
packaging/release; and interactive ImPlot visual smoke testing remain
deferred.
