# Phase 3A rolling scalar metrics acceptance

## Decision and bounded scope

Phase 3A is accepted as the public, scalar rolling-metrics slice of the
C++23, DataFrame-native API. It publishes exactly four existing one-column
functions through `<fincore/metrics.hpp>`:

- `roll_sharpe_ratio` and `roll_max_drawdown` preserve the requested column
  name and return the completed-window endpoint index.
- `rolling_volatility` and `rolling_sharpe` preserve the requested column name
  and return the complete input index, with unavailable windows represented by
  their established NaN values.

This task changes no rolling computation or input policy. `metrics.hpp` gains
only `fincore/metrics/rolling.hpp`; it does not include `rolling_engine.hpp`,
calendar, detail, analysis-context, ImPlot, or other rendering headers. No
paired or multi-column rolling API is exposed.

## Input commit chain

| Task | Accepted implementation commit |
| --- | --- |
| Task 23 — scalar rolling metrics | `4e0394b15a9d8beaac2ee6ebdeb80c29a31042a1` (`feat: add scalar rolling metrics`) |
| Task 23 follow-up — initial rolling drawdown point | `82fb69c883bc19bb3993961fb673b0bc177ef03b` (`fix: include initial rolling drawdown point`) |
| Task 24 — public umbrella and acceptance | `c110d173499dca4ab31a1c020c4562d413335e06` (`docs: accept phase 3A rolling scalar metrics`) |

The Task 23 NaN-oracle correction is already present in
`59f4b638ee8194ab78e64517f193b9d233934b93`: the completed-window
`roll_sharpe_ratio` fixture expects `0.0`, not a NaN value.

## Acceptance checklist

- [x] A public-only `MetricsUmbrella` RED then GREEN covers all four scalar
  rolling APIs, their requested-column preservation, and endpoint/full-index
  output shapes.
- [x] Registered Python-oracle coverage exercises the accepted scalar rolling
  behavior, including the corrected `0.0` NaN fixture.
- [x] A fresh headless AppleClang CTest run passes all 139 registered tests.
- [x] A fresh Homebrew LLVM ASan+UBSan CTest run passes all 139 registered
  tests with no sanitizer diagnostic.
- [x] A fresh invalid-Dear-ImGui configuration preserves its documented
  precise diagnostic; a positive ImPlot build remains deferred.

## Public umbrella RED/GREEN evidence

`tests/unit/metrics_umbrella_test.cpp` includes only public
`<fincore/metrics.hpp>` (and its existing public `<fincore/calendar.hpp>` UTC
fixture). It creates the one-column `strategy` series and calls all four
accepted APIs through the umbrella. The regression checks that
`roll_sharpe_ratio` and `roll_max_drawdown` return `{3}`, while
`rolling_volatility` and `rolling_sharpe` retain `{1, 2, 3}`, and that each
output retains `strategy` as its requested metric column.

Before `metrics/rolling.hpp` was added to the public umbrella, the tracked
checklist above was created and a fresh directory was preserved as
`/tmp/fincore-task24-checklist-red.NioYmC`. Configuration succeeded:

```sh
cmake -S . -B /tmp/fincore-task24-checklist-red.NioYmC/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
```

Its public-only build exited `1` as intended:

```sh
cmake --build /tmp/fincore-task24-checklist-red.NioYmC/build --parallel 4
```

The missing-public-surface errors were:

```text
error: no member named 'roll_sharpe_ratio' in namespace 'fincore'
error: no member named 'roll_max_drawdown' in namespace 'fincore'
error: no member named 'rolling_volatility' in namespace 'fincore'
error: no member named 'rolling_sharpe' in namespace 'fincore'
```

The only production change then added was:

```cpp
#include <fincore/metrics/rolling.hpp>
```

The same preserved directory then built and passed the public regression:

```sh
cmake --build /tmp/fincore-task24-checklist-red.NioYmC/build --parallel 4
ctest --test-dir /tmp/fincore-task24-checklist-red.NioYmC/build \
  -R MetricsUmbrella --output-on-failure
```

`MetricsUmbrella.ExposesOnlyImplementedDataframeNativeMetrics` passed `1/1`.

## Python-oracle and full-index evidence

The fresh full suites include the registered rolling tests
`RollingMetrics.MatchesPythonGoldensWithEndpointAndFullIndexShapes`,
`RollingMetrics.SupportsAnnualizationFrequencyAndNamedColumns`, and
`RollingMetrics.AppliesTheRequiredNanPolicies`. They cover all four accepted
scalar functions, requested names, endpoint/full-index shapes, frequency and
annualization behavior, and the corrected NaN fixture whose completed-window
`roll_sharpe_ratio` result is `0.0`.

## Fresh headless AppleClang verification

`/usr/bin/c++ --version` reported `Apple clang version 17.0.0
(clang-1700.6.3.2)`; CMake identified `AppleClang 17.0.0.17000603`. CMake was
`4.0.2` and Ninja was `1.12.1`. This preserved, independently fresh directory
was created with `mktemp -d`:

```sh
cmake -S . -B /tmp/fincore-task24-core.yrdcIb/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task24-core.yrdcIb/build --parallel 4
ctest --test-dir /tmp/fincore-task24-core.yrdcIb/build --output-on-failure
```

All commands exited `0`; CTest passed `139/139` with no failures.

## Fresh Homebrew LLVM ASan+UBSan verification

`/opt/homebrew/opt/llvm/bin/clang++ --version` reported `Homebrew clang
version 22.1.8`; CMake identified `Clang 22.1.8`. A separate preserved
directory created with `mktemp -d` used these commands:

```sh
cmake -S . -B /tmp/fincore-task24-sanitize.OSfL0y/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build /tmp/fincore-task24-sanitize.OSfL0y/build --parallel 4
ctest --test-dir /tmp/fincore-task24-sanitize.OSfL0y/build --output-on-failure
```

All commands exited `0`; CTest passed `139/139` with no ASan or UBSan
diagnostic. The build emitted one third-party GoogleTest
`-Wcharacter-conversion` warning from `gtest-printers.h`; it is not a
sanitizer finding.

## ImGui/ImPlot dependency boundary

No real Dear ImGui checkout was supplied, fetched, cloned, vendored,
fabricated, or substituted. The independent invalid-source gate used another
fresh preserved directory:

```sh
cmake -S . -B /tmp/fincore-task24-invalid-imgui.wPV832/build -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task24-invalid-imgui.wPV832/not-a-dear-imgui-checkout
```

It exited `1` at `cmake/fincore_implot.cmake:47` with:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task24-invalid-imgui.wPV832/not-a-dear-imgui-checkout'
```

This negative configuration gate is not a positive ImPlot build or demo, and
no positive attempt was made.

## Deferred/open work

The paired rolling `roll_alpha`, `roll_beta`, and capture functions;
multi-column `roll_alpha_beta` and `rolling_regression`; `rolling_beta`;
portfolio positions and transactions; round trips; performance
attribution/stats; Bayesian/bootstrap functions; optimization; simulation;
reports/tearsheets; a full native facade; packaging/release; and a positive
`fincore_implot` / `fincore_implot_demo` build/demo remain separate work.
Phase 3A does not claim full fincore parity or project completion.
