# Phase 3B rolling paired metrics acceptance

## Decision and bounded scope

Phase 3B is accepted as the public, paired rolling-metrics slice of the C++23,
DataFrame-native API. It publishes exactly five one-column endpoint functions
through `<fincore/metrics.hpp>` (already exposed via `metrics/rolling.hpp` from
Phase 3A):

- `roll_alpha` resolves its annualization once and delegates every completed
  aligned window to `alpha`.
- `roll_beta` forwards `risk_free` to `beta` and intentionally ignores `period`
  and `annualization`, matching the accepted scalar beta path.
- `roll_up_capture`, `roll_down_capture`, and `roll_up_down_capture` resolve
  their period-derived annualization once and delegate to the corresponding
  scalar capture function.

Each result is a one-column double series named by the requested returns
column, indexed at the final timestamp of every complete aligned window. The
two inputs are aligned exactly once through `align_on_common_index`, retaining
common timestamps in the left/strategy frame's supplied order. This phase adds
no two-column `roll_alpha_beta`, no `rolling_regression`, no full-index
`rolling_beta`, no multi-factor DataFrame, and no change to `rolling_engine`.

## Input commit chain

| Task | Accepted implementation commit |
| --- | --- |
| Task 25 - paired rolling metrics | `7c779a131435960d4bb215762970fe37a1db500a` (`feat: add paired rolling metrics`) |
| Task 25 follow-up - annualization validation ordering | `091f9173433468c7ee70dc564ace2e5fcd0e52f2` (`fix: validate paired rolling annualization`) |
| Task 26 - public umbrella consumer regression | `a0c130d815cb1d18f85e1c01b73cfc8e6d59d32a` (`test: cover paired rolling metrics in umbrella`) |

The Phase 3B plan was authored at `b7a85ba9c8724935e305b623302a9219b9b19903`
and clarified at `1c90cab1dd5bc447e8e13cf8b1c56b0236ff3765`.

## Acceptance checklist

- [x] A public-only `MetricsUmbrella` consumer regression calls all five paired
  rolling APIs through `<fincore/metrics.hpp>` and asserts the endpoint index
  `{3}`, the requested `strategy` column name, and a single double column.
- [x] Registered Python-oracle coverage pins all five endpoint vectors for the
  six-observation daily fixture at window `3`, plus left-order alignment,
  empty/short/no-common-index behavior, window-one and zero validation, custom
  columns, custom alpha annualization/risk-free inputs, beta's ignored
  annualization, and a NaN fixture compared directly against the public scalar
  window delegate.
- [x] A fresh headless AppleClang CTest run passes all 146 registered tests.
- [x] A fresh Homebrew LLVM ASan+UBSan CTest run passes all 146 registered
  tests with no sanitizer diagnostic.
- [x] A fresh invalid-Dear-ImGui configuration preserves its documented precise
  diagnostic; a positive ImPlot build remains deferred.

## Public umbrella RED/GREEN evidence

`tests/unit/metrics_umbrella_test.cpp` includes only public
`<fincore/metrics.hpp>` (and its existing public `<fincore/calendar.hpp>` UTC
fixture). Phase 3A had already added `metrics/rolling.hpp` to the umbrella, so
the paired RED was the genuine public-header absence recorded during Task 25:
before `7c779a1`, the five declarations were absent from the public surface, so
a public-only consumer failed to compile with `no member named` errors for
`roll_alpha`, `roll_beta`, `roll_up_capture`, `roll_down_capture`, and
`roll_up_down_capture` in namespace `fincore`.

`7c779a1` added exactly those five declarations and the private paired-endpoint
helpers. The Task 26 GREEN extended `MetricsUmbrella` to call all five APIs on
the `{1, 2, 3}` `strategy`/`market` pair at window `3` and assert, for each
paired endpoint, the index `{3}`, the `strategy` column name, and a single
double column (`get_columns_info<double>().size() == 1`). The accepted
umbrella regression is `a0c130d815cb1d18f85e1c01b73cfc8e6d59d32a`.

## Python-oracle and endpoint evidence

The registered rolling tests cover the six-observation daily fixture (UTC days
2021-01-01 through 2021-01-06, returns `[.01, -.02, .03, 0, -.01, .02]`,
benchmark `[.005, -.01, .02, .01, -.02, .01]`, window `3`). The endpoint index
is `{3, 4, 5, 6}` and the oracle vectors are:

- alpha: `[-.3431833620809164, -.8357653939371649, 1.6310200676280466,
  1.313133510455331]`
- beta: `[1.6666666666666659, 1.571428571428571, .8461538461538458,
  .6666666666666666]`
- up capture: `[6.637108874074204, .9752504552841719, .9752504552841719,
  .9866347325869022]`
- down capture: `[1.079619499803848, 1.079619499803848, .9262522584870747,
  .9262522584870747]`
- up/down capture: `[6.147637084435837, .903327936797512, 1.0528994087173726,
  1.0651900964845744]`

Additional regressions pin left-order common-index alignment (including a
mismatched/unsorted left timestamp fixture), empty genuine double columns,
no-common-index frames, short and exactly-one-window frames, `window == 0`
rejection, custom column names, custom alpha `risk_free`/`annualization`, beta's
intentionally ignored `period`/`annualization`, capture `period` behavior, all
missing/wrong-typed/short-column input errors on either side, and a NaN fixture
compared directly against the corresponding public scalar window delegate
rather than a new rolling filter.

## Fresh headless AppleClang verification

`/usr/bin/c++ --version` reported `Apple clang version 17.0.0
(clang-1700.6.3.2)`; CMake identified `AppleClang 17.0.0.17000603`. CMake was
`4.3.2` and Ninja was `1.13.0`. This preserved, independently fresh directory
was created with `mktemp -d`:

```sh
cmake -S . -B /tmp/fincore-task26-core.VfIPiv/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task26-core.VfIPiv/build --parallel 4
ctest --test-dir /tmp/fincore-task26-core.VfIPiv/build --output-on-failure
```

All commands exited `0`; CTest passed `146/146` with no failures.

## Fresh Homebrew LLVM ASan+UBSan verification

`/opt/homebrew/opt/llvm/bin/clang++ --version` reported `Homebrew clang
version 22.1.8`; CMake identified `Clang 22.1.8`. A separate preserved
directory created with `mktemp -d` used these commands:

```sh
cmake -S . -B /tmp/fincore-task26-sanitize.TjdgSE/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build /tmp/fincore-task26-sanitize.TjdgSE/build --parallel 4
ctest --test-dir /tmp/fincore-task26-sanitize.TjdgSE/build --output-on-failure
```

All commands exited `0`; CTest passed `146/146` with no ASan or UBSan
diagnostic. The build emitted one third-party GoogleTest
`-Wcharacter-conversion` warning from `gtest-printers.h`; it is not a
sanitizer finding.

## ImGui/ImPlot dependency boundary

No real Dear ImGui checkout was supplied, fetched, cloned, vendored,
fabricated, or substituted. The independent invalid-source gate used another
fresh preserved directory:

```sh
cmake -S . -B /tmp/fincore-task26-invalid-imgui.cqhcsc/build -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task26-invalid-imgui.cqhcsc/not-a-dear-imgui-checkout
```

It exited `1` at `cmake/fincore_implot.cmake:47` with:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task26-invalid-imgui.cqhcsc/not-a-dear-imgui-checkout'
```

This negative configuration gate is not a positive ImPlot build or demo, and
no positive attempt was made.

## Deferred/open work

Multi-column `roll_alpha_beta` and `rolling_regression`; full-index
`rolling_beta` (including multi-factor inputs); portfolio positions and
transactions; round trips; performance attribution/stats; Bayesian/bootstrap
functions; optimization; simulation; reports/tearsheets; a full native facade;
packaging/release; and a positive `fincore_implot` / `fincore_implot_demo`
build/demo remain separate work. Phase 3B does not claim full fincore parity or
project completion.
