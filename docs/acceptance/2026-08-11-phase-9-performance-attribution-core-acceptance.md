# Phase 9 performance attribution core acceptance

## Scope and traceability

This is a bounded acceptance of the deterministic C++23,
cloudQuant/DataFrame-native performance-attribution core. It accepts public
umbrella visibility, typed long-form loading schemas, deterministic arithmetic,
and build/test evidence. It is not a release, full Python-parity, provider, or
GUI-runtime acceptance.

- Task 50 factor exposure metrics:
  `3e6991919ecbe02ad70ab0e8ddd1cb5f5b123571`
  (`feat: add native factor exposure metrics`).
- Task 51 performance-attribution implementation:
  `3e6b1623d39c3913462c236283acefdf00f5a8c7`
  (`feat: add performance attribution core`).
- Task 51 regression hardening:
  `566ce45c41c6e36a549cafb41ee90fd8462da2e8`
  (`test: strengthen performance attribution regressions`).
- Task 52 acceptance change exports only
  `<fincore/metrics/perf_attrib.hpp>` from the existing public
  `<fincore/metrics.hpp>` umbrella. It does not change the attribution
  implementation.

## Public API and schema mapping

| Public API | Native schema / result |
| --- | --- |
| Column constants | `attribution_symbol_column="symbol"`, `attribution_total_returns_column="total_returns"`, `attribution_common_returns_column="common_returns"`, `attribution_specific_returns_column="specific_returns"`, `attribution_tilt_returns_column="tilt_returns"`, `attribution_timing_returns_column="timing_returns"`, and `attribution_factor_column="factor"`. |
| Result records | `perf_attribution_result{dataframe risk_exposures, dataframe attribution}`; `perf_attribution_statistics{std::map<std::string, double> summary, dataframe factor_summary}`. |
| `normalize_attribution_positions` | `positions_in_dollars=true`, `cash_column_name=cash_column`; timestamp index, ordered genuine-`double` assets, optional genuine-`double` cash removal, and per-row absolute-gross dollar normalization. |
| `compute_exposures` | Explicit nonempty ordered `std::span<const std::string_view>` factor selectors; defaults `cash_column_name=cash_column`, `symbol_column_name=attribution_symbol_column`; wide weights plus long-form loadings yield ordered factor `double` columns. |
| `perf_attrib_core` | Accepts already-normalized positions; defaults `returns_column_name=returns_column`, `cash_column_name=cash_column`, and `symbol_column_name=attribution_symbol_column`. |
| `perf_attrib` | Same return/cash/symbol defaults plus `positions_in_dollars=true`; it returns `perf_attribution_result`. `risk_exposures` has factor columns; `attribution` has factor-contribution columns then `total_returns`, `common_returns`, `specific_returns`, `tilt_returns`, `timing_returns`. |
| `create_perf_attrib_stats` | `annualization=daily_annualization`; returns the statistics record whose ordinal-indexed factor summary has `factor` (`std::string`), `Average Risk Factor Exposure`, `Annualized Return`, and `Cumulative Return` (`double`). |
| `cumulative_returns_less_costs` | One-frame `returns_column_name=returns_column` delegates to accepted `cumulative_returns`; two-frame additionally defaults `costs_column_name=returns_column`, requires the exact same index sequence, then subtracts selected cost returns. |

The loading model is intentionally long-form: each loading row has the date as
the DataFrame index, `symbol` as a genuine `std::string` column, and all
selected factor loadings as genuine `double` columns. Positions are wide
timestamp-indexed `double` asset columns. Required `(timestamp, symbol)` keys
are unique; missing keys, invalid types, malformed lengths, invalid factor
names, or nonidentical input index sequences are `fincore::input_error`.
This is a native DataFrame contract, not a fabricated pandas `MultiIndex`.

## Public-only RED then GREEN

Before adding the umbrella include, `tests/unit/metrics_umbrella_test.cpp`
continued to include only `<fincore/calendar.hpp>` and
`<fincore/metrics.hpp>`. It constructs a two-date/two-asset/two-factor fixture
through `fincore::perf_attrib`, asserts market exposure `0.28`, common return
`0.0058`, and the no-cost cumulative result `0.0098`.

The required RED command was run in the existing build directory:

```bash
cmake --build build --parallel 4
ctest --test-dir build -R MetricsUmbrella --output-on-failure
```

The build failed as intended with:

```text
error: no member named 'perf_attrib' in namespace 'fincore'
error: no member named 'cumulative_returns_less_costs' in namespace 'fincore'
```

Because the compile failed before linking a new test executable, the following
CTest invocation could run the prior binary and reported its then-existing
`7/7` tests. It is not treated as RED success evidence; the compiler failure
is the public-consumer RED proof.

Adding the single public include
`#include <fincore/metrics/perf_attrib.hpp>` to `metrics.hpp` made the exact
fixture compile and pass. The focused GREEN command passed `8/8` umbrella
tests, and the existing configured build passed `231/231` CTest tests.

## Numeric oracle evidence

The following complete command was independently rerun from the source Python
checkout. It uses the mandated Anaconda base interpreter and the Phase 9
three-date, two-asset, two-factor long/short dollar-position fixture. It is
kept inline so the evidence does not depend on an ignored report or a transient
terminal log:

```bash
cd /Users/yunjinqi/Documents/new_projects/fincore
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
import pandas as pd
from fincore.metrics.perf_attrib import normalize_and_stack_positions, perf_attrib_core, create_perf_attrib_stats, cumulative_returns_less_costs
pd.set_option("display.precision", 15)
dates = pd.to_datetime(["2024-01-02", "2024-01-03", "2024-01-04"])
returns = pd.Series([.02, -.01, .03], index=dates)
positions = pd.DataFrame({"A": [20., 30., -20.], "B": [50., -10., 20.], "cash": [30., 10., 10.]}, index=dates)
factor_returns = pd.DataFrame({"market": [.01, .02, -.01], "value": [.03, -.02, .04]}, index=dates)
loading_index = pd.MultiIndex.from_tuples([(dates[0], "A"), (dates[0], "B"), (dates[1], "A"), (dates[1], "B"), (dates[2], "A"), (dates[2], "B")], names=["dt", "ticker"])
loadings = pd.DataFrame({"market": [.2, .4, .5, .2, .1, .6], "value": [.4, -.2, .1, .3, .5, -.1]}, index=loading_index)
risk, attribution = perf_attrib_core(returns, normalize_and_stack_positions(positions), factor_returns, loadings)
summary, factor_summary = create_perf_attrib_stats(attribution, risk)
cost_adjusted = cumulative_returns_less_costs(returns, pd.Series([.001, .002, .003], index=dates))
for name, value in (("risk_exposures", risk), ("attribution", attribution), ("summary", summary), ("factor_summary", factor_summary), ("cost_adjusted", cost_adjusted)):
    print(name)
    print(value.to_csv(float_format="%.15g"), end="")
'
```

The complete printed goldens were:

```text
risk_exposures
,market,value
2024-01-02,0.342857142857143,-0.0285714285714286
2024-01-03,0.325,1.38777878078145e-17
2024-01-04,0.25,-0.3
attribution
,market,value,total_returns,common_returns,specific_returns,tilt_returns,timing_returns
2024-01-02,0.00342857142857143,-0.000857142857142858,0.02,0.00257142857142857,0.0174285714285714,-0.000226190476190476,0.00279761904761905
2024-01-03,0.0065,-2.77555756156289e-19,-0.01,0.0065,-0.0165,0.00830952380952381,-0.00180952380952381
2024-01-04,-0.0025,-0.012,0.03,-0.0145,0.0445,-0.00744047619047619,-0.00705952380952381
summary
,
Annualized Specific Return,39.8964850633999
Annualized Common Return,-0.37309539033531
Annualized Total Return,26.1705011834329
Specific Sharpe Ratio,7.86494186919736
Cumulative Specific Return,0.0451695244999999
Cumulative Common Return,-0.00554363521428558
Total Returns,0.0400940000000001
factor_summary
,Average Risk Factor Exposure,Annualized Return,Cumulative Return
market,0.305952380952381,0.861676871127715,0.00742598000000005
value,-0.10952380952381,-0.662479050655964,-0.0128468571428572
cost_adjusted
,0
2024-01-02,0.0189999999999999
2024-01-03,0.006772
2024-01-04,0.0339548439999999
```

These values match the tolerance-bounded C++ assertions in
`PerfAttrib.ProducesCoreAttributionStatisticsAndCostAdjustedReturns`.
Task 51 additionally covers NaN/zero-gross/non-finite arithmetic, typed-empty
and custom schemas, malformed/misaligned inputs, missing/duplicate loadings,
factor collisions, and unsorted-but-identical date sequences. This is numeric
oracle/test evidence only; it does not accept the Python warning-and-drop
alignment layer.

## Fresh build evidence

All roots below were created with `mktemp -d`; no preexisting build directory
was removed. Configure, build, and CTest commands exited `0` unless the
explicitly negative configure states otherwise. The explicitly shown sanitizer
`rg` no-match scan returned `1`, as its lack of matches requires.

### AppleClang headless core

Fresh root: `/tmp/fincore-phase9-apple.aZppsy`.

```bash
cmake -S . -B /tmp/fincore-phase9-apple.aZppsy -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase9-apple.aZppsy --parallel 4
ctest --test-dir /tmp/fincore-phase9-apple.aZppsy --output-on-failure
```

CMake identified AppleClang `17.0.0.17000603`; CTest passed `231/231`.

### Homebrew LLVM 22 ASan/UBSan headless core

The binding brief's aggregate spelling `-DFINCORE_ENABLE_SANITIZERS=ON` was
tried in independent `/tmp/fincore-phase9-llvm.wJmF4B`. CMake emitted the
exact warning `Manually-specified variables were not used by the project:
FINCORE_ENABLE_SANITIZERS`; it therefore is not sanitizer evidence. The project
defines the separate `FINCORE_ENABLE_ASAN` and `FINCORE_ENABLE_UBSAN` options,
so the actual sanitizer gate used this fresh root:
`/tmp/fincore-phase9-llvm-asan.CMMQNA`.

```bash
cmake -S . -B /tmp/fincore-phase9-llvm-asan.CMMQNA -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase9-llvm-asan.CMMQNA --parallel 4
ctest --test-dir /tmp/fincore-phase9-llvm-asan.CMMQNA --output-on-failure
rg -n 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error' /tmp/fincore-phase9-llvm-asan.CMMQNA/configure.log /tmp/fincore-phase9-llvm-asan.CMMQNA/build.log /tmp/fincore-phase9-llvm-asan.CMMQNA/ctest.log
```

`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CTest passed `231/231`. The sanitizer scan had no matches for
`AddressSanitizer`, `UndefinedBehaviorSanitizer`, or `runtime error`. The
evidence-based absence is limited to these logs. The build did emit the
third-party GoogleTest warning:

```text
warning: implicit conversion from 'char8_t' to 'char32_t' may change the meaning of the represented code unit [-Wcharacter-conversion]
```

### Default-local ImPlot/demo compile and link boundary

`FINCORE_BUILD_IMPLOT` and `FINCORE_BUILD_IMPLOT_DEMO` default to `OFF` in this
project. To exercise the requested default *local source paths* rather than a
headless default, the fresh root `/tmp/fincore-phase9-implot.DA613y` explicitly
enabled both options while intentionally omitting `FINCORE_IMGUI_SOURCE_DIR`
and `FINCORE_IMPLOT_SOURCE_DIR`:

```bash
cmake -S . -B /tmp/fincore-phase9-implot.DA613y -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-phase9-implot.DA613y --parallel 4
ctest --test-dir /tmp/fincore-phase9-implot.DA613y -N
ctest --test-dir /tmp/fincore-phase9-implot.DA613y --output-on-failure
```

The build compiled and linked `fincore_implot_demo`. `ctest -N` listed
`ImPlotAdapterCompile`; the full run passed `232/232`. No GUI executable was
launched. The build emitted exactly these macOS OpenGL deprecation warnings:

```text
warning: 'glViewport' is deprecated: first deprecated in macOS 10.14 - OpenGL API deprecated. (Define GL_SILENCE_DEPRECATION to silence these warnings) [-Wdeprecated-declarations]
warning: 'glClearColor' is deprecated: first deprecated in macOS 10.14 - OpenGL API deprecated. (Define GL_SILENCE_DEPRECATION to silence these warnings) [-Wdeprecated-declarations]
warning: 'glClear' is deprecated: first deprecated in macOS 10.14 - OpenGL API deprecated. (Define GL_SILENCE_DEPRECATION to silence these warnings) [-Wdeprecated-declarations]
```

These results are not a no-warning claim.

### Fresh invalid ImGui override

Fresh root: `/tmp/fincore-phase9-invalid-imgui.5RlNC8`.

```bash
cmake -S . -B /tmp/fincore-phase9-invalid-imgui.5RlNC8 -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase9-invalid-imgui.5RlNC8/not-a-dear-imgui-checkout
```

The configure exited `1`, as expected, at `cmake/fincore_implot.cmake:47`:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-phase9-invalid-imgui.5RlNC8/not-a-dear-imgui-checkout'
```

No dependency was fetched, cloned, vendored, fabricated, or substituted.

## Deferred scope

This acceptance does not add Python's warning-producing `align_and_warn`
behavior or its silent date/asset dropping; transaction-turnover diagnostics;
market-data joins or factor-provider adapters; Brinson/style attribution;
Bayesian/bootstrap simulation; reporting/tearsheets; packaging/release work;
or interactive GUI/visual smoke validation. The ImPlot result is only a
compile/link and CTest contract, and Phase 9 is not full fincore/Python parity.
