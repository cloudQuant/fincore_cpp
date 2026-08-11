# Phase 4A performance statistics acceptance

## Scope and traceability

This is a bounded acceptance of the deterministic native
`fincore::perf_stats` facade. It does not claim complete fincore parity,
release readiness, a Python string-indexed result table, Python's occasional
outer-shaped pair intermediate, or an interactive visual result.

- Task 29 implementation:
  `a1caa4bc8bd2496ff67c7728a7368e8214a6a1fd`
  (`feat: add performance statistics facade`).
- Task 29 public-header repair:
  `1cd7cccc9cc5b78c417f8d0452d463357d4d0e4b`
  (`test: prove performance statistics public header`).
- Task 30 acceptance/documentation publication:
  `5600dbb2bc2c21a29833d0020e58f8d2154e2a68`
  (`docs: accept phase 4A performance statistics`). This documentation-only
  traceability follow-up does not self-reference or amend history.

## Accepted public contract

`<fincore/metrics.hpp>` includes `metrics/perf_stats.hpp` exactly once. The
existing `MetricsUmbrella` consumer includes only `<fincore/metrics.hpp>` and
its existing `<fincore/calendar.hpp>` date helper; it calls both overloads
using the custom input names `"strategy"` and `"market"` and verifies their
complete key sets.

The native return type is `std::map<std::string, double>`, keyed by metric
name. It is deliberately not a synthetic string-indexed timestamp DataFrame.
The unary overload returns exactly these 13 keys:

```text
Annual return             Cumulative returns       Annual volatility
Sharpe ratio              Calmar ratio              Stability
Max drawdown              Omega ratio               Sortino ratio
Skew                      Kurtosis                  Tail ratio
Daily value at risk
```

The benchmark overload returns those same 13 keys plus exactly `Alpha` and
`Beta`, for 15 keys. Callers select the returns and benchmark double columns
through the public `returns_column_name` and `benchmark_column_name`
arguments; the accepted umbrella regression proves non-default names rather
than relying only on the default `returns` and `benchmark` names.

`period` changes annual return, annual volatility, Sharpe, Calmar, and
Sortino through the requested annualization factor. `Alpha` and `Beta` use the
daily `alpha_beta` delegate, while `Omega ratio` retains its zero-threshold,
daily delegate convention. The facade introduces no new nonfinite rule:
nonfinite values, column validation, and benchmark timestamp behavior remain
owned by the public scalar delegates. In particular, native Alpha/Beta align
common timestamps in the left/returns input order; this acceptance does not
assert Python outer-shaped pair behavior.

Task 29's `PerfStats` tests supply finite daily and weekly Python-oracle
goldens, exact key sets, custom columns, empty genuine double columns,
structural validation, mismatched benchmark timestamps, NaN, and infinity.
Its public-header RED was the genuine pre-implementation error:

```text
fatal error: 'fincore/metrics/perf_stats.hpp' file not found
```

After the Task 29 public-header repair, this Task 30 umbrella-only consumer
was built and run without removing any accepted header to manufacture another
RED:

```bash
cmake --build build --parallel 4
ctest --test-dir build -R MetricsUmbrella --output-on-failure
```

Both commands exited `0`; the targeted CTest run passed `1/1`.

## Acceptance checklist

- [x] Public umbrella exposes the two `perf_stats` overloads and their exact
  13/15-key contracts for custom named return and benchmark columns.
- [x] Task 29's public-header RED and the new public-umbrella GREEN are
  recorded without removing accepted functionality to manufacture a failure.
- [x] The finite Python-oracle, structural validation, nonfinite delegation,
  and benchmark alignment boundaries are traceable to Task 29 evidence.
- [x] A fresh AppleClang headless configure, build, and full CTest run pass.
- [x] A fresh Homebrew LLVM 22 ASan+UBSan headless configure, build, and full
  CTest run pass without sanitizer or runtime-error diagnostics.
- [x] A fresh default-path ImGui/ImPlot/demo configure, build, and CTest run
  pass without launching a GUI.
- [x] A fresh explicit invalid-ImGui configure exits with the expected CMake
  diagnostic.
- [x] Commit SHAs, warnings, remaining scope, and owned-path audit are based
  on completed commands and committed history only.

## Fresh headless verification

All commands below used CMake `4.0.2`, Ninja, and the source worktree at
`/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation`.

### AppleClang core

Fresh root: `/tmp/fincore-task30-core.WFaqdh`. `/usr/bin/c++ --version`
reported Apple clang `17.0.0` (`clang-1700.6.3.2`); CMake identified
`AppleClang 17.0.0.17000603` and used `/usr/bin/c++`.

```bash
cmake -S . -B /tmp/fincore-task30-core.WFaqdh/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task30-core.WFaqdh/build --parallel 4
ctest --test-dir /tmp/fincore-task30-core.WFaqdh/build -N
ctest --test-dir /tmp/fincore-task30-core.WFaqdh/build --output-on-failure
```

Every command exited `0`. `ctest -N` reported `Total Tests: 152`; the full
run passed `152/152` with `0` failures. The captured fresh build output had no
compiler warning diagnostic.

### Homebrew LLVM 22 ASan+UBSan core

Fresh root: `/tmp/fincore-task30-sanitize.dLPsFE`.
`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake identified `Clang 22.1.8` and used that compiler for C and
C++. The cache records `FINCORE_ENABLE_ASAN:BOOL=ON` and
`FINCORE_ENABLE_UBSAN:BOOL=ON`.

```bash
cmake -S . -B /tmp/fincore-task30-sanitize.dLPsFE/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON \
  -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build /tmp/fincore-task30-sanitize.dLPsFE/build --parallel 4
ctest --test-dir /tmp/fincore-task30-sanitize.dLPsFE/build --output-on-failure
```

Every command exited `0`; full CTest passed `152/152` with `0` failures. The
build emitted one third-party GoogleTest
`-Wcharacter-conversion` warning at `gtest-printers.h:524` about converting
`char8_t` to `char32_t`; it is not a fincore or sanitizer finding. No
`AddressSanitizer`, `UndefinedBehaviorSanitizer`, or `runtime error`
diagnostic appeared in the captured LLVM build or CTest output.

## Fresh optional ImPlot compile boundary

Fresh root: `/tmp/fincore-task30-implot.Ufa94z`. This AppleClang configuration
intentionally passed neither `FINCORE_IMGUI_SOURCE_DIR` nor
`FINCORE_IMPLOT_SOURCE_DIR`; its cache resolved the defaults to the
user-supplied local checkouts:

```text
FINCORE_IMGUI_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/imgui
FINCORE_IMPLOT_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/implot
```

```bash
cmake -S . -B /tmp/fincore-task30-implot.Ufa94z/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-task30-implot.Ufa94z/build --parallel 4
ctest --test-dir /tmp/fincore-task30-implot.Ufa94z/build -N
ctest --test-dir /tmp/fincore-task30-implot.Ufa94z/build --output-on-failure
```

Every command exited `0`. The build linked `fincore_implot_demo`; `ctest -N`
listed `ImPlotAdapterCompile` as test `#153`, and the full run passed `153/153`
with `0` failures. The build emitted exactly three macOS OpenGL deprecation
warnings for `glViewport`, `glClearColor`, and `glClear`. No GUI executable was
launched: this remains an adapter/demo compile-and-link plus headless CTest
contract, not interactive visual proof.

## Fresh invalid ImGui override

Fresh root: `/tmp/fincore-task30-invalid-imgui.BJWB1f`.

```bash
cmake -S . -B /tmp/fincore-task30-invalid-imgui.BJWB1f/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task30-invalid-imgui.BJWB1f/not-a-dear-imgui-checkout
```

The configure exited `1` at `cmake/fincore_implot.cmake:47` with:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task30-invalid-imgui.BJWB1f/not-a-dear-imgui-checkout'
```

This negative diagnostic is not evidence against the positive default-path
build. No Dear ImGui or ImPlot source was fetched, cloned, vendored,
fabricated, substituted, or otherwise mutated by these checks.

## Deferred scope

`perf_stats_bootstrap`, `calc_bootstrap`, and `calc_distribution_stats`;
positions, transactions, and round trips; attribution; reports and tearsheets;
the broader native/Python-style facade; packaging and release remain deferred.
So do multi-column and full-index rolling APIs, Bayesian and advanced-risk
models, optimization, simulation, data providers, and interactive ImPlot
visual smoke testing. This accepted slice is not project completion.
