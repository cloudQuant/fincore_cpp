# Phase 12 factor-regression core acceptance

## Accepted boundary

Phase 12 accepts the bounded C++23, deterministic, DataFrame-native ordinary
least-squares factor-regression core implemented by Task 60 commits
`ff3a0defdbb4cee9419f0b8dc4aec3d3181ae66f` (`feat: add native factor
regression`) and `c18a87010218324c43b773e11bd0e8340f68a7e6` (`fix: preserve
zero r squared for constant returns`). This Task 61 acceptance exports that
existing leaf through `<fincore/attribution.hpp>`; it does not make a release
or assert full Python Fama-French parity.

The sole entry point is:

```cpp
factor_regression_result fit_factor_regression(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::string_view returns_column_name = returns_column);
```

It returns `factor_regression_result{alpha, r_squared, coefficients,
fitted_returns, residuals}`. `coefficients` has synthetic ordinal timestamp
index `0..K`, exactly one genuine `std::string` `factor` column and one genuine
`double` `coefficient` column. Its rows are `alpha` first and then the caller's
requested factors in exact supplied order. `fitted_returns` and `residuals`
each preserve the raw returns timestamp index and contain exactly one genuine
`double` column named `fitted_returns` or `residuals`, respectively. The
scalar `alpha` duplicates coefficient row zero; `r_squared` is the population
coefficient of determination.

The factor-name span is nonempty, has no empty or duplicate name, and cannot
contain reserved `alpha`. The selected return and factor columns must exist,
be genuine `double`, and match their owning indexes; unrelated malformed
metadata is ignored. Both raw timestamp indexes must be duplicate-free and
exactly equal in length, values, and order. No input is positional-zipped,
aligned, sorted, intersected, or dropped. All selected values must be finite:
NaN and either signed infinity throw `fincore::input_error`. The core requires
at least `K + 1` observations and a full-rank intercept-plus-factor design;
rank deficiency under the Householder-QR scale-relative tolerance throws
`input_error` rather than yielding a minimum-norm solution. Exact fits are
valid. An exactly constant selected return vector yields `r_squared == 0.0`
only after the same finite and rank validations.

## Source-derived finite oracle and public-header TDD

The source Python model was run from
`/Users/yunjinqi/Documents/new_projects/fincore` with exactly
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python` and
`FamaFrenchModel("3factor").fit(..., newey_west_lags=0)`. On the retained
eight-observation MKT/SMB/HML fixture it returned alpha
`0.0005694444444444665`; betas MKT `1.5310634180729992`, SMB
`-0.45298603018400957`, and HML `0.23286251744428613`; R-squared
`0.9999467109765429`; and residuals
`[-0.0003203580214942679, -0.00007556613003010387,
0.00014016753708259722, 0.00034876163551868407,
-0.00010695865164660895, 0.00020029277626710043,
0.00010131173838608121, -0.00028765088408364426]`. The complete current-run
output is retained in the ignored Task 61 report evidence.

Before adding the one new umbrella include, the newly registered independent
consumer included only `<fincore/attribution.hpp>` among fincore headers and
constructed a full-rank one-factor frame. `cmake --build build --parallel 4`
exited `1` at
`tests/unit/attribution_factor_regression_umbrella_test.cpp`: the umbrella
had no `fincore::fit_factor_regression` or factor-regression public symbols.
The follow-on exact filtered CTest exited `0` with `No tests were found!!!`,
so no stale executable was accepted as RED evidence. Adding exactly
`#include <fincore/attribution/factor_regression.hpp>` next to Brinson made
the direct umbrella GREEN command pass `1/1`.

The configured focused command
`ctest --test-dir build -R 'AttributionFactorRegressionUmbrella|FactorRegression|Brinson|Dataframe|Returns|PerfAttrib' --output-on-failure`
passed `87/87`; the configured full command passed `254/254`. The consumer
calls the scalar fields, string coefficient table, fitted table, and residual
table, while including neither the leaf nor any Brinson, metrics, or private
source header.

## Fresh retained gate evidence

Every successful configure, build, and CTest command below was executed with
`2>&1 | tee` into the named retained log. Scanner commands were sequenced only
after the matching CTest command. The retained order record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-12-factor-regression-core/task-61-gate-order.log`.

| Gate | Retained fresh directory and result |
| --- | --- |
| AppleClang headless | `/tmp/fincore-phase12-apple.rOKpYH`: Debug `FINCORE_BUILD_IMPLOT=OFF` configure, build, and CTest all exited `0`; CTest passed `254/254`. CMake identifies AppleClang `17.0.0.17000603`; `compiler-version.log` records Apple clang `17.0.0 (clang-1700.6.3.2)`. `warnings-scan-final.log` scans configure/build/CTest only after CTest and records `rg` exit `1` (zero `warning:` matches). |
| Homebrew LLVM ASan+UBSan headless | The first distinct fresh directory `/tmp/fincore-phase12-llvm.yPtdJo` configured with Clang `22.1.8` but exited nonzero during FetchContent because GitHub DNS could not resolve `github.com`; it is retained, not deleted, and is environmental failure evidence rather than acceptance proof. The non-overlapping retry `/tmp/fincore-phase12-llvm.opkGLQ` configured with `/opt/homebrew/opt/llvm/bin/clang++`, ASan and UBSan enabled, and `FINCORE_BUILD_IMPLOT=OFF`; configure/build/CTest all exited `0`, with `254/254` passed. `compiler-version.log` records Homebrew clang `22.1.8`. The post-CTest sanitizer scanner records expected no-match `rg` exit `1` for `AddressSanitizer`, `UndefinedBehaviorSanitizer`, and `runtime error`. |
| ImPlot/demo compile-link only | `/tmp/fincore-phase12-implot.cPmHP5`: Debug configure with `FINCORE_BUILD_IMPLOT=ON` and `FINCORE_BUILD_IMPLOT_DEMO=ON`, build, and CTest all exited `0`; CTest passed `255/255`, including `ImPlotAdapterCompile`, and the build links `fincore_implot_demo`. This is compile/link evidence only: no GUI executable was launched. |
| Invalid ImGui negative configuration | `/tmp/fincore-phase12-invalid-imgui.eooeH2`: configure with `FINCORE_BUILD_IMPLOT=ON` and `FINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui` exited nonzero as required. `configure.log` retains the full CMake error at `cmake/fincore_implot.cmake:47`: `FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required` followed by `files; received '/definitely-invalid-imgui'`. The prior literal single-line `rg` exited `1` because CMake physically wrapped that phrase; this is retained in `task-61-invalid-literal-rg.log` and is not a false GREEN. The whitespace-tolerant multiline scan in `whitespace-tolerant-diagnostic-scan.log` verified the complete diagnostic. |

Apple warning evidence contains no matches. The successful LLVM warning scan
contains exactly one downloaded third-party GoogleTest warning:
`gtest-printers.h:524:35`, implicit `char8_t` to `char32_t` conversion under
`-Wcharacter-conversion`; it is not a sanitizer diagnostic. ImPlot warning
evidence contains exactly three macOS OpenGL deprecation warnings in
`examples/implot_dashboard.cpp` for `glViewport`, `glClearColor`, and
`glClear`. These records are warnings, not warning-free claims.

## Explicit exclusions

This is a no-state generic factor-regression core, not a complete
Fama-French implementation. It excludes standard factor-model enumeration,
factor providers/fetching/caching, risk-free adjustment, p-values, standard
errors, Newey-West, weighting, GLS, prediction, rolling analysis/exposure,
attribution decomposition, idiosyncratic-risk helpers, Python embedding,
warning/drop alignment behavior, reports/tearsheets, packaging/release
validation, full Python API parity, and GUI runtime validation. ImPlot proof
is explicitly compile/link only.
