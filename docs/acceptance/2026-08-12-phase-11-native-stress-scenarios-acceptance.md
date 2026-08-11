# Phase 11 native stress scenarios acceptance

## Accepted boundary and public API

Phase 11 accepts only the deterministic, C++23, DataFrame-native return-stress
subset of Python `fincore.simulation.scenarios`. Task 57 implementation is
`bfe5dc549b080b57be13d62bb2abcd68712add00` (`feat: add native stress
scenarios`); Task 58 exports it through `<fincore/simulation.hpp>`. This is not
full simulation/Python parity, a release, or project completion.

Commit trace: Task 57 implementation is
`bfe5dc549b080b57be13d62bb2abcd68712add00`; Task 58 initial acceptance is
`08d8fd9f1c554a2cf5dbf86b7eb181f8ef3a7921` (`docs: accept phase 11 stress
scenarios`). Task 59 is documentation-only, does not amend Task 58, and neither
lists nor anticipates its own SHA.

The umbrella has exactly this content:

```cpp
#pragma once

#include <fincore/simulation/scenarios.hpp>
```

```cpp
enum class predefined_stress_scenario {
    crash, spike, volatility_crush, volatility_spike,
};

struct custom_stress_scenario {
    std::string name;
    double return_shift{0.0};
    double return_multiplier{1.0};
};

struct stress_scenario_result {
    std::string name;
    dataframe stressed_returns;
    double cumulative_return;
    double max_drawdown;
    double volatility;
    double final_value;
};

std::vector<stress_scenario_result> stress_test(
    const dataframe& returns, std::string_view column = returns_column);
std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::span<const predefined_stress_scenario> scenarios,
    std::span<const custom_stress_scenario> custom_scenarios = {},
    std::string_view column = returns_column);
dataframe scenario_table(std::span<const stress_scenario_result> results);
```

The default overload emits `crash`, `spike`, `vol_crush`, and `vol_spike` in
that order. The explicit overload preserves predefined order then custom order.
Crash/spike replace `size()/2` with `-0.20`/`0.10`; volatility crush/spike
multiply every selected return by `0.5`/`2.0`; custom applies
`(return + return_shift) * return_multiplier`. Results own their names and
frames, and derive `cumulative_return`, `max_drawdown`, and `volatility` from
the accepted scalar metric surface once each; `final_value` is exactly
`1.0 + cumulative_return`.

The selected column must have a nonempty name, be a genuine `double` of exact
raw-index length, and contain at least one row. Input timestamps are neither
sorted nor deduplicated: raw unsorted/duplicate index order is copied
verbatim. Selected NaN throws `fincore::input_error` before output construction
so no timestamp is silently deleted; signed infinity is neither filtered nor
clamped and follows ordinary IEEE arithmetic. Invalid enum/name/duplicate
scenario configuration also throws `input_error`.

`scenario_table` is numeric model data, not Python's percent-formatted pandas
display. Its index is synthetic ordinal `0..N-1`; columns are exactly genuine
`std::string scenario`, then genuine `double cumulative_return`,
`max_drawdown`, `volatility`, and `final_value`, in that order. Empty results
retain this typed five-column schema.

## Python oracle and RED-GREEN evidence

Task 57 ran the required Anaconda oracle from
`/Users/yunjinqi/Documents/new_projects/fincore` using
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`. On
`[.01, -.02, .03, -.01]`, its complete recorded finite cases were:

| Scenario | Stressed returns | cumulative | drawdown | volatility | final |
| --- | --- | ---: | ---: | ---: | ---: |
| `crash` | `[.01, -.02, -.20, -.01]` | `-.2160784` | `-.22384` | `1.5472556349873154` | `.7839216` |
| `spike` | `[.01, -.02, .10, -.01]` | `.0778922` | `-.02` | `.8694826047713664` | `1.0778922` |
| `vol_crush` | `[.005, -.01, .015, -.005]` | `.004824878749999817` | `-.01` | `.1759971590679804` | `1.0048248787499998` |
| `vol_spike` | `[.02, -.04, .06, -.02]` | `.017192960000000035` | `-.04` | `.7039886362719217` | `1.01719296` |
| `shift_then_mult` | `[.03, -.015, .06, 0]` | `.07542300000000002` | `-.015` | `.5279914772039412` | `1.075423` |

Task 57 leaf RED failed only because `<fincore/simulation/scenarios.hpp>` was
missing. Its GREEN passed `7/7` stress tests, `96/96` focused tests, and
`246/246` full configured tests. Its ignored report retains the complete
oracle output and command.

Task 58's `tests/unit/simulation_umbrella_test.cpp` includes only
`<fincore/simulation.hpp>` among fincore headers and calls both `stress_test`
and `scenario_table` with a one-date/two-scenario fixture. Before the umbrella
existed, `cmake --build build --parallel 4` exited `1` at line 1 with only
`fatal error: 'fincore/simulation.hpp' file not found`; filtered CTest found no
executable, so no stale binary was used as RED evidence. After adding the
one-header export, `SimulationUmbrella` GREEN passed `1/1`; the focused
`SimulationUmbrella|StressScenarios|Returns|Drawdown|Ratios|Dataframe` command
passed `97/97`, the configured full suite passed `247/247`, and `git diff
--check` had no output.

## Fresh retained gate evidence

Every directory below is a retained fresh `mktemp -d` directory. Every listed
configure, build, and CTest operation used `set -o pipefail` and
`2>&1 | tee`; links name actual logs, including scan inputs and scan output.

| Gate | Evidence |
| --- | --- |
| AppleClang headless | `/tmp/fincore-phase11-apple.bwnRAm`: [`configure.log`](/tmp/fincore-phase11-apple.bwnRAm/configure.log), [`build.log`](/tmp/fincore-phase11-apple.bwnRAm/build.log), and [`ctest.log`](/tmp/fincore-phase11-apple.bwnRAm/ctest.log) each exited `0`; CTest is `247/247`. CMake and [`compiler-version.log`](/tmp/fincore-phase11-apple.bwnRAm/compiler-version.log) identify AppleClang `17.0.0.17000603`. [`warnings-scan.log`](/tmp/fincore-phase11-apple.bwnRAm/warnings-scan.log) names those exact three input logs and records expected `rg` no-match exit `1`: zero warnings. |
| LLVM 22 ASan+UBSan headless | `/tmp/fincore-phase11-llvm.fdfKVC`: [`configure.log`](/tmp/fincore-phase11-llvm.fdfKVC/configure.log), [`build.log`](/tmp/fincore-phase11-llvm.fdfKVC/build.log), and [`ctest.log`](/tmp/fincore-phase11-llvm.fdfKVC/ctest.log) each exited `0`; CTest is `247/247`. CMake and [`compiler-version.log`](/tmp/fincore-phase11-llvm.fdfKVC/compiler-version.log) identify Homebrew Clang `22.1.8`. [`sanitizer-scan.log`](/tmp/fincore-phase11-llvm.fdfKVC/sanitizer-scan.log) names exactly those three logs, scans `AddressSanitizer|UndefinedBehaviorSanitizer|runtime error`, and records expected zero-match exit `1`: no matching sanitizer diagnostic. [`warnings-scan.log`](/tmp/fincore-phase11-llvm.fdfKVC/warnings-scan.log) retains one third-party GoogleTest `gtest-printers.h:524:35` `-Wcharacter-conversion` warning; it is not a sanitizer result. |
| ImPlot/demo compile-link only | `/tmp/fincore-phase11-implot.oiehnH`: [`configure.log`](/tmp/fincore-phase11-implot.oiehnH/configure.log), [`build.log`](/tmp/fincore-phase11-implot.oiehnH/build.log), and [`ctest.log`](/tmp/fincore-phase11-implot.oiehnH/ctest.log) each exited `0`; CTest is `248/248`, including `ImPlotAdapterCompile`. The build log records `fincore_implot_demo` linking. [`warnings-scan.log`](/tmp/fincore-phase11-implot.oiehnH/warnings-scan.log) retains exactly three macOS OpenGL deprecation warnings for `glViewport`, `glClearColor`, and `glClear`. This is compile/link only: no GUI process was started. |
| Invalid ImGui expected failure | `/tmp/fincore-phase11-invalid-imgui.ltivF4`: [`configure.log`](/tmp/fincore-phase11-invalid-imgui.ltivF4/configure.log) exited `1`, recorded by [`status.log`](/tmp/fincore-phase11-invalid-imgui.ltivF4/status.log). [`diagnostic-scan.log`](/tmp/fincore-phase11-invalid-imgui.ltivF4/diagnostic-scan.log) retains the complete required diagnostic: `FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required files; received '/definitely-invalid-imgui'`. |

## Explicit exclusions

This bounded native slice excludes correlation matrix generation; random
simulations, bootstrap, GBM, LHS, antithetic paths, and simulation VaR/CVaR;
EVT/GARCH; Fama-French/style attribution; providers; reports/tearsheets;
packages/releases and release validation; full Python simulation parity; and
GUI runtime validation. No random engine, matrix dependency, provider, or
reporting dependency was added. The ImPlot result proves compile/link only and
does not imply GUI runtime validation.
