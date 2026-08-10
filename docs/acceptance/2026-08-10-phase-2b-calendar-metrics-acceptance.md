# Phase 2B calendar metrics acceptance

## Decision and bounded scope

Phase 2B is accepted as a C++23, DataFrame-native UTC calendar/yearly metric
slice. It is not acceptance of full fincore parity or of the complete project.

The accepted public additions are:

- `calendar.hpp`: exact signed-nanosecond UTC timestamp construction and year
  extraction.
- `metrics/yearly.hpp`, exposed through `metrics.hpp`: yearly return,
  volatility, Sharpe, drawdown, active-return, and information-ratio result
  DataFrames.
- `annual_alpha` and `annual_beta`, already exposed through the metric façade.

The public umbrella regression directly includes only
`<fincore/calendar.hpp>` and `<fincore/metrics.hpp>`. It constructs the
fixture through `utc_timestamp` and calls `annual_return_by_year`,
`annual_alpha`, and `annual_beta`. `metrics.hpp` gained only
`<fincore/metrics/yearly.hpp>`; it does not include calendar, a detail header,
analysis context, rolling engine, or an ImPlot adapter.

## Input commit chain

Review baseline: `f4fae66d237bf01ca13df4f75abdef5b94f97997`.

| Task | Accepted implementation commit(s) |
| --- | --- |
| Task 15 — UTC calendar helper | `e062ae70628575fe1e71524cc7fd36aa36619869`; boundary-hardening follow-up `70abe149772cebd0a40eb10c82367f9b5aeaeae3` |
| Task 16 — yearly metrics | `b47d7c30c9035b96fde6aa7ca42ef253070bcad9`; alignment-coverage follow-up `466d8d84d85b0474f45737be8e3a711f478ef63e` |
| Task 17 — annual alpha/beta | `f4fae66d237bf01ca13df4f75abdef5b94f97997` |

## Acceptance checklist

- [x] UTC calendar exactness is covered by the fresh full CTest run, including
  epoch/leap-day conversion, default UTC New Year, sub-day nanoseconds, year
  extraction, signed endpoints, and invalid/unrepresentable components.
- [x] The public metrics umbrella regression was RED before `yearly.hpp` was
  included and is GREEN through only the two supported public headers.
- [x] A fresh AppleClang headless core build and full CTest passed.
- [x] A fresh Homebrew LLVM ASan+UBSan build and full CTest passed.
- [ ] Independent positive `fincore_implot` / `fincore_implot_demo` builds
  remain open pending a real local Dear ImGui checkout supplied by the user.

## Public-umbrella RED/GREEN evidence

Before adding the yearly umbrella include, a fresh headless AppleClang build
was configured in `/tmp/fincore-task18-red.aj8rIA`:

```sh
cmake -S . -B /tmp/fincore-task18-red.aj8rIA/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task18-red.aj8rIA/build --parallel 4
```

Configuration completed, and the build exited `1` exactly where expected:

```text
tests/unit/metrics_umbrella_test.cpp:72:22: error: no member named
'annual_return_by_year' in namespace 'fincore'
```

After the minimal `#include <fincore/metrics/yearly.hpp>` addition to
`metrics.hpp`, a distinct fresh AppleClang build in
`/tmp/fincore-task18-green.9Fnuti` configured and built successfully. Its
focused verification:

```sh
ctest --test-dir /tmp/fincore-task18-green.9Fnuti/build \
  -R MetricsUmbrella --output-on-failure
```

exited `0`, with `1/1` `MetricsUmbrella` test passing.

## Fresh headless core verification

The AppleClang compiler was `Apple clang version 17.0.0
(clang-1700.6.3.2)`; CMake identified it as `AppleClang 17.0.0.17000603`.
The following commands used a fresh directory created with `mktemp -d`:

```sh
cmake -S . -B /tmp/fincore-task18-core.rg1AFn/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task18-core.rg1AFn/build --parallel 4
ctest --test-dir /tmp/fincore-task18-core.rg1AFn/build --output-on-failure
```

All commands exited `0`. The fresh full CTest result was `108/108` passing.

## Fresh Homebrew LLVM ASan+UBSan verification

The sanitizer compiler was `/opt/homebrew/opt/llvm/bin/clang++`, reporting
`Homebrew clang version 22.1.8`; CMake identified it as `Clang 22.1.8`. A
separate directory created with `mktemp -d` was used:

```sh
cmake -S . -B /tmp/fincore-task18-sanitize.lEkSlr/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build /tmp/fincore-task18-sanitize.lEkSlr/build --parallel 4
ctest --test-dir /tmp/fincore-task18-sanitize.lEkSlr/build --output-on-failure
```

All commands exited `0`. The fresh full CTest result was `108/108` passing;
no ASan or UBSan diagnostic appeared. Compilation of third-party GoogleTest
emitted one `-Wcharacter-conversion` warning, which is not a sanitizer
diagnostic.

## ImGui/ImPlot dependency boundary

The independent negative configuration check used a new `mktemp -d` directory
and did not create a substitute Dear ImGui tree:

```sh
cmake -S . -B /tmp/fincore-task18-invalid-imgui.wSq1cj/build -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task18-invalid-imgui.wSq1cj/not-a-dear-imgui-checkout
```

It exited `1` at `cmake/fincore_implot.cmake:47` with:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task18-invalid-imgui.wSq1cj/not-a-dear-imgui-checkout'
```

No Dear ImGui dependency was fetched, cloned, vendored, fabricated, or
substituted. This negative diagnostic does not prove a positive adapter build;
positive `fincore_implot` and `fincore_implot_demo` compilation remains open
until a real user-supplied local Dear ImGui checkout is available.

## Deferred work

Weekly/monthly/quarterly aggregation, deep drawdown and consecutive metrics,
positions, transactions, round trips, Bayesian/bootstrap metrics, performance
attribution, and a comprehensive native facade remain deferred. These gates,
the broader planned analytics migration, packaging, release, and the positive
ImPlot/demo build are outside this acceptance decision.
