# Phase 14 factor-attribution decomposition acceptance

## Scope and consumed history

This is a bounded acceptance of the deterministic, DataFrame-native,
one-shot factor-attribution decomposition. It accepts the public umbrella
export, literal source-derived component semantics, strict delegated input
contracts, the isolated consumer, and the fresh headless build gates recorded
below. It is not a release, full Python-parity, provider, stateful model, or
GUI-runtime acceptance.

Task 67 consumes the complete Task 66 implementation revision:

- `43c65fd50784988ac41dacff894ee66ab2038259`
  (`feat: add factor attribution decomposition`).

Task 66's ignored report retains the complete Anaconda Python oracle, its
genuine public-leaf RED, and its configured GREEN evidence. Its selected
`.0002` source oracle values are:

```text
alpha             = 0.0005694444444444665
MKT_attribution   = 0.010526060999251869
SMB_attribution   = -0.0031142789575150653
HML_attribution   = 0.0010187735138187518
specific_return   = 0.008430555555555556
common_return     = 0.0088
unexplained       = -0.00020000000000002134
```

Those values came from the local source checkout through exactly
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`, using
`FamaFrenchModel("3factor", risk_free_rate=.0002).attribution_decomposition`
on the fixed eight-row `MKT`/`SMB`/`HML` fixture. Task 66 also proves direct
public-fit coefficient-times-mean parity, caller-order preservation,
risk-free locality, selector/index/type/finiteness/rank errors, ignored
unrelated metadata, and output ownership. Task 67 does not alter the leaf
implementation, solver, or leaf tests.

## Accepted public contract

`<fincore/attribution.hpp>` now exports:

```cpp
fincore::dataframe factor_attribution_decomposition(
    const fincore::dataframe& returns,
    const fincore::dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    double risk_free_rate = 0.0,
    std::string_view returns_column_name = fincore::returns_column);
```

The function produces one independently owned summary `dataframe` with a
synthetic timestamp index `0..K+3`, exactly one genuine `std::string`
`component` column followed by exactly one genuine `double` `contribution`
column, and no other columns. Rows are precisely `alpha`, then caller-ordered
`<factor>_attribution` labels, then `specific_return`, `common_return`, and
`unexplained`.

The source semantics are literal rather than statistical reinterpretations:
each factor contribution is the public fitted beta times that selected factor's
arithmetic raw mean; `specific_return` is the sum of those factor
contributions; `common_return` is the selected return arithmetic mean minus
the supplied risk-free rate; and `unexplained` is
`common_return - alpha - specific_return`. The finite risk-free rate affects
only `common_return` and `unexplained`.

The explicit factor span is nonempty, and every factor name is nonempty,
distinct, free of embedded NUL, and not reserved `alpha`; the return selector
is also nonempty and free of embedded NUL. Selectors are copied before any
DataFrame C-string boundary or public-leaf delegation, so bounded
non-NUL-terminated views are valid. The function invokes public
`fit_factor_regression` exactly once. That delegate remains the authority for
the selected genuine-`double`, finite, owning-length, duplicate-free exact
raw-index, at-least-`K + 1`, and full-rank requirements. Inputs are not
aligned, sorted, dropped, resampled, deduplicated, positional-zipped, or
otherwise repaired.

This is a supplied-data calculation, not a full Fama-French model/provider.
It excludes standard-factor selection/fetch/cache, a stateful model receiver,
prediction, rolling exposures, Newey-West or p-values, residual/idiosyncratic
risk helpers, Brinson or position changes, reports/tearsheets,
packages/releases, external numerical dependencies, full Python dict parity,
and GUI runtime.

## Umbrella-only RED and GREEN

`tests/unit/attribution_factor_attribution_umbrella_test.cpp` includes only
`<fincore/attribution.hpp>` among fincore headers. It includes no
factor-attribution, factor-regression, factor-exposure, metric, or source
header. Its minimal full-rank one-factor frame checks the complete ordinal
index, exact genuine-string/genuine-double two-column schema, labels, and the
literal component identities.

Before the umbrella export existed, the newly registered consumer was built
with:

```sh
cmake --build build --parallel 4
```

It exited `1` solely because the umbrella lacked the factor-attribution public
surface: `factor_attribution_decomposition` and its two public output-label
constants were absent from namespace `fincore`. CTest was deliberately not run
after the compilation failure, so no stale `fincore_tests` executable was used
as RED evidence. The retained log is
`/tmp/fincore-phase14-task67-umbrella-red.M2udnj/build.log`.

After adding exactly
`#include <fincore/attribution/factor_attribution.hpp>` to the attribution
umbrella, the configured build succeeded. The isolated
`AttributionFactorAttributionUmbrella` CTest passed `1/1`; the configured
relevant selection passed `107/107`; and the configured full headless CTest
suite passed `274/274`. The post-GREEN prohibited-leaf include audit had no
match.

## Fresh retained gates

Every fresh command's terminal output was retained with `tee` in its unique
directory. Each positive gate ran configure, build, and CTest before its scan.
No GUI executable was launched. CMake was `4.0.2`.

### AppleClang headless

Fresh directory: `/tmp/fincore-phase14-apple.NGVBku`. `/usr/bin/c++ --version`
reported Apple clang `17.0.0 (clang-1700.6.3.2)`; CMake identified
`AppleClang 17.0.0.17000603` for C and C++.

```sh
cmake -S . -B /tmp/fincore-phase14-apple.NGVBku -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase14-apple.NGVBku --parallel 4
ctest --test-dir /tmp/fincore-phase14-apple.NGVBku --output-on-failure
```

All commands exited `0`; CTest passed `274/274`. Only after CTest, the
`warning:` scan over configure/build/CTest logs completed with `rg` status `1`
(expected no match), recorded in `warnings-scan-final.log`.

### Homebrew LLVM 22 ASan+UBSan headless

Fresh directory: `/tmp/fincore-phase14-llvm.zYUJcS`.
`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake used Clang `22.1.8` for C and C++. The fresh configure
succeeded directly, so no FetchContent/DNS/network failure occurred and no
retry directory was needed.

```sh
cmake -S . -B /tmp/fincore-phase14-llvm.zYUJcS -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase14-llvm.zYUJcS --parallel 4
ctest --test-dir /tmp/fincore-phase14-llvm.zYUJcS --output-on-failure
```

All commands exited `0`; CTest passed `274/274`. After CTest, the retained
sanitizer scan found no `AddressSanitizer`, `UndefinedBehaviorSanitizer`, or
`runtime error` diagnostic (`rg` status `1`, expected no match). The separate
post-CTest warning scan recorded one third-party GoogleTest
`-Wcharacter-conversion` warning at `gtest-printers.h:524` for an implicit
`char8_t` to `char32_t` conversion; it is recorded rather than treated as a
fincore or sanitizer failure.

### ImPlot and demo compile/link boundary

Fresh directory: `/tmp/fincore-phase14-implot.UDzcB3`. AppleClang configured
the local Dear ImGui and ImPlot defaults with `FINCORE_BUILD_IMPLOT=ON` and
`FINCORE_BUILD_IMPLOT_DEMO=ON`.

```sh
cmake -S . -B /tmp/fincore-phase14-implot.UDzcB3 -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-phase14-implot.UDzcB3 --parallel 4
ctest --test-dir /tmp/fincore-phase14-implot.UDzcB3 --output-on-failure
```

All commands exited `0`; the build linked `fincore_implot_demo`, and CTest
passed `275/275`, including `ImPlotAdapterCompile`. The post-CTest warning
scan recorded exactly the three macOS OpenGL deprecation warnings in
`examples/implot_dashboard.cpp` for `glViewport`, `glClearColor`, and
`glClear`. `fincore_implot_demo` is not a CTest registration and was never
launched: this evidence is compile/link plus headless CTest only.

### Invalid ImGui override

Fresh directory: `/tmp/fincore-phase14-invalid-imgui.bgUfym`. The expected
negative configure command exited `1`:

```sh
cmake -S . -B /tmp/fincore-phase14-invalid-imgui.bgUfym -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui
```

The retained required multiline diagnostic is:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received '/definitely-invalid-imgui'
```

This verifies dependency validation only; it does not add GUI-runtime evidence
or weaken the positive default-path ImPlot compile/link result.

## Scope and retained report

The ignored Task 67 record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-14-factor-attribution-decomposition/task-67-report.md`.
It records the source-oracle provenance, RED/GREEN chronology, compiler and
CTest summaries, scans, warnings, exact retained `/tmp` paths, and the fact
that no LLVM retry or GUI launch occurred. The tracked Task 67 scope is limited
to the attribution umbrella, this isolated consumer and CMake registration,
README, and this acceptance record. It does not expand accepted scope to a
provider/model, solver change, interactive visual validation, packaging or
release validation, or full Python API parity.
