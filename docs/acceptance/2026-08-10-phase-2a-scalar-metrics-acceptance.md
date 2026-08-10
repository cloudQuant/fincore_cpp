# Phase 2A scalar metrics acceptance

Base: `db3b6c5e4554f9d2f96b646e063cdf281e286b97`

## Accepted native surface

`#include <fincore/metrics.hpp>` is the accepted C++23, DataFrame-native
umbrella for this Phase 2A scalar-metric slice. It includes `fincore/dataframe.hpp`
and these public metric headers only:

- `fincore/metrics/basic.hpp`
- `fincore/metrics/returns.hpp`
- `fincore/metrics/drawdown.hpp`
- `fincore/metrics/ratios.hpp`
- `fincore/metrics/risk.hpp`
- `fincore/metrics/stats.hpp`
- `fincore/metrics/alpha_beta.hpp`
- `fincore/metrics/timing.hpp`

The `MetricsUmbrella` compile/use test reaches the native DataFrame factory and
all eight metric groups, including paired alpha/beta and a timing regression,
without a private header, context object, rolling engine, or ImPlot adapter.

## TDD evidence

### RED

Before `include/fincore/metrics.hpp` existed, after registering
`tests/unit/metrics_umbrella_test.cpp`, ran:

```sh
cmake -S . -B build -DFINCORE_BUILD_IMPLOT=OFF && cmake --build build -j2
```

Result: expected failure (exit 1):

```text
fatal error: 'fincore/metrics.hpp' file not found
```

### GREEN

After adding the public umbrella, ran:

```sh
cmake -S . -B build -DFINCORE_BUILD_IMPLOT=OFF && cmake --build build -j2
ctest --test-dir build -R MetricsUmbrella --output-on-failure
```

Result: `MetricsUmbrella` 1/1 passed.

## Fresh headless verification

### AppleClang core build

Fresh directory: `/tmp/fincore-task14-headless.z8T77h`.

Compiler: `/usr/bin/c++`, AppleClang `17.0.0.17000603`; C++23, Ninja
`1.12.1`.

```sh
cmake -S . -B /tmp/fincore-task14-headless.z8T77h -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task14-headless.z8T77h --parallel 4
ctest --test-dir /tmp/fincore-task14-headless.z8T77h --output-on-failure
```

Result: 85/85 CTest tests passed.

### Homebrew LLVM ASan + UBSan core build

Fresh directory: `/tmp/fincore-task14-sanitize.4h0YKt`.

Compiler: `/opt/homebrew/opt/llvm/bin/clang++`, Homebrew LLVM `22.1.8`; C++23,
Ninja `1.12.1`.

```sh
cmake -S . -B /tmp/fincore-task14-sanitize.4h0YKt -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON \
  -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build /tmp/fincore-task14-sanitize.4h0YKt --parallel 4
ctest --test-dir /tmp/fincore-task14-sanitize.4h0YKt --output-on-failure
```

Result: 85/85 CTest tests passed with no ASan or UBSan diagnostic. The build
printed one non-fatal `-Wcharacter-conversion` warning in fetched GoogleTest's
`gtest-printers.h`; no fincore source warning or sanitizer diagnostic was
reported.

## Optional ImPlot dependency gate

No valid Dear ImGui checkout was supplied, fetched, vendored, or fabricated.
The positive ImPlot adapter build and demo remain open.

The expected invalid-source configure guard was checked in fresh directory
`/tmp/fincore-task14-implot-exit.TF7fii`:

```sh
cmake -S . -B /tmp/fincore-task14-implot-exit.TF7fii -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task14-missing-imgui
```

Result: expected configure failure (exit 1) with the intact diagnostic:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received '/tmp/fincore-task14-missing-imgui'
```

## Deferred beyond this acceptance

This record accepts only the Phase 2A native scalar-metric surface. It does
not claim full Phase 2 or project completion. The following remains deferred:

- calendar/yearly APIs, including `annual_alpha` and `annual_beta`;
- extended drawdown and consecutive-date outputs;
- positions, transactions, and round trips;
- Bayesian, bootstrap, and performance-attribution modules;
- a comprehensive native or Python-style dynamic facade; and
- a positive optional ImPlot build/demo pending a genuine local Dear ImGui
  checkout.
