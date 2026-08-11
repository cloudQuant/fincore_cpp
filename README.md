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

- Task 19 detailed drawdown episodes: initial
  `1c38c227d4c63edb2efb6be761ec6e06bbee0970`
  (`feat: add detailed drawdown episodes`); accepted follow-up
  `cccc59ed1e239c55cc2bf8a9bd25f7485d8acd28`
  (`fix: maintain drawdown peaks in wealth space`).
- Task 20 drawdown durations and ranks:
  `481ced0ce64ce4948b7f5dc3d4f7bc8dd99ff55f`.
- Task 21 daily consecutive metrics:
  `be9b7ad2f41eda6472b999827ae6fdd443a6a2a5`.
- Task 22 public umbrella and acceptance:
  `7e3c781030058c6c9a6660600c035ea0b2643564`
  (`docs: accept phase 2C path metrics`).

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
Bayesian/bootstrap metrics, optimization, simulation, reports, tearsheets, a
full native/Python-style façade, packaging and release work, and the
then-unaccepted `fincore_implot` / `fincore_implot_demo` build were open in
that dated Phase 2C record. Its former positive-ImPlot item is superseded by
the Phase 3C compile acceptance below. The negative ImGui configuration check
is not a positive ImPlot build or demo result. See the complete record in
[`docs/acceptance/2026-08-10-phase-2c-path-metrics-acceptance.md`](docs/acceptance/2026-08-10-phase-2c-path-metrics-acceptance.md).

## Accepted Phase 3A rolling scalar metrics

The public metric umbrella now adds only the accepted scalar rolling façade:

```cpp
#include <fincore/metrics.hpp>

// One requested double column in, preserving its name in each result.
const auto endpoint_sharpe = fincore::roll_sharpe_ratio(returns, 252U);
const auto endpoint_drawdown = fincore::roll_max_drawdown(returns, 252U);
const auto full_index_volatility = fincore::rolling_volatility(returns, 252U);
const auto full_index_sharpe = fincore::rolling_sharpe(returns, 252U);
```

`roll_sharpe_ratio` and `roll_max_drawdown` return only completed-window
endpoint indices. `rolling_volatility` and `rolling_sharpe` retain the full
input index and use the established NaN values before a complete window. All
four preserve the requested column name. The umbrella adds only
`metrics/rolling.hpp`; it does not export `rolling_engine.hpp`, calendar,
detail, analysis-context, or ImPlot headers, and it exposes no paired or
multi-column rolling APIs.

The accepted implementation chain is Task 23
`4e0394b15a9d8beaac2ee6ebdeb80c29a31042a1` (`feat: add scalar rolling
metrics`) followed by `82fb69c883bc19bb3993961fb673b0bc177ef03b` (`fix:
include initial rolling drawdown point`), and its Task 24 public-umbrella
acceptance commit `c110d173499dca4ab31a1c020c4562d413335e06` (`docs: accept
phase 3A rolling scalar metrics`). The registered Python-golden tests cover
the two output shapes, requested names, and the corrected completed-window NaN
fixture, whose `roll_sharpe_ratio` result is `0.0`.

Fresh Phase 3A acceptance evidence from 2026-08-10 is:

- A public-only umbrella RED in preserved
  `/tmp/fincore-task24-checklist-red.NioYmC/build` used
  `cmake -S . -B /tmp/fincore-task24-checklist-red.NioYmC/build -G Ninja
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF`, then `cmake --build
  /tmp/fincore-task24-checklist-red.NioYmC/build --parallel 4`. It exited `1`
  before publication because all four rolling declarations were absent from
  `<fincore/metrics.hpp>`. After adding only
  `#include <fincore/metrics/rolling.hpp>`, that same preserved build and
  `ctest --test-dir /tmp/fincore-task24-checklist-red.NioYmC/build -R
  MetricsUmbrella --output-on-failure` exited `0` and passed `1/1`.
- AppleClang `17.0.0.17000603` (`/usr/bin/c++`: Apple clang `17.0.0`) ran the
  fresh preserved command set `cmake -S . -B
  /tmp/fincore-task24-core.yrdcIb/build -G Ninja
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF`, `cmake --build
  /tmp/fincore-task24-core.yrdcIb/build --parallel 4`, and `ctest --test-dir
  /tmp/fincore-task24-core.yrdcIb/build --output-on-failure`; all exited `0`
  and CTest passed `139/139`.
- Homebrew LLVM `clang++ 22.1.8` (`/opt/homebrew/opt/llvm/bin/clang++`) ran a
  separate fresh preserved command set: `cmake -S . -B
  /tmp/fincore-task24-sanitize.OSfL0y/build -G Ninja
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++`, `cmake --build
  /tmp/fincore-task24-sanitize.OSfL0y/build --parallel 4`, and `ctest
  --test-dir /tmp/fincore-task24-sanitize.OSfL0y/build --output-on-failure`.
  All exited `0`; CTest passed `139/139` with no ASan or UBSan diagnostic. One
  `-Wcharacter-conversion` warning arose from third-party GoogleTest and was
  not a sanitizer finding.
- The independent invalid-ImGui gate ran `cmake -S . -B
  /tmp/fincore-task24-invalid-imgui.wPV832/build -G Ninja
  -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task24-invalid-imgui.wPV832/not-a-dear-imgui-checkout`
  and exited `1` at `cmake/fincore_implot.cmake:47` with `FINCORE_IMGUI_SOURCE_DIR
  must name a Dear ImGui checkout with all required files`. No Dear ImGui
  source was fetched, cloned, vendored, fabricated, or substituted; no
  positive ImPlot build was attempted.

Deferred work remains the paired rolling `roll_alpha`, `roll_beta`, and
capture functions; multi-column `roll_alpha_beta` and `rolling_regression`;
`rolling_beta`; portfolio positions and transactions; round trips;
performance attribution/stats; Bayesian/bootstrap functions; optimization;
simulation; reports/tearsheets; a full native facade; packaging/release; and
the then-unaccepted `fincore_implot` / `fincore_implot_demo` build/demo in
that dated Phase 3A observation. Its former positive-ImPlot item is superseded
by the Phase 3C compile acceptance below. Phase 3A is not full fincore parity
or project completion. The complete acceptance evidence is
in [`docs/acceptance/2026-08-10-phase-3a-rolling-scalar-metrics-acceptance.md`](docs/acceptance/2026-08-10-phase-3a-rolling-scalar-metrics-acceptance.md).

## Accepted Phase 3B rolling paired metrics

The public metric umbrella now adds the five paired rolling endpoint functions
through the already-exposed `metrics/rolling.hpp`:

```cpp
#include <fincore/metrics.hpp>

// One strategy series and one benchmark series in, one endpoint series out.
const auto a = fincore::roll_alpha(returns, benchmark, 252U);
const auto b = fincore::roll_beta(returns, benchmark, 252U);
const auto up = fincore::roll_up_capture(returns, benchmark, 252U);
const auto down = fincore::roll_down_capture(returns, benchmark, 252U);
const auto up_down = fincore::roll_up_down_capture(returns, benchmark, 252U);
```

Each result is a one-column double series named by the requested returns
column, indexed at the final timestamp of every complete aligned window. The
two inputs are aligned exactly once through `align_on_common_index`, retaining
common timestamps in the left/strategy frame's supplied order, and every
window delegates to the corresponding accepted public scalar function.
`roll_alpha` resolves its annualization once; `roll_beta` forwards `risk_free`
and intentionally ignores `period`/`annualization`, matching scalar beta; the
three capture functions resolve their period-derived annualization once. No
two-column `roll_alpha_beta`, `rolling_regression`, full-index `rolling_beta`,
multi-factor DataFrame, or `rolling_engine` change is exposed.

Validation ordering is deliberately asymmetric for empty or short aligned
inputs: alpha and the three capture endpoints resolve their frequency-derived
annualization first, so an invalid frequency throws before a typed-empty result
is returned. Beta ignores `period`/`annualization`; for the same empty or short
inputs and invalid frequency, it returns its normal typed-empty result. The
registered `PairedEndpointsValidateAnnualizationBeforeReturningEmptyAlignedResults`
test covers both cases.

The accepted implementation chain is Task 25
`7c779a131435960d4bb215762970fe37a1db500a` (`feat: add paired rolling
metrics`) followed by `091f9173433468c7ee70dc564ace2e5fcd0e52f2` (`fix:
validate paired rolling annualization`), and its Task 26 public-umbrella
acceptance commit `a0c130d815cb1d18f85e1c01b73cfc8e6d59d32a` (`test: cover
paired rolling metrics in umbrella`), followed by the Task 26 final
acceptance-document commit `32157fbfd190a2a83ad12d104e3cc1fdcc1658e7` (`docs:
accept phase 3B paired rolling metrics`). The registered Python-golden tests cover
all five endpoint vectors, left-order alignment, empty/short/no-common-index
behavior, `window == 0` rejection, custom columns, beta's ignored
annualization, and a NaN fixture compared directly against the public scalar
window delegate.

Fresh Phase 3B acceptance evidence from 2026-08-10 is:

- A public-only umbrella RED was the genuine public-header absence recorded
  during Task 25: before `7c779a1`, all five paired declarations were absent
  from `<fincore/metrics.hpp>`, so a public-only consumer failed with `no
  member named` errors. After `7c779a1`, the Task 26 GREEN
  `a0c130d815cb1d18f85e1c01b73cfc8e6d59d32a` extended `MetricsUmbrella` to
  call all five APIs and assert the `{3}` endpoint index, the `strategy`
  column, and a single double column.
- Focused replays of that public consumer passed `1/1` in both preserved Task
  26 builds: `ctest --test-dir /tmp/fincore-task26-core.VfIPiv/build -R
  MetricsUmbrella --output-on-failure` and `ctest --test-dir
  /tmp/fincore-task26-sanitize.TjdgSE/build -R MetricsUmbrella
  --output-on-failure`. The recorded complete CTest runs below remain
  `146/146`.
- AppleClang `17.0.0.17000603` (`/usr/bin/c++`: Apple clang `17.0.0`) ran the
  fresh preserved command set `cmake -S . -B
  /tmp/fincore-task26-core.VfIPiv/build -G Ninja
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF`, `cmake --build
  /tmp/fincore-task26-core.VfIPiv/build --parallel 4`, and `ctest --test-dir
  /tmp/fincore-task26-core.VfIPiv/build --output-on-failure`; all exited `0`
  and CTest passed `146/146`.
- Homebrew LLVM `clang++ 22.1.8` (`/opt/homebrew/opt/llvm/bin/clang++`) ran a
  separate fresh preserved command set: `cmake -S . -B
  /tmp/fincore-task26-sanitize.TjdgSE/build -G Ninja
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++`, `cmake --build
  /tmp/fincore-task26-sanitize.TjdgSE/build --parallel 4`, and `ctest
  --test-dir /tmp/fincore-task26-sanitize.TjdgSE/build --output-on-failure`.
  All exited `0`; CTest passed `146/146` with no ASan or UBSan diagnostic.
- The independent invalid-ImGui gate ran `cmake -S . -B
  /tmp/fincore-task26-invalid-imgui.cqhcsc/build -G Ninja
  -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task26-invalid-imgui.cqhcsc/not-a-dear-imgui-checkout`
  and exited `1` at `cmake/fincore_implot.cmake:47` with `FINCORE_IMGUI_SOURCE_DIR
  must name a Dear ImGui checkout with all required files`. No Dear ImGui
  source was fetched, cloned, vendored, fabricated, or substituted; no
  positive ImPlot build was attempted.

Deferred work remains multi-column `roll_alpha_beta` and `rolling_regression`;
full-index `rolling_beta` (including multi-factor inputs); portfolio positions
and transactions; round trips; performance attribution/stats; Bayesian/
bootstrap functions; optimization; simulation; reports/tearsheets; a full
native facade; packaging/release; and the then-unaccepted `fincore_implot` /
`fincore_implot_demo` build/demo in that dated Phase 3B observation. Its
former positive-ImPlot item is superseded by the Phase 3C compile acceptance
below. Phase 3B is not full fincore parity or project completion. The complete
acceptance evidence is in
[`docs/acceptance/2026-08-10-phase-3b-rolling-paired-metrics-acceptance.md`](docs/acceptance/2026-08-10-phase-3b-rolling-paired-metrics-acceptance.md).

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

## Optional ImPlot adapter — accepted compile boundary

`fincore` remains headless by default: `FINCORE_BUILD_IMPLOT` and
`FINCORE_BUILD_IMPLOT_DEMO` are both `OFF`, so the core does not link Dear
ImGui, ImPlot, GLFW, or OpenGL. Enable the separate `fincore_implot` adapter
only when graphics are wanted. Its caller owns current Dear ImGui and ImPlot
contexts and their frame/render lifecycle.

The cache default for `FINCORE_IMGUI_SOURCE_DIR` is the user-supplied local
checkout `/Users/yunjinqi/Documents/new_projects/imgui`. It must be complete
(`imgui.h`, `imgui_internal.h`, the `imstb_*.h` headers, `imgui.cpp`,
`imgui_draw.cpp`, `imgui_tables.cpp`, and `imgui_widgets.cpp`). CMake uses only
local sources here and does not fetch, clone, vendor, fabricate, or substitute
Dear ImGui. Callers can override the cache path when their own complete
checkout should be used.

The normal local positive build deliberately does **not** pass
`FINCORE_IMGUI_SOURCE_DIR`, proving the cache default:

```bash
cmake -S . -B build-implot -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot
cmake --build build-implot --parallel 4
ctest --test-dir build-implot --output-on-failure
```

To override only the local Dear ImGui cache value, add (or set in CMake's
cache UI):

```bash
-DFINCORE_IMGUI_SOURCE_DIR=/absolute/path/to/another/complete/imgui
```

With ImPlot enabled, CTest registers `ImPlotAdapterCompile`; it is a compile
contract for `fincore_implot`, not a GUI test. It is absent from a headless
`FINCORE_BUILD_IMPLOT=OFF` configuration along with graphics dependencies.
Fresh Phase 3C evidence accepted the default-path adapter and
`fincore_implot_demo` compilation on AppleClang and on Homebrew LLVM 22 with
ASan/UBSan, each at `147/147` CTest tests; the fresh headless build passed
`146/146` without `ImPlotAdapterCompile`. See the complete commands, compiler
versions, warnings, and invalid-override diagnostic in
[`docs/acceptance/2026-08-11-phase-3c-implot-activation-acceptance.md`](docs/acceptance/2026-08-11-phase-3c-implot-activation-acceptance.md).

The GLFW/OpenGL demo compiles and links, but its GUI runtime was not launched
by automation. Interactive visual smoke testing requires a user display. The
accepted macOS builds emitted three OpenGL deprecation warnings (`glViewport`,
`glClearColor`, and `glClear`); this is a warning record, not a no-warning
claim. An explicit invalid `FINCORE_IMGUI_SOURCE_DIR` remains an expected
negative CMake diagnostic, not evidence against the positive default path.

Task 27's optional-build contract is
`e60d64ec65560e80e079f89e3d433007b8d3f688`
(`test: register ImPlot adapter build contract`). Task 28's initial
evidence-first documentation commit is
`61aee786a01abf8cd97e7d58f841c1441116aa83`
(`docs: accept positive ImPlot build`); this documentation-only traceability
follow-up records that first Task 28 SHA and intentionally does not
self-reference. The dated foundation and Phase 2C/3A/3B records retain their
former positive-ImPlot OPEN observations; that historical item is superseded
by Phase 3C rather than rewritten.

This boundary is not full fincore parity or a project-release claim. Still-open
work includes interactive visual smoke testing; multi-series and tearsheet
visualizations; unimplemented multi-column and full-index rolling APIs;
positions and transactions; round trips; attribution; Bayesian/bootstrap and
advanced risk models; optimization; simulation; reports; data providers;
packaging and release; and full Python public-API parity.
