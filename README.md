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
performance attribution; remaining bootstrap/distribution statistics (the
deterministic `perf_stats` facade is accepted later in Phase 4A); Bayesian
functions; optimization; simulation; reports/tearsheets; a full native
facade; packaging/release; and the then-unaccepted `fincore_implot` /
`fincore_implot_demo` build/demo in
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
and transactions; round trips; performance attribution; remaining
bootstrap/distribution statistics (the deterministic `perf_stats` facade is
accepted later in Phase 4A); Bayesian functions; optimization; simulation;
reports/tearsheets; a full native facade; packaging/release; and the
then-unaccepted `fincore_implot` /
`fincore_implot_demo` build/demo in that dated Phase 3B observation. Its
former positive-ImPlot item is superseded by the Phase 3C compile acceptance
below. Phase 3B is not full fincore parity or project completion. The complete
acceptance evidence is in
[`docs/acceptance/2026-08-10-phase-3b-rolling-paired-metrics-acceptance.md`](docs/acceptance/2026-08-10-phase-3b-rolling-paired-metrics-acceptance.md).

## Accepted Phase 4A performance statistics

The public metric umbrella now exposes the bounded deterministic
`perf_stats` facade:

```cpp
#include <fincore/metrics.hpp>

const auto unary = fincore::perf_stats(returns, fincore::frequency::weekly,
                                        "strategy");
const auto paired = fincore::perf_stats(returns, benchmark,
                                         fincore::frequency::weekly,
                                         "strategy", "market");
```

Both calls return `std::map<std::string, double>` keyed by metric name, not a
synthetic string-indexed timestamp DataFrame. The unary overload has exactly
13 keys: `Annual return`, `Cumulative returns`, `Annual volatility`, `Sharpe
ratio`, `Calmar ratio`, `Stability`, `Max drawdown`, `Omega ratio`, `Sortino
ratio`, `Skew`, `Kurtosis`, `Tail ratio`, and `Daily value at risk`. The
benchmark overload adds exactly `Alpha` and `Beta`, for 15 keys. The requested
return and benchmark double-column names are public arguments, so callers need
not rename native DataFrame columns to the defaults.

The selected `period` applies to annual return, annual volatility, Sharpe,
Calmar, and Sortino. Alpha/Beta remain delegated to daily `alpha_beta`, while
Omega retains its zero-threshold daily delegate behavior. Column validation,
nonfinite behavior, and benchmark handling remain owned by the existing public
scalar delegates; native Alpha/Beta uses common timestamps in the
left/returns-frame order rather than claiming Python outer-shaped pair
behavior.

Task 29's accepted implementation and public-header repair are
`a1caa4bc8bd2496ff67c7728a7368e8214a6a1fd` and
`1cd7cccc9cc5b78c417f8d0452d463357d4d0e4b`; the Task 30 acceptance
publication is `5600dbb2bc2c21a29833d0020e58f8d2154e2a68`. The complete
Phase 4A contract, Python-oracle/delegation coverage, full fresh core and
sanitizer evidence, and optional graphics boundary are recorded in
[`docs/acceptance/2026-08-11-phase-4a-performance-statistics-acceptance.md`](docs/acceptance/2026-08-11-phase-4a-performance-statistics-acceptance.md).

The supplied local ImGui/ImPlot path still has a positive adapter/demo
compile-and-link boundary, but no GUI runtime was launched. Bootstrap and
distribution helpers, positions/transactions/round trips, attribution,
reporting, the broader facade, packaging, and release remain deferred. The
Phase 4B rolling API acceptance below replaces the former deferred
multi-column/full-index rolling claim. This is not a complete fincore-parity
or release claim.

## Accepted Phase 4B multi-column rolling metrics

Phase 4B completes the public function list in Python `metrics/rolling.py` for
the native DataFrame API; it is not whole-project parity. Through the existing
`<fincore/metrics.hpp>` umbrella, `roll_alpha_beta` returns two compact
endpoint columns, exactly `"alpha"` then `"beta"`, and delegates each complete
window to public `alpha_beta`. `rolling_regression` returns the same compact
two-column endpoint shape with a nonannualized regression intercept and beta.

The single-factor `rolling_beta` returns one full aligned index column named
by the requested returns column. The explicit multi-factor overload accepts a
non-empty, ordered list of selected factor names and returns one full aligned
index column for each selected factor in that supplied order. It does not
infer all numeric columns. All native paired alignment retains only common
timestamps in the left/returns supplied order; it does not claim Python
Series outer-shaped pairing. Fixed-width beta/regression windows containing a
nonfinite pair, or a zero-variance factor, produce NaN rather than changing a
scalar/global nonfinite policy.

Task 31 implementation and follow-up coverage are
`9bfedc0ea6c401366009c13e0b44a05dacb37ca9` and
`9e82194c9cca72b7e6755c85d6c7e7c6e47f8848`. Fresh Task 32 public-consumer,
headless, sanitizer, and ImPlot compile-boundary evidence is recorded in
[`docs/acceptance/2026-08-11-phase-4b-multicolumn-rolling-acceptance.md`](docs/acceptance/2026-08-11-phase-4b-multicolumn-rolling-acceptance.md).
Its initial Task 32 acceptance commit is
`7ecb127f248066c0364147b45f0395fd7b5f2397`; this subsequent docs-only
traceability update records that SHA without self-referencing or amending it.

Portfolio positions and transactions; round trips; performance attribution;
bootstrap/distribution statistics; Bayesian and advanced-risk models;
optimization; simulation; reports/tearsheets; a full native facade;
packaging/release; and interactive ImPlot visual smoke testing remain separate
work.

## Accepted Phase 5A numeric position metrics

The DataFrame-native numeric core of Python's position analysis is now
available through the existing public umbrella:

```cpp
#include <fincore/metrics.hpp>

const auto allocation = fincore::get_percent_alloc(positions);
const auto exposure = fincore::get_long_short_pos(positions);
const auto concentration =
    fincore::get_max_median_position_concentration(positions);
const auto leverage = fincore::gross_lev(positions);
```

A positions frame consists of its timestamp index and every genuine `double`
column, in the DataFrame's deterministic insertion order. Non-`double`
metadata is deliberately excluded, and missing or malformed numeric columns
are rejected. `get_percent_alloc` preserves that ordered numeric schema while
normalizing each row; its signed-infinity results become NaN.

`get_long_short_pos` returns a native two-column DataFrame, exactly `long` and
`short`, rather than Python's pair of Series. Its named cash field is optional:
when present it is excluded from exposure, and when absent all numeric columns
participate. In contrast, `gross_lev` and
`get_max_median_position_concentration` require a genuine named `double` cash
field. Gross leverage returns one `gross_lev` column and likewise replaces
signed-infinity results with NaN. Concentration returns exactly `max_long`,
`median_long`, `median_short`, and `max_short`; a side with no eligible holding
is NaN.

Task 33's implementation is
`4d32d076240e12022959d237f2baabe6682f1298` (`feat: add native position
metrics`). Fresh public-consumer, headless, sanitizer, and optional ImPlot
compile-boundary evidence is in
[`docs/acceptance/2026-08-11-phase-5a-position-metrics-acceptance.md`](docs/acceptance/2026-08-11-phase-5a-position-metrics-acceptance.md).

Top holdings; string/SID schemas; position extraction and stacking;
sector/style/cap/volume mappings; transactions, liquidation, round trips, and
reports; performance attribution; bootstrap/distribution statistics; Bayesian
and advanced-risk models; optimization; simulation; reports/tearsheets; a full
native facade; packaging/release; and interactive ImPlot visual smoke testing
remain separate work. This phase is not complete fincore parity or a GUI
runtime result.

## Accepted Phase 5B transaction metrics

The bounded native transaction aggregation and turnover surface is available
through the existing public umbrella:

~~~cpp
#include <fincore/metrics.hpp>

const auto daily = fincore::get_txn_vol(transactions);
const auto actual_gross_book = fincore::get_turnover(
    positions, transactions, fincore::turnover_denominator::actual_gross_book);
const auto portfolio_value = fincore::get_turnover(
    positions, transactions, fincore::turnover_denominator::portfolio_value);
~~~

Raw transaction inputs require distinct genuine double amount and price
columns, both matched to the timestamp index. Duplicate and intraday
timestamps are normalized to UTC midnight and aggregated into sorted daily
rows. The result is exactly the double columns txn_volume and txn_shares:
shares are absolute amounts, while volume is absolute amount times the signed
price. A NaN contribution is skipped; signed infinity remains ordinary
arithmetic in the daily aggregate.

Turnover returns one double column named turnover on the sorted union of
position days and transaction days. Position days must already be strictly
increasing and unique after UTC-day normalization. Actual-gross-book turnover
requires a genuine named double cash column: its first denominator is half of
the first non-cash gross book, and later denominators are the average of the
previous and current gross books. Portfolio-value turnover needs no cash
column and instead divides by each row's all-numeric position sum. In either
mode, a final nonfinite turnover value is emitted as 0.0.

Task 35's implementation and its accepted boundary correction are
87472e42bfa6802e0275af1bf0c3b3058a4f5f23 and
12ec9a0368ea4dd54b6f6669f2118a204947370d. Fresh public-consumer,
headless, sanitizer, and optional ImPlot compile-boundary evidence is in
[docs/acceptance/2026-08-11-phase-5b-transaction-metrics-acceptance.md](docs/acceptance/2026-08-11-phase-5b-transaction-metrics-acceptance.md).

Transaction mapping and symbols/SIDs; daily bar joins; liquidity, liquidation,
and market-impact or other execution-cost models beyond the bounded
basis-point adjustment below; round trips and attribution; performance
attribution; bootstrap/distribution statistics; Bayesian and advanced-risk
models; optimization; simulation; reports/tearsheets; a full native facade;
packaging/release; and interactive ImPlot visual smoke testing remain deferred.
This phase is not full fincore parity or a GUI runtime result.

## Accepted Phase 6A basis-point slippage adjustment

The transaction module now exposes the bounded native basis-point return
adjustment through the existing public umbrella:

~~~cpp
#include <fincore/metrics.hpp>

const auto adjusted = fincore::adjust_returns_for_slippage(
    returns, positions, transactions, 10.0);
~~~

Return and position timestamps are normalized to UTC midnight and must remain
strictly increasing and unique after normalization. Raw transactions retain
their existing unordered, duplicate, and intraday aggregation contract. The
result index is the sorted union of return, position, and transaction UTC days,
without date forward-fill, and the result contains exactly the requested
return column.

The operation computes daily portfolio value from every genuine `double`
position column while skipping NaN values, with no cash-column requirement.
It applies `abs(amount) * price * slippage_bps / 10000` through the established
daily transaction-volume aggregation and reproduces pandas-style one-sided
`fill_value=0` arithmetic for adjusted PnL. Exact zero PnL becomes a NaN
denominator. Only NaN in the final adjusted return is replaced with `0.0`;
computed positive or negative infinity is preserved, and `slippage_bps` is not
validated or clamped.

Task 38's implementation and accepted fill-value correction are
`e49befe52581f719a426e28c4462b2efbfa9e130` and
`b87266bf20eff1e985988d1c8c1bdb443c4d4870`. Fresh public-consumer,
headless, sanitizer, and optional ImPlot compile-boundary evidence is in
[docs/acceptance/2026-08-11-phase-6a-slippage-adjustment-acceptance.md](docs/acceptance/2026-08-11-phase-6a-slippage-adjustment-acceptance.md).

Future market-impact, liquidity, liquidation, and other execution-cost models;
round trips and attribution; advanced analytics; visualization runtime;
packaging/release; and full Python public-API parity remain deferred. Phase 6A
is not a whole-project completion, release, or GUI visual-validation claim.

## Accepted Phase 6B quadratic volume-share slippage penalty

The transaction module also exposes the bounded quadratic penalty surface
through the same public umbrella:

~~~cpp
#include <fincore/metrics.hpp>

const auto adjusted = fincore::apply_slippage_penalty(
    returns, transactions_with_bar_data,
    simulate_starting_capital, backtest_starting_capital, impact);
~~~

`transactions_with_bar_data` is deliberately a prejoined raw transaction and
daily-bar-volume table: it has distinct, genuine `double` `amount`, `price`,
and `volume` fields, each matching its timestamp index. It may contain
unordered, duplicate, and intraday rows, which are normalized and aggregated
by UTC day. This operation does not join transactions to market data.

With `m = simulate_starting_capital / backtest_starting_capital`, each raw row
contributes `((abs(m * amount) / volume)^2) * impact * (price * abs(m * amount))`
to its day. NaN contributions are skipped while ordinary signed infinities
remain IEEE values. Return input needs a requested genuine `double` column and
strictly increasing, unique UTC days. The output has exactly that one return
column and only the normalized return-day index; transaction-only days are not
emitted.

The return path compounds private wealth, treating a NaN return as zero only
for that wealth calculation. Its simulated portfolio value is
`(wealth * backtest_starting_capital) * m`. An absent or NaN daily penalty is
zero; an exact-zero portfolio value makes the adjusted division NaN and so
recovers the original return. Exact zero `backtest_starting_capital` is
rejected, while simulated capital and impact retain ordinary IEEE behavior.

Task 41's implementation and signed-simulation coverage are
`da9387aa80a2586753ad51b4cbb45a8670500829` and
`2c15648f26e7263a08ccfe7bc16e890f38c646d1`. Fresh public-consumer,
headless, sanitizer, and optional compile-boundary evidence is in
[docs/acceptance/2026-08-11-phase-6b-quadratic-slippage-acceptance.md](docs/acceptance/2026-08-11-phase-6b-quadratic-slippage-acceptance.md).

Daily transaction/bar-data joins, symbols/SIDs, liquidity and liquidation
metrics, external market-data providers, alternative impact models, round
trips and attribution, reports/tearsheets, packaging/release, and interactive
ImPlot visual smoke testing remain deferred. Phase 6B is neither full fincore
parity nor a GUI-runtime or whole-project-completion claim.

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
visualizations; positions and transactions; round trips; attribution;
Bayesian/bootstrap and advanced risk models; optimization; simulation; reports;
data providers; packaging and release; and full Python public-API parity.

## Accepted Phase 7 return transformations

The public metric umbrella re-exports the native return-transformation surface;
calendar fixtures use the separately public UTC helper:

```cpp
#include <fincore/calendar.hpp>
#include <fincore/metrics.hpp>

const auto wealth = fincore::cumulative_returns(returns, 100.0, "strategy");
const double ending_wealth =
    fincore::cumulative_return(returns, 100.0, "strategy");
const auto indexed = fincore::normalize(values, 1.0, "equity");
const auto weekly = fincore::aggregate_returns(
    returns, fincore::return_aggregation_period::weekly, "strategy");
```

With a nonzero start value, cumulative transformations return wealth levels;
the legacy zero start preserves return-space output. `normalize` divides by
the first value and scales by its requested start value. Aggregation returns
three columns in this order: integer `year`, integer `period`, then the
requested double return column. Its labels are explicit: weekly aggregation
uses the calendar year of each timestamp together with its ISO week number;
the resulting index is a canonical UTC bucket start. Groups emit
lexicographically by `(year, period)`, while observations within each bucket
retain raw input order. The weekly collision rule is label-derived and
permutation-stable: find the earliest matching day in the label year, take its
ISO Monday, and clamp it to January 1. Thus weekly timestamp indices can be
nonmonotonic even though their `(year, period)` labels are sorted.

Task 44's accepted compatibility implementation is
`feaae2c7fd5e2747c300582d09e76d09de55ace2`
(`fix: stabilize return transformation compatibility`). The public-only
consumer exercises the four transformations through `<fincore/metrics.hpp>`
without including the leaf returns header. Fresh acceptance commands and their
bounded evidence are in
[`docs/acceptance/2026-08-11-phase-7-return-transformations-acceptance.md`](docs/acceptance/2026-08-11-phase-7-return-transformations-acceptance.md).

This is a bounded native slice, not pandas parity or a full-project release:
pandas `MultiIndex` representation, local-timezone bucketing, additional
return-transformation scope, packaging, and release validation remain
deferred. GUI programs were not run as part of this acceptance.

## Accepted Phase 8 round-trip core

The public metric umbrella now exports the deterministic transaction
consolidation and FIFO round-trip extraction surface:

```cpp
#include <fincore/calendar.hpp>  // public UTC timestamp construction
#include <fincore/metrics.hpp>

const auto blocks = fincore::groupby_consecutive_transactions(transactions);
const auto round_trips = fincore::extract_round_trips(transactions);
```

Transactions require genuine `std::string` symbol and `double` amount/price
columns that match their timestamp index. Consolidation stable-sorts by symbol,
timestamp, and original row order; equal-direction executions remain together
at the default eight-hour boundary and use signed VWAP. Extraction then matches
consolidated lots FIFO, preserving partial fills and crossings. Its typed,
ordinal-indexed result contains `pnl`, `open_dt`, `close_dt`, `long`,
`rt_returns`, `symbol`, and nanosecond `duration` columns.

Task 47's accepted implementation and targeted correction are
`4b0046675872f2cd463677647a70776254154921` and
`61caa9c209f050bdd4fac2256707297ffd08fa2e`. Fresh public-consumer,
headless, sanitizer, and optional ImPlot compile/link evidence is in
[docs/acceptance/2026-08-11-phase-8-round-trip-core-acceptance.md](docs/acceptance/2026-08-11-phase-8-round-trip-core-acceptance.md).

Portfolio-value-normalized return joins, synthetic closing transactions,
sector mappings, dynamic summary tables, attribution, reports/tearsheets,
interactive GUI validation, packaging/release validation, and full Python
public-API parity remain deferred. Phase 8 is not a full-project or release
readiness claim.

## Accepted Phase 9 performance attribution core

`<fincore/metrics.hpp>` now re-exports the deterministic DataFrame-native
performance-attribution surface; callers need no leaf-header include:

```cpp
#include <fincore/calendar.hpp>
#include <fincore/metrics.hpp>

const auto result = fincore::perf_attrib(
    returns, positions, factor_returns, factor_loadings);
const auto statistics = fincore::create_perf_attrib_stats(result);
```

| Public operation | Native input/output contract |
| --- | --- |
| `attribution_symbol_column`, `attribution_total_returns_column`, `attribution_common_returns_column`, `attribution_specific_returns_column`, `attribution_tilt_returns_column`, `attribution_timing_returns_column`, `attribution_factor_column` | Public `std::string_view` labels: `symbol`, `total_returns`, `common_returns`, `specific_returns`, `tilt_returns`, `timing_returns`, and `factor`. |
| `normalize_attribution_positions` | `positions_in_dollars=true`, `cash_column_name=cash_column`; removes optional genuine-`double` cash and, for dollars, divides each row by absolute non-cash gross exposure. |
| `compute_exposures` | Takes explicit ordered `std::span<const std::string_view>` factor selectors; `cash_column_name=cash_column`, `symbol_column_name=attribution_symbol_column`; returns ordered factor exposures. |
| `perf_attrib_core` | Accepts already-normalized positions; defaults return/cash/symbol selectors to `returns_column`, `cash_column`, and `attribution_symbol_column`. |
| `perf_attrib` | Has the same return/cash/symbol defaults and `positions_in_dollars=true`; returns `perf_attribution_result{risk_exposures, attribution}`. Attribution orders factor contributions before `total_returns`, `common_returns`, `specific_returns`, `tilt_returns`, and `timing_returns`. |
| `create_perf_attrib_stats` | Returns `perf_attribution_statistics{summary, factor_summary}` with `annualization=daily_annualization`; `summary` is `std::map<std::string, double>` and `factor_summary` is ordinal-indexed with `factor`, average exposure, annualized return, and cumulative return. |
| `cumulative_returns_less_costs` | The one-frame overload defaults `returns_column_name=returns_column`; the two-frame overload additionally defaults `costs_column_name=returns_column` and requires exact-index cost subtraction before cumulative calculation. |

Loadings are a long-form `dataframe`: timestamp index plus a genuine
`std::string` `symbol` column and genuine `double` factor columns. Wide
timestamp-indexed positions use `double` asset-symbol columns; each required
`(timestamp, symbol)` loading key is unique and required. The returned tables
preserve the input returns index and ordered factor columns. This is deliberately
not a synthetic pandas `MultiIndex` API.

The implementation lineage is Task 50 factor exposure metrics
`3e6991919ecbe02ad70ab0e8ddd1cb5f5b123571`, Task 51 performance-attribution
implementation `3e6b1623d39c3913462c236283acefdf00f5a8c7`, and Task 51
regression hardening `566ce45c41c6e36a549cafb41ee90fd8462da2e8`. Post-whole-
phase correction `e886a64cb8705acd5eff9e285233ce349b3b1400` rejects duplicate
position timestamps in both public primitives, empty asset-column names, and
empty loading-symbol values, while preserving valid repeated loading dates for
distinct symbols; its scoped review passed.

The current focused `MetricsUmbrella|PerfAttrib` gate passed `17/17`; the
configured/full suite, fresh AppleClang headless build, and fresh Homebrew LLVM
22 ASan/UBSan headless build each passed `233/233`; the fresh default-local
ImPlot/demo compile-and-link boundary passed `234/234`, including
`ImPlotAdapterCompile`.
The LLVM build emitted the third-party GoogleTest
`-Wcharacter-conversion` warning. The ImPlot demo build emitted macOS OpenGL
deprecation warnings for `glViewport`, `glClearColor`, and `glClear`. These
are warning records, not a warning-free claim; no GUI was launched.

The accepted numeric/oracle boundary and exact fresh-build commands are in
[`docs/acceptance/2026-08-11-phase-9-performance-attribution-core-acceptance.md`](docs/acceptance/2026-08-11-phase-9-performance-attribution-core-acceptance.md).
Warning-producing Python alignment/drop behavior, transaction-turnover
diagnostics, market-data/factor-provider adapters, reports/tearsheets, full
Python API parity, packaging/release, and GUI-runtime validation remain
deferred.

## Accepted Phase 10 Brinson attribution core

`<fincore/attribution.hpp>` exports the bounded, deterministic,
DataFrame-native BHB Brinson core:

```cpp
#include <fincore/attribution.hpp>

const fincore::brinson_period_result period = fincore::brinson_attribution(
    portfolio_returns, benchmark_returns, portfolio_weights,
    benchmark_weights, sectors);
const fincore::dataframe results = fincore::brinson_results(
    portfolio_returns, benchmark_returns, portfolio_weights,
    benchmark_weights, sectors);
const fincore::brinson_cumulative_result cumulative =
    fincore::brinson_cumulative(portfolio_returns, benchmark_returns,
                                 portfolio_weights, benchmark_weights, sectors);
```

`brinson_period_result` owns `allocation`, `selection`, `interaction`,
`total`, `portfolio_return`, and `benchmark_return`. Its cumulative counterpart
owns the first four effects plus `portfolio_cumulative` and
`benchmark_cumulative`. Each operation accepts four `fincore::dataframe`
inputs and a nonempty, distinct `std::span<const std::string_view>` of sector
names. Every selected column must be a genuine `double` on all four frames;
the frames must have duplicate-free, exactly equal raw timestamp sequences.
Nonselected metadata, including numeric metadata, is ignored. No sector is
inferred, and no input is sorted, aligned, normalized, or silently intersected.

`brinson_results` preserves the portfolio-return index and returns exactly six
ordered genuine-`double` columns: `allocation`, `selection`, `interaction`,
`total`, `portfolio_return`, and `benchmark_return`. Single-period attribution
requires exactly one row; valid empty multi-period input returns that typed
empty schema and cumulative attribution returns zero effects and zero
cumulative returns. The accepted implementation is Task 54 commit
`acd4d06000b59b3ed10ba7b51d60bddef8c717dc`.

This acceptance is only the BHB core. It does not include stateful sector
mapping or equal-weight fallbacks, Brinson-Hood, style or Fama-French
attribution, providers, reports/tearsheets, packaging/release validation, full
Python-attribution parity, or GUI runtime. The public-only umbrella consumer,
Anaconda Python oracle, and fresh compiler/build evidence are recorded in
[`docs/acceptance/2026-08-11-phase-10-brinson-attribution-acceptance.md`](docs/acceptance/2026-08-11-phase-10-brinson-attribution-acceptance.md).

## Accepted Phase 12 factor-regression core

`<fincore/attribution.hpp>` also exports the deterministic, no-state,
DataFrame-native factor-regression leaf:

```cpp
#include <fincore/attribution.hpp>

const fincore::factor_regression_result result =
    fincore::fit_factor_regression(returns, factor_returns, factor_names);
```

`factor_regression_result` owns scalar `alpha` and `r_squared`, an
ordinal-indexed `coefficients` table, and return-indexed `fitted_returns` and
`residuals` tables. `coefficients` has genuine `std::string factor` and
genuine `double coefficient` columns with `alpha` first and then the explicit
requested factor names in supplied order; the other two tables each contain
one genuine `double` column, `fitted_returns` or `residuals`, and preserve the
raw returns index. The selected return and factor columns must be genuine,
finite `double` values with owning lengths; factor names must be nonempty,
distinct, and not `alpha`. Both indexes must be duplicate-free and exactly
equal in value and order, the observation count must be at least `K + 1`, and
the intercept-plus-factor design must be full rank. Inputs are neither
aligned, sorted, dropped, nor otherwise repaired. An exactly constant selected
return vector has `r_squared == 0.0` after full input and rank validation.

This is a generic OLS core, not a Fama-French model/provider or a stateful
surface. It excludes standard factor selection/fetching, p-values, standard
errors, Newey-West, weighting/GLS, prediction, rolling exposure, attribution
decomposition, reports/tearsheets, release validation, full Python parity, and
GUI runtime. Any ImPlot evidence is compile/link only; no GUI was launched.
The source-derived Python finite oracle, public-umbrella RED/GREEN, and fresh
AppleClang, LLVM ASan+UBSan, ImPlot, and invalid-ImGui evidence are retained in
[`docs/acceptance/2026-08-12-phase-12-factor-regression-core-acceptance.md`](docs/acceptance/2026-08-12-phase-12-factor-regression-core-acceptance.md).

## Accepted Phase 13 rolling factor exposures

`<fincore/attribution.hpp>` exports the bounded, deterministic, no-state,
DataFrame-native rolling factor-exposure operation:

```cpp
#include <fincore/attribution.hpp>

const fincore::dataframe exposures = fincore::rolling_factor_exposures(
    returns, factor_returns, factor_names,
    /* rolling_window = */ std::nullopt,
    /* returns_column_name = */ fincore::returns_column);
```

An omitted `rolling_window` uses the expanding raw history from row zero through
each endpoint; a positive value uses the trailing raw-row window ending at that
endpoint. `0` is an `input_error`. The independently owned result always keeps
the complete validated raw returns index and has genuine-`double` columns in
insertion order `alpha`, then the caller's factor names. Rows with fewer than
`K + 1` selected observations contain only ordinary `NaN`, including every row
for a valid positive width below `K + 1`; valid empty inputs retain that typed
empty schema. Every eligible raw window delegates to public
`fit_factor_regression`; an eligible rank-deficient window therefore raises
`input_error` rather than yielding a minimum-norm or fit-failure-`NaN` result.

The factor span must be nonempty, contain nonempty distinct names, and must not
contain reserved `alpha`. Selected return/factor columns must exist, be genuine
`double`, have exact owning index lengths, and be finite across the whole input
even when a row would otherwise be in the insufficient-history prefix. The two
raw timestamp indexes must be duplicate-free and exactly equal in length,
value, and supplied order. Inputs are not positional-zipped, aligned on common
timestamps, sorted, resampled, deduplicated, or otherwise repaired; unrelated
metadata is ignored and factor names are never inferred from it.

This is no-state rolling regression over caller-supplied data, not a full
Fama-French model or provider. It excludes a stateful model receiver,
Newey-West/standard errors/p-values, risk-free adjustment, factor
provider/fetch/cache, factor prediction, attribution decomposition,
idiosyncratic risk, generic rolling metrics, external numerical dependencies,
reports/releases, full Python parity, and GUI runtime. It does not change the
Phase 12 solver or duplicate its QR implementation. Any ImPlot evidence is
compile/link only; no GUI executable is launched.

The implementation is Task 63 commit
`fe4319be60052f6963a31e341ffc3fac8803184b`; its post-review request-order
test hardening is `504487779b8c283535c3e66d0dfc1726e81a7c5e`. Historical Task
63 evidence remains distinct: the initial leaf suite/full configured CTest
passed `7/7` and `262/262`, while the hardening revision passed `8/8` and
`263/263`. The source-derived finite Python oracle, public umbrella RED/GREEN,
and current fresh gate evidence are recorded in
[`docs/acceptance/2026-08-12-phase-13-rolling-factor-exposures-acceptance.md`](docs/acceptance/2026-08-12-phase-13-rolling-factor-exposures-acceptance.md).

## Accepted Phase 14 factor-attribution decomposition

`<fincore/attribution.hpp>` now also exports the bounded, deterministic,
one-shot DataFrame-native factor-attribution decomposition:

```cpp
#include <fincore/attribution.hpp>

const fincore::dataframe summary = fincore::factor_attribution_decomposition(
    returns, factor_returns, factor_names,
    /* risk_free_rate = */ 0.0,
    /* returns_column_name = */ fincore::returns_column);
```

The independently owned result has synthetic ordinal index `0..K+3`, exactly
one genuine `std::string` `component` column followed by one genuine `double`
`contribution` column, and rows in this exact order: `alpha`, caller-ordered
`<factor>_attribution` rows, `specific_return`, `common_return`, and
`unexplained`. `specific_return` intentionally means the sum of the factor
contributions; `common_return` is the arithmetic mean of the selected returns
minus only the supplied finite risk-free rate; and `unexplained` is
`common_return - alpha - specific_return`.

The operation owns and NUL-validates all selectors, then invokes the public
factor-regression leaf once. That leaf remains the authority for selected
genuine-double, finite, exact-index, observation-count, and full-rank
contracts; inputs are not aligned, sorted, dropped, or repaired. This is not a
stateful Fama-French model/provider, standard-factor selector, prediction,
rolling calculation, statistical-output suite, external numerical dependency,
full Python dict-parity claim, or GUI-runtime feature. ImPlot evidence is
compile/link only and no GUI executable was launched.

The Task 66 implementation is
`43c65fd50784988ac41dacff894ee66ab2038259`. The source-derived Python oracle,
isolated umbrella RED/GREEN, and fresh AppleClang, LLVM ASan+UBSan, ImPlot, and
invalid-ImGui evidence are retained in
[`docs/acceptance/2026-08-12-phase-14-factor-attribution-decomposition-acceptance.md`](docs/acceptance/2026-08-12-phase-14-factor-attribution-decomposition-acceptance.md).

## Accepted Phase 15 factor-regression predictions

`<fincore/attribution.hpp>` now also exports a deterministic, stateless,
DataFrame-native factor-regression prediction consumer:

```cpp
#include <fincore/attribution.hpp>

const fincore::dataframe predictions = fincore::factor_regression_predictions(
    regression, factor_returns, factor_names);
```

The supplied `fincore::factor_regression_result` is a caller-owned model value,
not mutable model state. Its consumed finite `alpha` must exactly equal row zero
of an exact two-column, ordinal-indexed coefficient table: genuine
`std::string factor` and genuine `double coefficient`, labeled `alpha` then the
explicit requested factors in order. The predictor deliberately does not
consume `r_squared`, fitted returns, or residuals. Selectors are nonempty,
distinct, NUL-free, and cannot be `alpha`; each selected prediction-factor
column must be a finite genuine `double` column with its owning raw-index
length. Unselected metadata is irrelevant.

The independently owned output retains the supplied factor frame's raw index
and row order exactly, including duplicates and nonmonotonic timestamps. It
has exactly one genuine-`double` `predicted_returns` column whose rows are
`alpha + sum(beta[j] * factor[j])` in the caller's requested factor order. A
valid model with a selected typed-empty factor frame yields a typed-empty
output. Nonfinite products, running sums, or results raise `input_error`; no
index matching, sorting, training-history lookup, or refit occurs.

This is the narrow native analogue of consuming Python
`FamaFrenchModel.predict` after a fit, rather than a stateful Python model,
standard-factor selector, provider/cache, QR/OLS implementation, attribution
or rolling calculation, residual/idiosyncratic-risk surface, statistical
output suite, full Python parity, release claim, or GUI-runtime feature.
ImPlot evidence is compile/link only and no GUI executable was launched. The
Task 69 implementation is `539de95ce8d2475dc018e61d331bf56f78c75b27`; source
oracle, isolated umbrella RED/GREEN, and fresh build-gate evidence are retained
in
[`docs/acceptance/2026-08-12-phase-15-factor-regression-prediction-acceptance.md`](docs/acceptance/2026-08-12-phase-15-factor-regression-prediction-acceptance.md).

## Accepted Phase 16 named correlation-breakdown matrix

`<fincore/simulation.hpp>` now also exports a deterministic, named
DataFrame-native adaptation of Python's integer-dimension
`generate_correlation_breakdown` helper:

```cpp
#include <fincore/simulation.hpp>

const std::array<std::string_view, 3U> assets{"equity", "credit", "commodity"};
const fincore::dataframe matrix =
    fincore::generate_correlation_breakdown(assets, 0.25);
```

The native operation accepts explicit asset names so its result can be a named
table rather than Python's anonymous nested list. For `N` names it returns an
independently owned `N x N` `fincore::dataframe`: ordinal timestamp index
`0..N-1`, exactly `N` genuine `double` columns in supplied asset order, and no
extras. The diagonal is exactly `1.0`; every off-diagonal cell is the literal
finite target. An empty name span yields a valid `0x0` result with an empty
index and no data columns.

Names must be nonempty, distinct, and free of embedded NUL; bounded
non-NUL-terminated views are copied safely and remain valid. The target must
be finite. There is intentionally no PSD check, repair, covariance conversion,
or clamping: finite values outside conventional correlation bounds are retained
unchanged off diagonal. No input time-series index, alignment, estimator,
random engine, simulation path, stress scenario, provider, or GUI runtime is
introduced.

The leaf began at Task 72 commit
`1e94efb0aa9365db592fa8845dec9ed3677d541a`; its standards-valid test correction
is `c1ffef2a5276d706d88eba02664f41f1e4b977f4`, which removes only an invalid
oversized `std::span` test probe and does not alter production behavior. The
source oracle, public-header RED/GREEN, isolated umbrella RED/GREEN, and fresh
AppleClang, LLVM ASan+UBSan, ImPlot compile/link, and invalid-ImGui evidence
are retained in
[`docs/acceptance/2026-08-12-phase-16-correlation-breakdown-acceptance.md`](docs/acceptance/2026-08-12-phase-16-correlation-breakdown-acceptance.md).
The ImPlot demo was compiled and linked only; it was never launched.

## Accepted Phase 17 simulation parameter estimation

The simulation umbrella now exports the deterministic, DataFrame-native
calibration counterpart to Python's estimate_parameters helper:

~~~cpp
#include <fincore/simulation.hpp>

const fincore::simulation_parameters parameters =
    fincore::estimate_simulation_parameters(returns);
// parameters.drift and parameters.volatility are owned double scalars.
~~~

The native adaptation accepts one selected, named genuine-double return column
from a fincore dataframe and returns the two values in a self-describing
value struct rather than Python's anonymous array/tuple result. For retained
observations it uses the sample denominator n - 1, annualizes drift by
periods_per_year, and annualizes volatility by sqrt(periods_per_year). The
timestamp index is validated through the established dataframe boundary but
does not change the positional calculation.

The source fixture [0.01, NaN, -0.02, 0.03, 0.0] at 252 periods per year
returns drift 1.2599999999999998 and volatility 0.3304542328371662. As in
Python, NaNs alone are removed in source order. Native code intentionally
rejects positive/negative infinity, fewer than two retained observations,
zero periods per year, malformed or non-double selected columns, and
nonfinite intermediate/final arithmetic (including overflow) with
fincore::input_error. It neither sorts, aligns, resamples, clips, compounds,
nor transforms finite returns.

Task 76's leaf implementation is
6e7ad34e1ce1cd49ea46408aaebad0079283b013. This acceptance is not a random
engine, GBM/path generator, bootstrap or Monte Carlo API, antithetic/LHS
sampler, SimResult replacement, provider, full Python simulation-parity, or
GUI-runtime claim. The retained public-umbrella RED/GREEN, AppleClang,
Homebrew LLVM ASan+UBSan, ImPlot compile/link-only, and invalid-ImGui evidence
is in
[the Phase 17 acceptance record](docs/acceptance/2026-08-12-phase-17-simulation-parameter-estimation-acceptance.md).

## Accepted Phase 18 native GBM path generation

`<fincore/simulation.hpp>` now also exports a reproducible native
Geometric-Brownian-Motion path generator:

~~~cpp
#include <fincore/simulation.hpp>

const fincore::dataframe paths = fincore::geometric_brownian_motion(
    100.0, 0.10, 0.20, 1.0, 1.0 / 252.0, 1U, 42U);
~~~

This is a DataFrame-native transpose of Python's anonymous two-dimensional
path array: rows are ordinal time steps `0..trunc(total_years / time_step)`;
columns are genuine `double` values named `path_0`, `path_1`, and so on in
path-number order. Each column starts at `initial_value`, then applies
`S_next = S_current * exp((mu - 0.5 * sigma^2) * dt + sigma * sqrt(dt) * Z)`.
The zero-volatility Python oracle is therefore shared exactly: with 100.0,
0.1, 0.0, 1.0, and 0.25, each path is 100.0, 102.53151205244289,
105.12710963760242, 107.78841508846317, 110.51709180756478.

An explicit seed makes the native stream repeatable with a local MT19937-64
and Box-Muller transform; it deliberately does not promise bit-for-bit NumPy
`default_rng` samples for the same seed. Omitting the seed uses a local
`random_device` seed. All floating model inputs must be finite; time step is
positive, total years nonnegative, and path count nonzero. Unrepresentable
sizes or nonfinite intermediate/path arithmetic raise `fincore::input_error`.
Finite zero or negative initial values remain valid algebraic inputs.

Task 80's implementation is
`5d9ceddb668568d5d5692cf6e3d03fd239e0177f`. This acceptance does not include
calibration changes, `gbm_from_returns`, bootstrap/Monte Carlo, antithetic or
Latin-hypercube sampling, `SimResult`, correlation/stress changes, provider
or full NumPy RNG parity. ImPlot evidence is compile/link-only; no GUI
executable was launched. Source oracle, public-umbrella RED/GREEN, and fresh
build evidence are retained in
[the Phase 18 acceptance record](docs/acceptance/2026-08-12-phase-18-gbm-path-generation-acceptance.md).

## Accepted Phase 19 GBM paths calibrated from returns

`<fincore/simulation.hpp>` already exports `paths.hpp`, so it also exposes the
accepted `gbm_from_returns` convenience operation without a new umbrella
include:

~~~cpp
#include <fincore/simulation.hpp>

const fincore::dataframe paths = fincore::gbm_from_returns(
    returns, /* horizon = */ 4U, /* path_count = */ 2U,
    /* periods_per_year = */ 4U, /* seed = */ 42U);
~~~

The wrapper first delegates to the accepted estimator, derives its native
source-compatible daily terms `drift / periods_per_year` and
`volatility / sqrt(periods_per_year)`, uses `1 / periods_per_year` as the
physical time step and `horizon / periods_per_year` as duration, then delegates
to native GBM once. It removes only GBM's initial price row and subtracts one,
so named genuine-`double` `path_0` through `path_N` columns keep the delegated
physical-step ordinal index `{1, ..., n_steps}`. The existing double
duration/step transformation is intentionally retained: horizon is not an
exact output-row guarantee. For example, horizon 3 at 5 periods per year has
two rows, while a horizon of zero returns an empty index and exactly
`path_count` empty genuine-`double` path columns.

Task 84 implementation commit
`cb222e867682fa346c462a487155f12c976ba4dd` changed only
`include/fincore/simulation/paths.hpp`, `src/simulation/paths.cpp`, and
`tests/unit/gbm_paths_test.cpp`. Its explicit seed is repeatable for the native
engine only; it does not promise NumPy random-stream equivalence. Existing
estimator validation remains authoritative for selected return data and the
existing GBM validation remains authoritative for path count and path
arithmetic; the wrapper additionally rejects unsafe selector and nonfinite
wrapper-result boundaries.

This is not Monte Carlo or bootstrap infrastructure, antithetic or Latin-
hypercube sampling, a `SimResult` API, a correlation/stress change, a provider,
or a full NumPy-simulation-parity claim. Optional ImPlot evidence is
compile/link-only and no GUI executable was launched. The public-only facade
consumer and fresh compiler/build evidence are retained in
[the Phase 19 acceptance record](docs/acceptance/2026-08-12-phase-19-gbm-from-returns-acceptance.md).

## Accepted Phase 20 bootstrap distribution

`<fincore/simulation.hpp>` now exports the accepted native bootstrap
distribution surface:

~~~cpp
#include <fincore/simulation.hpp>

const fincore::dataframe distribution = fincore::bootstrap_distribution(
    returns, 10000U, fincore::bootstrap_statistic::mean, 42U);
~~~

The operation samples one selected genuine-`double` return column with
replacement and returns an independently owned, one-column genuine-`double`
`bootstrap` table on ordinal index `0..resample_count-1`. It exposes the
source's mean, sample standard deviation, median, Sharpe, Sortino, minimum,
and maximum statistic choices. An explicit seed is repeatable for the native
`std::mt19937_64` implementation only; it intentionally does not claim NumPy
random-stream parity. NaNs are dropped before sampling, while malformed,
empty/all-NaN, infinite, unrepresentable, or allocation-failing input raises
`fincore::input_error`. Valid finite degenerate samples retain the specified
IEEE `NaN`/positive-infinity statistic results.

This acceptance excludes bootstrap confidence intervals and summaries,
callable statistics, Monte Carlo, antithetic/LHS sampling, providers, reports,
packaging/release work, full Python RNG parity, and GUI runtime behavior. The
public-only consumer, genuine umbrella RED/GREEN, compiler/sanitizer,
compile/link-only ImPlot, and invalid-ImGui evidence are retained in
[the Phase 20 acceptance record](docs/acceptance/2026-08-12-phase-20-bootstrap-distribution-acceptance.md).

## Accepted Phase 21 bootstrap confidence interval

`<fincore/simulation.hpp>` already includes
`<fincore/simulation/bootstrap.hpp>`.  Consequently, Task 92 extended the
existing public bootstrap leaf and the façade automatically exports the new
surface; no redundant `simulation.hpp` diff is needed.  The public-only
umbrella consumer includes only `<fincore/simulation.hpp>` and calls
`bootstrap_confidence_interval`, which is the GREEN proof of that boundary.

```cpp
const fincore::bootstrap_confidence_interval_result interval =
    fincore::bootstrap_confidence_interval(
        returns, 10000U, 0.05, fincore::bootstrap_statistic::mean, 42U);
```

Task 92 implementation commit
`f15dbb95eaf1ebb4c0d2f4d4a17958c85c84789d` adds the two-double
`bootstrap_confidence_interval_result{lower, upper}` and
`bootstrap_confidence_interval`.  After rejecting a nonfinite or out-of-range
`alpha`, the function calls public `bootstrap_distribution` exactly once with
the caller's distribution arguments unchanged.  It copies the generated
statistics, rejects an empty or nonfinite generated distribution, sorts the
finite copy, and computes the lower and upper endpoints at `alpha / 2` and
`1 - alpha / 2` with linear percentile position `p * (n - 1)` and adjacent
order-statistic interpolation.  This is the source `percentile` method only;
there is no method selector or callable statistic hook.

The Phase 21 source oracle was run through the mandated Anaconda environment.
It records source results for the nonconstant mean
`(0.004999999999999999, 0.01)`, constant mean `(0.01, 0.01)`, constant
standard deviation `(0.0, 0.0)`, and NaN-drop mean `(0.01, 0.01)`, along with
source `ValueError` for invalid alpha.  Python
raises an empty-percentile `IndexError` at zero count and emits a warning then
returns `(nan, nan)` for the recorded nonfinite statistic case.  Native CI
deliberately rejects both generated-empty and generated-nonfinite distributions
with `fincore::input_error`; that is not full Python nonfinite-percentile
parity.  A native seed is repeatable within the supported C++ build only; no
NumPy random-stream parity is claimed.

The true Task 92 RED registered both consumers before the declaration existed:
the leaf-only consumer failed because the result/function were absent, and the
umbrella-only consumer failed because `bootstrap_confidence_interval` was
absent.  CTest was deliberately not run after that expected compile failure.
After the leaf extension, the configured GREEN passed its focused `8/8`,
prescribed `71/71`, and full `332/332` suites.  Fresh Task 93 headless gates
also passed `332/332` on AppleClang and Homebrew LLVM 22 with ASan+UBSan; the
fresh ImPlot/demo build passed `333/333` including `ImPlotAdapterCompile`.
The ImPlot demo was compiled and linked only—the checked build-root executable
was never launched.  The exact commands, statuses, raw `tee` logs, dependency
provenance, and invalid-ImGui diagnostic scan are retained in
[the Phase 21 acceptance record](docs/acceptance/2026-08-12-phase-21-bootstrap-confidence-interval-acceptance.md).

This acceptance excludes bootstrap summaries, Python method variants and
callable statistics, full Python RNG/nonfinite-percentile parity, Monte Carlo,
antithetic or Latin-hypercube sampling, SimResult, providers, reports,
packaging/release work, and GUI runtime behavior.

## Accepted Phase 22 bootstrap summary

The existing simulation facade already exports the bootstrap leaf, so
bootstrap_summary is available through that path without a redundant include
change. It returns value-owning mean, sample-standard-deviation, Sharpe, and
Sortino summaries; each delegates once to the public bootstrap distribution
with the same input/seed/selector, then derives its ddof=1 standard error and
linear confidence interval from that same finite distribution. The native
boundary validates alpha before count and input, uses retained NaN-filtered
source values, and rejects nonfinite or degenerate summary components rather
than claiming full Python or NumPy-RNG parity.

Fresh AppleClang, LLVM ASan+UBSan, and configured CTest evidence is recorded
in [the Phase 22 acceptance record](docs/acceptance/2026-08-12-phase-22-bootstrap-summary-acceptance.md).
The ImPlot gate is compile/link-only; no GUI is launched.

## Accepted Phase 23 Latin-hypercube sampling

`<fincore/simulation.hpp>` already exposes the paths leaf, including the
accepted named LHS surface:

```cpp
const fincore::dataframe samples = fincore::latin_hypercube_sampling(
    100U, 3U, 42U);
```

It returns an owned ordinal-indexed DataFrame with exactly the genuine-double
columns `lhs_dimension_0..N-1`; all source zero shapes are retained. Every
nonempty dimension contains exactly one finite value in each half-open
stratum `[k/n,(k+1)/n)` and no value outside `[0,1)`. A native explicit seed
repeats within the supported C++ build, but does not claim NumPy stream
equality. The native implementation has no global random state or caller RNG,
and rejects unrepresentable index/name/vector/cell shapes before allocation.

The accepted repair guards the floating strata boundary: unsupported positive
sample counts above double's exact-strata resolution fail before random work,
and generated candidates are redrawn unless their public `floor(value * n)`
classification remains their selected stratum. It does not clip, sort, or
post-process samples. This is not an antithetic/normal/correlated transform,
`MonteCarlo`/`SimResult`, full Python/NumPy parity surface, provider/report,
release, or GUI runtime feature. Fresh AppleClang, LLVM ASan+UBSan, configured
CTest, ImPlot compile/link-only, and invalid-ImGui evidence is recorded in
[the Phase 23 acceptance record](docs/acceptance/2026-08-12-phase-23-latin-hypercube-sampling-acceptance.md).

## Accepted Phase 24 antithetic path transform

The existing simulation façade already includes the paths leaf, so it
automatically exposes the native antithetic transform without a redundant
`simulation.hpp` change:

```cpp
#include <fincore/simulation.hpp>

const fincore::dataframe doubled = fincore::antithetic_variates(paths);
```

For a time-indexed input with genuine-`double` columns `path_0`, `path_1`,
the result owns the exact raw index (including unordered or duplicate
timestamps), then contains `path_0`, `path_1`, `antithetic_path_0`, and
`antithetic_path_1` in that order. It copies the original columns and mirrors
each source value as `2.0 * initial - value`, where `initial` is the first
value of the first source column. This is the established native transpose of
Python's path-major row operation.

The leaf requires a nonempty table whose every data column is a genuine
index-length `double` column. It rejects empty, mixed/non-double, malformed,
derived-name-colliding, or nonrepresentable output shapes with
`fincore::input_error`. It rejects derived names that DataFrame cannot store
without truncation. It deliberately preserves ordinary IEEE NaN, infinity,
and overflow arithmetic—there is no sort, deduplication, finite-value filter,
or clamping.

The Phase 24 source oracle, public-header RED/GREEN, independent review,
configured CTest (`51/51` focused; `359/359` full), AppleClang, LLVM 22
ASan+UBSan, ImPlot compile/link-only, FetchContent provenance, and invalid
ImGui evidence are recorded in
[the Phase 24 acceptance record](docs/acceptance/2026-08-12-phase-24-antithetic-path-transform-acceptance.md).
The ImPlot demo executable was checked only for build-root existence and
executability; it was not launched.

This acceptance does not introduce Monte Carlo or `SimResult`, normal or
correlated transforms, a caller RNG, LHS/GBM/bootstrap changes, providers,
reports, packaging/release work, full NumPy stream parity, or GUI runtime
validation.

## Accepted Phase 11 native stress scenarios

`<fincore/simulation.hpp>` exports only the deterministic, DataFrame-native
return-stress leaf. It declares `enum class predefined_stress_scenario { crash,
spike, volatility_crush, volatility_spike }`,
`custom_stress_scenario{std::string name, double return_shift = 0.0, double
return_multiplier = 1.0}`, and
`stress_scenario_result{std::string name, dataframe stressed_returns, double
cumulative_return, double max_drawdown, double volatility, double final_value}`.
The overloads are `stress_test(const dataframe&, std::string_view column =
returns_column)` and `stress_test(const dataframe&,
std::span<const predefined_stress_scenario>,
std::span<const custom_stress_scenario> = {}, std::string_view column =
returns_column)`, plus `scenario_table(std::span<const stress_scenario_result>)`.

Default order is `crash`, `spike`, `vol_crush`, `vol_spike`; explicit
predefined then custom spans preserve supplied order. Crash/spike replace the
midpoint return with `-0.20`/`0.10`; crush/spike multiply returns by `0.5`/`2.0`;
custom applies `(return + return_shift) * return_multiplier`. The selected
column must be named, genuine `double`, index-length-correct, and nonempty.
Raw unsorted/duplicate timestamp order is preserved. Selected NaN is rejected
before output construction, so timestamps are never removed; signed infinity
uses ordinary IEEE arithmetic. `scenario_table` is numeric model data, not
Python's formatted pandas display: ordinal index `0..N-1`, genuine
`std::string scenario`, then genuine-double `cumulative_return`,
`max_drawdown`, `volatility`, and `final_value`, including typed-empty output.

Task 57 is `bfe5dc549b080b57be13d62bb2abcd68712add00`. Its Anaconda oracle on
`[.01, -.02, .03, -.01]`, public-header RED, and GREEN are recorded in the
acceptance record; Task 58's umbrella consumer passed `1/1`, focused tests
`97/97`, and full configured tests `247/247`.

This is not correlation-matrix support; random simulations/bootstrap/GBM;
EVT/GARCH; Fama-French/style attribution; providers; reports/tearsheets;
release validation; full Python parity; or GUI runtime validation. ImPlot
evidence is compile/link only. See
[`docs/acceptance/2026-08-12-phase-11-native-stress-scenarios-acceptance.md`](docs/acceptance/2026-08-12-phase-11-native-stress-scenarios-acceptance.md).

## Accepted Phase 25 simulation result core

`<fincore/simulation.hpp>` exports a value-owning terminal-path analysis
surface for an already-generated native path table:

```cpp
const fincore::simulation_result result =
    fincore::analyze_simulation_paths(paths);

const auto& statistics = result.statistics();
const double var_95 = result.terminal_value_at_risk(0.05);
const double cvar_95 = result.terminal_conditional_value_at_risk(0.05);
```

Native paths have the established DataFrame orientation: raw index rows are
observations and every named genuine-`double` column is a path. The result
owns a copy of the input table, preserves its raw (including duplicate or
nonmonotonic) index and column order, and analyzes each path's final value.
It exposes mean, sample standard deviation, median, 1/5/25/50/75/95/99 linear
percentiles, terminal VaR, and inclusive-tail terminal CVaR. A singleton has
a `NaN` sample standard deviation; invalid alpha (zero, one, NaN, or infinity)
returns `NaN`. Nonterminal IEEE values remain in the owned table, while a
nonfinite terminal value or structural/non-double/malformed path input raises
`fincore::input_error`.

This is a bounded result value, not Python's mutable `SimResult` or a full
`MonteCarlo` orchestrator. It does not alter GBM, calibration, antithetic,
LHS, bootstrap, stress, random-control, provider, report, packaging, or GUI
runtime behavior. The source oracle, public-facade RED/GREEN, compiler and
sanitizer gates, ImPlot compile/link-only evidence, and invalid-ImGui gate are
recorded in [the Phase 25 acceptance record](docs/acceptance/2026-08-12-phase-25-simulation-result-core-acceptance.md).

## Accepted Phase 26 Monte Carlo simulation composer

`<fincore/simulation.hpp>` exports the accepted stateless, value-owning
historical-return simulation composer:

```cpp
const fincore::simulation_result result = fincore::simulate_monte_carlo(
    returns, 1000U, 252U, false, 42U);
```

It calls native `gbm_from_returns` once using the source-compatible fixed
`252U` annualization, optionally applies `antithetic_variates` once, then calls
`analyze_simulation_paths` once. Its native result uses raw ordinal path rows
and named genuine-`double` path columns; it owns its paths, terminal
statistics, VaR, and CVaR after the input frame changes or is destroyed. An
explicit seed is repeatable for this supported C++ build but does not claim a
NumPy random-stream match. Zero horizon reaches the established empty-path
result invariant and raises `fincore::input_error`.

This narrow composer is not Python's mutable `MonteCarlo`/`SimResult` object
model and adds no class, cache, caller/global RNG, drift/volatility control,
return cleaning, new GBM/result arithmetic, providers, reports,
packaging/release work, or GUI runtime behavior. Its source oracle,
public-facade RED/GREEN, compiler/sanitizer gates, compile/link-only ImPlot
boundary, and invalid-ImGui diagnostic are retained in [the Phase 26
acceptance record](docs/acceptance/2026-08-12-phase-26-monte-carlo-simulation-composer-acceptance.md).

## Accepted Phase 27 Monte Carlo parameter composer

The existing `<fincore/simulation.hpp>` façade now exposes the accepted
stateless counterpart to Python `MonteCarlo.from_parameters`:

```cpp
const fincore::simulation_result result =
    fincore::simulate_monte_carlo_from_parameters(
        0.252, 0.0, 2.0, 2U, 4U, 42U);
```

The composer preserves the source's fixed-252 transformation exactly:
`mu_daily = annualized_drift / 252`,
`sigma_daily = annualized_volatility / sqrt(252)`,
`T = horizon / 252`, and `dt = 1 / 252`. It calls the public raw GBM leaf
once and passes that complete price table to public terminal analysis once.
Unlike the Phase 26 historical-return composer, this result retains the
index-zero initial-price row and does not subtract one. A zero horizon is
therefore a valid one-row result rather than an empty-return-path error.

Raw ordinal rows remain observations and exactly `path_count` named
genuine-`double` `path_N` columns remain paths. The result owns the complete
price table, terminal statistics, VaR, and CVaR. Explicit seeds repeat only
for the supported native `std::mt19937_64` implementation; no NumPy stream
equality is claimed. Raw GBM and result validation remains authoritative:
nonfinite model inputs, zero or unrepresentable path/output counts,
nonrepresentable steps, nonfinite path arithmetic, and nonfinite terminal
statistics raise the inherited `fincore::input_error`. Finite zero/negative
initial values and finite negative volatility retain raw-GBM behavior.

Task 117 comprises implementation `7e328da6716687b8e5596dce82fe539e972b36e3`
and test hardening `64d258029568e5e9292613dc082675151984b8e9`;
its independent re-review concluded **Spec PASS / Task-Quality PASS** with
Critical/Important/Minor `0/0/0`. The public-only façade consumer, Python
source oracle, real leaf RED/GREEN, inherited-error parity, fresh AppleClang,
LLVM ASan+UBSan, compile/link-only ImPlot, FetchContent provenance, and
invalid-ImGui evidence are retained in [the Phase 27 acceptance
record](docs/acceptance/2026-08-12-phase-27-monte-carlo-parameter-composer-acceptance.md).

This narrow surface does not add Python's mutable class lifecycle,
`price_paths`/`simulate` parameter branches, a caller period, antithetic
option, return estimation, stress wrapper, local/global RNG, class/cache,
provider, report, packaging/release work, full Python/NumPy stream parity, or
GUI runtime validation. The ImPlot demo was compiled and linked only and was
never launched.

## Accepted Phase 28 Bayesian posterior return cone

`<fincore/metrics.hpp>` now exports the deterministic, DataFrame-native
posterior return cone:

```cpp
const fincore::dataframe cone = fincore::compute_bayes_cone(
    predicted_returns, 1.0);
```

Its exact function declaration is:

```cpp
[[nodiscard]] fincore::dataframe compute_bayes_cone(
    const fincore::dataframe& predicted_returns,
    double starting_value = 1.0);
```

The direct Task 121 leaf commit is
`7790aa169a843a4837c91936b8ce21fa6a4f8d22`
(`feat: add bayesian posterior return cone`). Its exact five-path scope is
`CMakeLists.txt`, `include/fincore/metrics/bayesian.hpp`,
`src/metrics/bayesian.cpp`, `tests/CMakeLists.txt`, and
`tests/unit/bayesian_cone_test.cpp`. Its independent reviews concluded
**Spec PASS / Task-Quality PASS** with Critical/Important/Minor `0/0/0`.

The native input is deliberately the transpose of Python's
`[posterior_path, forecast_step]` array: a DataFrame is
`[forecast_step, posterior_path]`, with raw rows as steps and every genuine
`double` column as a path. It compounds each path in raw row order and returns
only the owned, genuine-`double` columns `percentile_1`, `percentile_5`,
`percentile_25`, `percentile_50`, `percentile_75`, `percentile_95`, and
`percentile_99`. The result copies the exact raw index, including duplicate or
nonmonotonic labels. An index-empty input with one or more typed-empty double
paths returns those seven typed-empty columns; a pathless, mixed/non-double,
or malformed input is rejected.

Unlike the Python helper's permissive non-finite propagation, the native leaf
requires finite starts, returns, multipliers, cumulative wealth, and
percentiles, reporting `fincore::input_error` at that boundary. Negative or
zero starts and returns at or below `-1.0` remain valid when arithmetic stays
finite. The result is value-owning: it retains no source frame or column view.

The leaf's public-header RED/GREEN and the façade-only RED/GREEN are retained
with source-oracle, compiler, sanitizer, FetchContent, and compile/link-only
ImPlot evidence in [the Phase 28 acceptance
record](docs/acceptance/2026-08-12-phase-28-bayesian-return-cone-acceptance.md).
The Task 121 leaf RED was a missing-header build (with no CTest afterward) and
its final GREEN passed `6/6` focused and `387/387` full CTest. The Task 122
façade RED was a missing-export build (again with no CTest afterward); its
terminal GREEN passed `89/89` relevant and `388/388` full CTest.
The façade consumer includes only `<fincore/metrics.hpp>`; it proves the exact
function/default-call contract, raw-index preservation, the total seven-column
typed schema without hidden non-double columns, and directly calculated
two-path percentile endpoints. This scope does not add PyMC, posterior
sampling, a `Bayesian` class, `compute_consistency_score`, reports, or GUI
runtime behavior. The ImPlot demo was compiled/linked and checked for
build-root existence/executability only; it was never launched.

## Accepted Phase 29 native Style Analysis core

`<fincore/attribution.hpp>` exports the deterministic, DataFrame-native
counterpart to Python `fincore.attribution.style.style_analysis`:

```cpp
const fincore::style_analysis_result result = fincore::style_analysis(
    asset_returns,
    {.market_caps = market_caps,
     .value_scores = value_scores,
     .momentum_window = 5U,
     .size_quantiles = {.3, .7}});
```

The input is a time-by-asset DataFrame: raw rows are observations and every
genuine `double` column is an asset. Optional market-cap, book-to-price, and
value-score spans map positionally to that stable column order; a supplied
span must have one finite value per asset, and value scores take precedence
over book-to-price. The defaults remain momentum window `252` and size
quantiles `{.5,.5}`.

The value-owning result contains three independent frames. `exposures` uses
ordinal asset rows, a genuine string `asset` column, and ordered double style
weights. With caps its style order is `large, mid, small, winner, loser,
value, growth, high, low`; without caps, `equal_weight` replaces the three
size columns. `returns_by_style` uses ordinal rows plus genuine string `style`
and genuine double `return`. `overall_returns` preserves the exact raw input
index—including duplicates and nonmonotonic values—and has one genuine double
`return` column.

Size uses linear percentiles, momentum compounds the last configured window,
value uses a median split, and volatility matches pandas rolling width `60`,
minimum `20`, population standard deviation. A short history can therefore
produce zero-weight `high`/`low` styles whose summary return is `NaN`; input
NaN or infinity is still rejected. Structural, non-double, malformed,
nonfinite, mismatched-characteristic, invalid-quantile, arithmetic, and
allocation boundaries raise `fincore::input_error`.

Task 125 consists of implementation `9ff78501763a9d4090005df018002928aa4fb54f`
and test hardening `9b7afab1efbacda8e2108ee26096e467a124f6bf`.
Independent review concluded **Spec PASS / Task-Quality PASS** with
Critical/Important/Minor `0/0/0`; the hardening locks default semantics,
quantile ties, and 60-row rolling truncation without changing production.
The Python oracle, real leaf/façade RED-GREEN records, current and fresh
compiler/sanitizer gates, FetchContent provenance, and compile/link-only
ImPlot evidence are recorded in [the Phase 29 acceptance
record](docs/acceptance/2026-08-12-phase-29-style-analysis-core-acceptance.md).
The ImPlot demo was checked for build-root existence/executability only and
was never launched.

This phase does not implement style tilts, regression attribution, remote
factor fetching/providers, SciPy optimization, reports, packaging/release
work, or GUI runtime validation. Performance-by-style is accepted separately
in Phase 30 below.

## Accepted Phase 30 native performance by style

`<fincore/attribution.hpp>` now exposes the DataFrame-native counterpart to
Python `fincore.attribution.style.analyze_performance_by_style` through exactly
two value-returning overloads:

```cpp
const fincore::dataframe automatic =
    fincore::analyze_performance_by_style(asset_returns);
const fincore::dataframe explicit_result =
    fincore::analyze_performance_by_style(asset_returns, style_exposures);
```

Explicit exposures use Phase 29's ordinal rows, genuine string `asset`
identity, and ordered genuine-`double` style columns. Assets align by exact
name; missing return assets receive zero weight and extra exposure assets are
ignored. A positive aligned total is normalized while preserving negative
hedge weights; a zero or negative total emits zeros. The automatic overload
calls public `style_analysis` once and delegates.

The value-owning output uses ordinal rows and ordered genuine-double
`{style}_return` columns. Empty-shaped returns or exposures with no rows or no
styles return the pandas-compatible empty `0x0` surface. Nonempty inputs obey
the documented strict schema, finite, identity, name, ordinal, capacity, and
`fincore::input_error` boundary.

Task 129 is commit `b5bdd7b28060ebcc0f4a7b59c1402c4bd970ed98`
with exactly the public leaf header/implementation, CTest registration, and
leaf-only/façade-only tests. Independent review concluded **Spec PASS /
Quality APPROVE**, Critical/Important/Minor `0/0/0`. The exact Python oracle,
genuine dual missing-overload RED, ordinal-index RED, current and fresh
compiler/sanitizer gates, FetchContent provenance, and compile/link-only
ImPlot evidence are recorded in [the Phase 30 acceptance
record](docs/acceptance/2026-08-12-phase-30-style-performance-acceptance.md).
The ImPlot demo was verified as a build-root executable only and was never
launched.

This phase does not add style tilts, regression attribution, factor fetching
or providers, a Python runtime, SciPy or another solver, plots, reports,
packaging/release work, or GUI runtime validation.

## Accepted Phase 31 native rolling style tilts

`<fincore/attribution.hpp>` now exposes the pure C++23/cloudQuant DataFrame
counterpart to Python `fincore.attribution.style.calculate_style_tilts`:

```cpp
const fincore::dataframe tilts =
    fincore::calculate_style_tilts(asset_returns, 252U);
```

The native API deliberately omits Python's unused `factor_returns` argument.
Each output row uses only earlier input positions, copies the trailing raw
timestamp label, and emits genuine-double binary columns in four phase-major
blocks: all asset `large/small` pairs, then `winner/loser`, `value/growth`,
and `high_vol/low_vol`. Duplicate and nonmonotonic raw labels are retained,
but values are written positionally instead of reproducing pandas `.loc`
broadcast/overwrite side effects.

Size and value use ascending one-based average ranks; momentum uses checked
trailing `product(1+r)-1`; volatility uses the last 60 excluded observations,
sample standard deviation (`ddof=1`), and a linear cross-sectional median.
Empty-shaped inputs, windows below two, and histories below three rows return
the canonical `0x0` frame. Nonempty inputs require exact finite genuine-double
columns and safe distinct fixed-width names; the result owns all index, name,
and value storage.

Task 133 is commit `9c4f81e` with exactly the style public header/source,
CTest registration, and leaf-only/façade-only consumers. Independent
specification and task-quality reviews both passed with
Critical/Important/Minor `0/0/0`. Its genuine dual missing-declaration RED,
Anaconda Python oracle, `419/419` headless gates, LLVM ASan+UBSan evidence,
and local ImPlot `420/420` compile/link-only evidence are recorded in [the
Phase 31 acceptance
record](docs/acceptance/2026-08-12-phase-31-style-tilts-acceptance.md). The
ImPlot demo was checked as a build-root executable only and was never run.

This phase does not implement regression attribution, factor providers,
Python embedding, SciPy or another solver, plotting behavior, reports,
packaging/release work, or GUI runtime validation.

## Accepted Phase 32 native style correlation attribution

`<fincore/attribution.hpp>` now exposes the pure C++23/cloudQuant DataFrame
counterpart to Python
`fincore.attribution.style.calculate_regression_attribution` through exactly
two overloads:

```cpp
const fincore::dataframe automatic =
    fincore::calculate_regression_attribution(asset_returns);

const fincore::dataframe explicit_result =
    fincore::calculate_regression_attribution(
        portfolio_returns, style_returns, selected_styles);
```

Despite the source-compatible public name, this is correlation attribution,
not an OLS regression. For each selected style it calculates centered Pearson
correlation against the portfolio and multiplies it by that style's common-row
mean. Fewer than three common rows or either population standard deviation
strictly below `1e-15` produces zero contribution. The residual is the full
portfolio mean minus all style contributions.

The explicit overload row-averages every genuine-double portfolio column,
requires unique timestamps, and aligns common rows in portfolio-left order.
Its copied `selected_styles` span replaces Python's unread exposure values:
missing names are ignored and output remains in style-return column order.
The automatic overload calls the accepted public style-analysis and explicit
performance-by-style APIs once each and pairs their results positionally, so
duplicate or nonmonotonic raw asset-return labels remain valid. Output is an
owned ordinal table with genuine string `style`, genuine double
`contribution`, and `residual` last.

Task 137 consists of implementation `890a4ae` and one-test boundary hardening
`77b3347`. Final independent specification and task-quality reviews both
concluded **PASS**, with Critical/Important/Minor `0/0/0`. The Anaconda source
oracle, genuine dual missing-API RED, `434/434` headless gates, LLVM 22
ASan+UBSan run, local ImPlot `435/435` compile/link-only gate, dependency
provenance, and invalid-ImGui diagnostic are recorded in [the Phase 32
acceptance record](docs/acceptance/2026-08-13-phase-32-style-correlation-attribution-acceptance.md).
The ImPlot demo was compiled and linked as a build-root executable only; it was
never run and no GUI was launched.

This phase does not add an exposure-value regression, OLS or another solver,
factor fetching/providers, Python/pandas embedding, plots or reports,
packaging/release work, or GUI runtime validation.

## Accepted Phase 33 native common-sense ratio

The Phase 32 audit closed every successful local public calculation path in
Python `fincore.attribution.style`: native code now covers style analysis,
performance by style, rolling style tilts, and correlation attribution. The
remaining `StyleResult` helpers are Python container conveniences, while
`fetch_style_factors` is an always-failing provider stub; neither is a missing
local calculation API. Phase 33 therefore leaves the style route and selects
the smallest useful dependency-closed package gap.

`<fincore/metrics.hpp>` now exposes one native symbol through its existing
ratio leaf:

```cpp
const double result = fincore::common_sense_ratio(returns);
const double named = fincore::common_sense_ratio(returns, "strategy");
```

The implementation rejects embedded-NUL selectors before a DataFrame C-string
boundary, validates a genuine-double column before applying the raw two-row
minimum, then calls the accepted public `tail_ratio` and `win_rate` exactly
once each. A NaN delegate or zero win rate returns NaN; a win rate of one
returns positive infinity; every other result is the direct IEEE expression
`tail_ratio * win_rate / (1 - win_rate)` without a finite clamp.

This deliberately composes the accepted native finite-filter policy. Python
drops NaN but retains signed infinity, so its percentile path returns NaN for
the recorded infinity fixtures. Native `tail_ratio` and `win_rate` drop all
nonfinite observations, and both signed-infinity fixtures therefore return
`3.294117647058822`. Tests also lock the source-visible raw-count distinction,
an independently undefined tail with nonzero win rate, and a legitimate
positive-infinite public tail with a `0.05` win rate so the final result cannot
be normalized or clamped.

Task 141 consists of implementation `ae9f49d` and test-only boundary hardening
`759af93`. Final independent specification and task-quality reviews both
concluded **PASS**, with Critical/Important/Minor `0/0/0`. The mandatory
Anaconda source oracle, genuine dual missing-symbol RED plus transparent
exact-base evidence recovery, `439/439` headless gates, LLVM 22 ASan+UBSan
run, local ImPlot `440/440` compile/link-only gate, dependency provenance, and
invalid-ImGui diagnostic are recorded in [the Phase 33 acceptance
record](docs/acceptance/2026-08-13-phase-33-common-sense-ratio-acceptance.md).
The ImPlot demo was checked as a build-root executable only; it was never run
and no GUI was launched.

This phase does not implement `mar_ratio`, `var_excess_return`, conditional
ratios, weekly/monthly consecutive statistics, EVT, provider or solver paths,
aliases, plots, reports, packaging/release work, or GUI runtime validation.

## Accepted Phase 34 native VaR excess return

The Phase 34 route audit confirmed that the apparently smaller `loss_rate`
metric is already declared, implemented, facade-exported, and tested. The
smallest useful dependency-closed package gap was therefore Python
`fincore.metrics.risk.var_excess_return`, whose successful path composes three
already accepted public native leaves.

`<fincore/metrics.hpp>` now exposes exactly one native overload through its
existing risk leaf:

```cpp
const double result = fincore::var_excess_return(returns);
const double weekly = fincore::var_excess_return(
    returns, 0.05, 0.0, fincore::frequency::weekly);
const double custom = fincore::var_excess_return(
    returns, 0.05, 0.01, fincore::frequency::daily, 7.0, "strategy");
```

The implementation rejects an embedded-NUL selector before any DataFrame
C-string boundary, validates a genuine-double selected column before the raw
two-row shortcut, then calls public `annualization_factor`, `annual_return`,
and `historical_value_at_risk` exactly once each. Zero or NaN VaR and NaN
annual return produce NaN; every other result is the direct IEEE expression
`(annualized_return - risk_free) / abs(value_at_risk)` without finite
clamping.

Cutoff endpoints `0` and `1` remain accepted. Unknown frequency is observed
before invalid cutoff unless an explicit annualization override bypasses the
frequency lookup. Risk-free NaN/infinity and legitimate infinite results
retain direct IEEE behavior. Explicit annualization follows the accepted
native annual-return leaf: its `0` and signed-infinity behavior deliberately
differs from Python's intermediate division, while finite source goldens and
the `-3`/NaN cases agree.

Task 145 consists of implementation `56c821b` and the test-only zero-VaR
mutation hardening `39e6dc3`. Final independent specification and task-quality
reviews both concluded **PASS**, with Critical/Important/Minor `0/0/0`. The
mandatory Anaconda source oracle, genuine dual-consumer missing-symbol RED,
`449/449` headless gates, LLVM 22 ASan+UBSan run, local ImPlot compile/link-only
gate, dependency provenance, and invalid-ImGui diagnostic are recorded in
[the Phase 34 acceptance
record](docs/acceptance/2026-08-13-phase-34-var-excess-return-acceptance.md).
The ImPlot demo was compiled and linked as a build-root executable only; it was
never run and no GUI was launched.

This phase does not implement `mar_ratio`, adjusted or conditional Sharpe,
weekly/monthly consecutive statistics, EVT, aliases, providers, plots,
reports, packaging/release work, or GUI runtime validation.

## Accepted Phase 35 native trading value at risk

The Phase 35 route audit first considered `mar_ratio`, then selected the
smaller dependency-closed public gap
`fincore.metrics.risk.trading_value_at_risk`. The apparently smaller
`loss_rate` was again confirmed already declared, implemented,
facade-exported, and tested.

`<fincore/metrics.hpp>` exposes exactly one new native overload through its
existing risk leaf:

```cpp
const double result = fincore::trading_value_at_risk(returns);
const double named =
    fincore::trading_value_at_risk(returns, 1.0, "strategy");
```

Python's `period` parameter is completely unused and is deliberately omitted
from the native API. The implementation rejects an embedded-NUL selector
before any DataFrame C-string boundary, validates the selected genuine-double
column before the raw two-row shortcut, computes the raw arithmetic mean and
sample standard deviation, then returns `mean - sigma * standard_deviation`
directly. It performs no filtering, annualization, sigma validation,
zero-deviation special case, or nonfinite clamp.

Consequently NaN and raw signed infinity are not cleaned; zero multiplied by
signed infinity remains NaN; negative, zero, NaN, and signed-infinite `sigma`
values retain direct IEEE behavior; and legitimate final signed infinity is
preserved. Native overflow, cancellation, and reduction order come from the
accepted `detail::mean` and `detail::sample_variance` helpers rather than a
private NumPy-compatible reduction.

Task 148 design/plan commit `a9d695e` and Task 149 implementation commit
`e52d288` form a direct two- then four-path chain. Final independent
specification and task-quality reviews both concluded **PASS**, with
Critical/Important/Minor `0/0/0`. The mandatory 19-case Anaconda source
oracle, genuine dual-consumer missing-symbol RED, current `457/457` headless
gates, and fresh compiler/sanitizer/graphics/provenance evidence are recorded
in [the Phase 35 acceptance
record](docs/acceptance/2026-08-13-phase-35-trading-value-at-risk-acceptance.md).
The ImPlot gate is compile/link plus adapter-test evidence only; its demo was
not run and no GUI was launched.

This phase does not implement `mar_ratio`, `r_cubed`, `kappa_three_ratio`,
adjusted or conditional Sharpe, calendar-resampled consecutive statistics,
EVT, aliases, providers, reports, plots, packaging/release work, Python
container adapters, or GUI runtime validation.
