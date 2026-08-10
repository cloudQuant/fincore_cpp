# Phase 2C path metrics acceptance

## Decision and bounded scope

Phase 2C is accepted as a C++23, DataFrame-native extension for detailed
drawdown episodes/durations/ranks and **daily-only** consecutive-return
metrics. It is not full fincore parity or complete-project acceptance.

The public path remains one validated double-column `dataframe` indexed by
signed UTC nanoseconds. Detailed drawdown list/dict-style Python values are
native C++ value objects; daily consecutive statistics are a native batch
value. No second tabular representation, local-time conversion, or global
sorting/finite-input policy was added.

## Input commit chain

| Task | Accepted implementation commit |
| --- | --- |
| Task 19 — detailed drawdown episodes | `cccc59ed1e239c55cc2bf8a9bd25f7485d8acd28` |
| Task 20 — drawdown durations and ranks | `481ced0ce64ce4948b7f5dc3d4f7bc8dd99ff55f` |
| Task 21 — daily consecutive metrics | `be9b7ad2f41eda6472b999827ae6fdd443a6a2a5` |
| Task 22 — public umbrella and acceptance | This acceptance commit; its full post-commit SHA is recorded after Git creates it in `.superpowers/sdd/2026-08-10-fincore-cpp-phase-2c-path-metrics/task-22-report.md`. A Git commit cannot embed its own object ID. |

## Acceptance checklist

- [x] Task 19/20 detailed-drawdown Python-golden coverage is registered and
  exercised in the fresh full CTest suite: chronological recovered/open
  episodes, earliest trough/tie handling, NaN compounding, durations,
  recovery, UTC elapsed time, and severity ranks.
- [x] Task 21 daily-consecutive Python-golden coverage is registered and
  exercised in the fresh full CTest suite: daily run counts/gains/losses,
  dates, zero/NaN boundaries, signed infinities, first ties, original order,
  named columns, and structural errors.
- [x] `MetricsUmbrella` was RED before `metrics/consecutive.hpp` joined the
  public umbrella, then GREEN through public headers only.
- [x] `metrics.hpp` includes `fincore/metrics/consecutive.hpp` and no
  calendar, detail, analysis-context, rolling-engine, or ImPlot header.
- [x] A fresh headless AppleClang build passed all registered CTest tests.
- [x] A fresh Homebrew LLVM ASan+UBSan build passed all registered CTest tests
  with no sanitizer diagnostic.
- [x] The invalid local Dear ImGui source guard retains its exact diagnostic;
  no source was fetched, cloned, vendored, fabricated, or substituted.
- [ ] An independent positive `fincore_implot` / `fincore_implot_demo` build
  remains open until a real user-supplied Dear ImGui checkout is available.

## Public umbrella RED/GREEN evidence

`tests/unit/metrics_umbrella_test.cpp` includes only
`<fincore/metrics.hpp>` and the public `<fincore/calendar.hpp>` used to create
its UTC fixture. It instantiates both supported Phase 2C output types:

```cpp
const std::vector<fincore::drawdown_period> detailed_drawdowns =
    fincore::get_all_drawdowns_detailed(returns);
const fincore::daily_consecutive_statistics consecutive =
    fincore::daily_consecutive_stats(returns);
```

Before adding `fincore/metrics/consecutive.hpp` to `metrics.hpp`, a fresh
directory was created with `mktemp -d` as
`/tmp/fincore-task22-red.6i9Jpw`. The following configuration succeeded:

```sh
cmake -S . -B /tmp/fincore-task22-red.6i9Jpw/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
```

Then this build command exited `1` as expected:

```sh
cmake --build /tmp/fincore-task22-red.6i9Jpw/build --parallel 4
```

The precise missing-public-surface errors were:

```text
tests/unit/metrics_umbrella_test.cpp:64:18: error: no type named
'daily_consecutive_statistics' in namespace 'fincore'
tests/unit/metrics_umbrella_test.cpp:65:16: error: no member named
'daily_consecutive_stats' in namespace 'fincore'
```

The sole production change was adding
`#include <fincore/metrics/consecutive.hpp>` to `metrics.hpp`. No calendar,
detail, analysis-context, rolling-engine, or ImPlot include was added. In the
same preserved build directory, the following commands both exited `0`:

```sh
cmake --build /tmp/fincore-task22-red.6i9Jpw/build --parallel 4
ctest --test-dir /tmp/fincore-task22-red.6i9Jpw/build \
  -R MetricsUmbrella --output-on-failure
```

`MetricsUmbrella.ExposesOnlyImplementedDataframeNativeMetrics` passed `1/1`.

## Fresh headless AppleClang verification

`/usr/bin/c++ --version` reported:

```text
Apple clang version 17.0.0 (clang-1700.6.3.2)
```

CMake identified the compiler as `AppleClang 17.0.0.17000603`; Ninja was
`1.12.1` and CMake was `4.0.2`. A separate fresh directory created with
`mktemp -d`, `/tmp/fincore-task22-core.nzlndV`, remains preserved. Commands:

```sh
cmake -S . -B /tmp/fincore-task22-core.nzlndV/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task22-core.nzlndV/build --parallel 4
ctest --test-dir /tmp/fincore-task22-core.nzlndV/build --output-on-failure
```

All commands exited `0`; the fresh full CTest result was `132/132` passing.

## Fresh Homebrew LLVM ASan+UBSan verification

`/opt/homebrew/opt/llvm/bin/clang++ --version` reported:

```text
Homebrew clang version 22.1.8
```

CMake identified it as `Clang 22.1.8`. A distinct fresh directory created
with `mktemp -d`, `/tmp/fincore-task22-sanitize.asKk9c`, remains preserved.
Commands:

```sh
cmake -S . -B /tmp/fincore-task22-sanitize.asKk9c/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build /tmp/fincore-task22-sanitize.asKk9c/build --parallel 4
ctest --test-dir /tmp/fincore-task22-sanitize.asKk9c/build --output-on-failure
```

All commands exited `0`; the fresh full CTest result was `132/132` passing
with no ASan or UBSan diagnostic. The build emitted one
`-Wcharacter-conversion` warning from third-party GoogleTest's
`gtest-printers.h`; it was not a sanitizer finding.

## ImGui/ImPlot dependency boundary

No valid Dear ImGui checkout was supplied, fetched, cloned, vendored,
fabricated, or substituted. The independent negative configuration check used
a new `mktemp -d` directory, `/tmp/fincore-task22-invalid-imgui.hYXsmI`:

```sh
cmake -S . -B /tmp/fincore-task22-invalid-imgui.hYXsmI/build -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task22-invalid-imgui.hYXsmI/not-a-dear-imgui-checkout
```

The command exited `1` at `cmake/fincore_implot.cmake:47` with the retained
diagnostic:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task22-invalid-imgui.hYXsmI/not-a-dear-imgui-checkout'
```

This negative configuration result does not prove a positive adapter build or
demo. No positive ImPlot build was attempted without a real user-supplied
Dear ImGui checkout.

## Deferred/open work

Only daily consecutive indicators are accepted. Weekly/monthly consecutive
aggregation and Python's `consecutive_stats` remain deferred. So do
`get_top_drawdowns` and `gen_drawdown_table`, positions, transactions, round
trips, attribution, Bayesian/bootstrap metrics, a full native/Python-style
façade, packaging/release, and a positive `fincore_implot` /
`fincore_implot_demo` build/demo. Phase 2C is not full fincore parity or
project completion.
