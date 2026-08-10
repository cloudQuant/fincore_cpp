# fincore_cpp

The C++23, DataFrame-native foundation for fincore analytics.

## Accepted Phase 2A scalar metrics

Use the native scalar-metric surface through one public header:

```cpp
#include <fincore/metrics.hpp>
```

It exposes the accepted DataFrame-native basic, return, drawdown, ratio, risk,
statistics, alpha/beta, and timing APIs. The Phase 2A acceptance record,
including tested boundaries and explicitly deferred work, is available at
[`docs/acceptance/2026-08-10-phase-2a-scalar-metrics-acceptance.md`](docs/acceptance/2026-08-10-phase-2a-scalar-metrics-acceptance.md).

## Accepted Phase 2B calendar/yearly metric slice

The Phase 2B slice adds UTC calendar-year result DataFrames for yearly return,
volatility, Sharpe, drawdown, active-return, and information-ratio metrics,
along with per-year `annual_alpha` and `annual_beta`. The metric façade now
exports the yearly APIs:

```cpp
#include <fincore/calendar.hpp>  // utc_timestamp for the DataFrame index
#include <fincore/metrics.hpp>   // scalar, yearly, annual alpha/beta metrics
```

`calendar.hpp` intentionally remains a separate public dependency. The metric
façade includes `metrics/yearly.hpp`, but does not add calendar, detail,
analysis-context, rolling-engine, or ImPlot headers.

The accepted implementation chain uses these full commit IDs:

- Task 15 calendar helper: `e062ae70628575fe1e71524cc7fd36aa36619869`
  (follow-up boundary hardening:
  `70abe149772cebd0a40eb10c82367f9b5aeaeae3`).
- Task 16 yearly metrics: `b47d7c30c9035b96fde6aa7ca42ef253070bcad9`
  (follow-up alignment coverage:
  `466d8d84d85b0474f45737be8e3a711f478ef63e`).
- Task 17 annual alpha/beta: `f4fae66d237bf01ca13df4f75abdef5b94f97997`.

Fresh Phase 2B acceptance evidence from 2026-08-10 is:

- AppleClang `17.0.0.17000603`, in a new `/tmp` Ninja build with
  `FINCORE_BUILD_IMPLOT=OFF`: `cmake -S . -B
  /tmp/fincore-task18-core.rg1AFn/build -G Ninja
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF`, then `cmake --build
  /tmp/fincore-task18-core.rg1AFn/build --parallel 4`, then `ctest --test-dir
  /tmp/fincore-task18-core.rg1AFn/build --output-on-failure`; all exited `0`
  and CTest passed `108/108`.
- Homebrew LLVM `clang++ 22.1.8`, in a separate new `/tmp` Ninja build with
  `FINCORE_ENABLE_ASAN=ON` and `FINCORE_ENABLE_UBSAN=ON`: `cmake -S . -B
  /tmp/fincore-task18-sanitize.lEkSlr/build -G Ninja
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++`, then `cmake --build
  /tmp/fincore-task18-sanitize.lEkSlr/build --parallel 4`, then `ctest
  --test-dir /tmp/fincore-task18-sanitize.lEkSlr/build --output-on-failure`;
  all exited `0` and CTest passed `108/108` with no ASan or UBSan diagnostic.
  The build emitted one third-party GoogleTest `-Wcharacter-conversion`
  warning, which is not a sanitizer finding.
- A fresh ImPlot-enabled configure using a nonexistent
  `FINCORE_IMGUI_SOURCE_DIR` ran `cmake -S . -B
  /tmp/fincore-task18-invalid-imgui.wSq1cj/build -G Ninja
  -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task18-invalid-imgui.wSq1cj/not-a-dear-imgui-checkout`
  and exited `1` with:

  ```text
  FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
  files; received
  '/tmp/fincore-task18-invalid-imgui.wSq1cj/not-a-dear-imgui-checkout'
  ```

  No Dear ImGui source was fetched, cloned, vendored, fabricated, or
  substituted.

That Phase 2B acceptance covers only the calendar/yearly slice. Its remaining
scope is updated by the bounded Phase 2C acceptance below; it is not a full
fincore-parity or project-completion claim. See the historical record in
[`docs/acceptance/2026-08-10-phase-2b-calendar-metrics-acceptance.md`](docs/acceptance/2026-08-10-phase-2b-calendar-metrics-acceptance.md).

## Accepted Phase 2C path-metric slice

Phase 2C adds detailed drawdown episodes/durations/ranks and **daily-only**
consecutive-return metrics. The metric umbrella exposes both through:

```cpp
#include <fincore/metrics.hpp>
```

The public umbrella regression includes only `<fincore/metrics.hpp>` and the
already-public `<fincore/calendar.hpp>` for a UTC date fixture. It creates
`std::vector<fincore::drawdown_period>` with
`get_all_drawdowns_detailed`, and `fincore::daily_consecutive_statistics` with
`daily_consecutive_stats`. `metrics.hpp` gained only
`metrics/consecutive.hpp`: it still does not include calendar, detail,
analysis-context, rolling-engine, or ImPlot headers.

The accepted implementation chain is:

- Task 19 detailed drawdown episodes: `cccc59ed1e239c55cc2bf8a9bd25f7485d8acd28`.
- Task 20 drawdown durations and ranks:
  `481ced0ce64ce4948b7f5dc3d4f7bc8dd99ff55f`.
- Task 21 daily consecutive metrics:
  `be9b7ad2f41eda6472b999827ae6fdd443a6a2a5`.
- Task 22's full post-commit object ID is recorded in its ignored SDD report
  after Git creates the acceptance commit; a commit cannot contain its own
  object ID.

Fresh Phase 2C acceptance evidence from 2026-08-10 is:

- The umbrella test was first built in
  `/tmp/fincore-task22-red.6i9Jpw/build` before the `consecutive.hpp` include.
  It failed as intended with no `fincore::daily_consecutive_statistics` type
  and no `fincore::daily_consecutive_stats` member. After the one-header
  addition, `ctest --test-dir /tmp/fincore-task22-red.6i9Jpw/build -R
  MetricsUmbrella --output-on-failure` passed `1/1`.
- AppleClang `/usr/bin/c++` reported `Apple clang version 17.0.0
  (clang-1700.6.3.2)` and CMake identified `AppleClang 17.0.0.17000603`.
  In the new preserved Ninja directory `/tmp/fincore-task22-core.nzlndV/build`,
  `cmake -S . -B /tmp/fincore-task22-core.nzlndV/build -G Ninja
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF`, `cmake --build
  /tmp/fincore-task22-core.nzlndV/build --parallel 4`, and `ctest --test-dir
  /tmp/fincore-task22-core.nzlndV/build --output-on-failure` all exited `0`;
  CTest passed `132/132`.
- Homebrew `/opt/homebrew/opt/llvm/bin/clang++` reported `Homebrew clang
  version 22.1.8` (CMake `Clang 22.1.8`). In the separate preserved Ninja
  directory `/tmp/fincore-task22-sanitize.asKk9c/build`, the same headless
  configure plus `-DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++`, build, and CTest
  all exited `0`; CTest passed `132/132` with no ASan or UBSan diagnostic. The
  build emitted one third-party GoogleTest `-Wcharacter-conversion` warning.
- The preserved negative ImGui configure in
  `/tmp/fincore-task22-invalid-imgui.hYXsmI/build` used
  `-DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task22-invalid-imgui.hYXsmI/not-a-dear-imgui-checkout`
  and exited `1` at `cmake/fincore_implot.cmake:47` with:

  ```text
  FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
  files; received
  '/tmp/fincore-task22-invalid-imgui.hYXsmI/not-a-dear-imgui-checkout'
  ```

  No Dear ImGui source was fetched, cloned, vendored, fabricated, or
  substituted.

Phase 2C is not complete fincore parity or project completion. Only daily
consecutive indicators are accepted: weekly/monthly aggregation and Python's
`consecutive_stats` remain deferred. `get_top_drawdowns` and
`gen_drawdown_table`, positions, transactions, round trips, attribution,
Bayesian/bootstrap metrics, a full native/Python-style façade, packaging and
release work, and the positive `fincore_implot` / `fincore_implot_demo` build
remain open. The negative ImGui configuration check is not a positive ImPlot
build or demo result. See the complete record in
[`docs/acceptance/2026-08-10-phase-2c-path-metrics-acceptance.md`](docs/acceptance/2026-08-10-phase-2c-path-metrics-acceptance.md).

## Headless core verification

The core target is headless. It links `DataFrame::DataFrame` and does not
configure Dear ImGui, ImPlot, GLFW, or OpenGL unless the optional adapter is
explicitly enabled:

```bash
cmake -S . -B build-core -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure
```

For the core's ASan/UBSan gate, use Homebrew LLVM on this host:

```bash
cmake -S . -B build-sanitize -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON \
  -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build-sanitize --parallel 4
ctest --test-dir build-sanitize --output-on-failure
```

Sanitizer compilation remains target-scoped. When `fincore` is a static
library, its sanitizer link options are usage requirements of the final
consumer so the sanitizer runtimes are linked without mutating unrelated
targets. The current commands and observed results are recorded in
[`docs/acceptance/2026-08-10-foundation-acceptance.md`](docs/acceptance/2026-08-10-foundation-acceptance.md).

## Optional ImPlot adapter

`fincore` is headless and never links Dear ImGui, ImPlot, GLFW, or OpenGL. Set
`FINCORE_BUILD_IMPLOT=ON` to build the separate `fincore_implot` rendering
adapter. It consumes DataFrame columns while the application owns the active
Dear ImGui and ImPlot contexts and their rendering lifecycle.

The adapter uses only local source checkouts; CMake does not fetch dependencies.
The supplied ImPlot checkout is the default. Dear ImGui must be supplied
explicitly as a complete checkout containing `imgui.h`, `imgui_internal.h`,
the `imstb_*.h` headers, `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
and `imgui_widgets.cpp`:

```bash
cmake -S . -B build-implot -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/absolute/path/to/imgui
cmake --build build-implot --target fincore_implot
```

The missing-ImGui configuration diagnostic is verified, but it is not a
positive adapter-build result. The positive `fincore_implot` and demo gates
remain open until a real local `FINCORE_IMGUI_SOURCE_DIR` is supplied; do not
fetch, clone, vendor, or fabricate Dear ImGui to satisfy them.

Use the compile contract with that same, known configured binary directory:

```bash
cmake -DBINARY_DIR="$(pwd)/build-implot" \
  -P tests/compile/implot_adapter_compile_test.cmake
```

The GLFW/OpenGL dashboard is deliberately opt-in and is not a CTest dependency:

```bash
cmake -S . -B build-implot-demo -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/absolute/path/to/imgui
cmake --build build-implot-demo --target fincore_implot_demo
```
