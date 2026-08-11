# Phase 10 Brinson attribution acceptance

## Accepted boundary

Phase 10 accepts only the deterministic, C++23, DataFrame-native BHB
(Brinson-Hood-Beebower) core implemented by Task 54 commit
`acd4d06000b59b3ed10ba7b51d60bddef8c717dc`
(`feat: add native Brinson attribution`) and exported by this Task 55 public
umbrella. This is not an acceptance of all Python attribution behavior, a
release, or project completion.

`<fincore/attribution.hpp>` includes only
`<fincore/attribution/brinson.hpp>`. The independent public consumer includes
only that umbrella among fincore headers and calls all three APIs with a
one-date, two-sector fixture:

| API | Native value/shape contract |
| --- | --- |
| `brinson_attribution` | Requires exactly one valid timestamp row and returns value-owning `brinson_period_result{allocation, selection, interaction, total, portfolio_return, benchmark_return}`. |
| `brinson_results` | Returns a new dataframe preserving the exact portfolio-return timestamp index with ordered genuine-`double` columns `allocation`, `selection`, `interaction`, `total`, `portfolio_return`, and `benchmark_return`. |
| `brinson_cumulative` | Returns value-owning `brinson_cumulative_result{allocation, selection, interaction, total, portfolio_cumulative, benchmark_cumulative}`. |

Each operation consumes four `fincore::dataframe` inputs and an explicit,
nonempty, distinct `std::span<const std::string_view>` of sector names. Every
selected sector must exist as a genuine `double` column with the owning index
length on all four frames; each timestamp index must be duplicate-free and the
four raw timestamp sequences must be exactly identical in original order.
Unselected metadata, including numeric metadata, is ignored. The core never
infers sectors, sorts or aligns frames, normalizes weights, or drops data.
Valid empty multi-period input produces the typed six-column empty result and
zero cumulative values; single-period attribution rejects it.

## Oracle and public-header TDD evidence

The required Anaconda command was run from
`/Users/yunjinqi/Documents/new_projects/fincore` using only
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`. Its two-period,
three-sector oracle reported period effects `(allocation, selection,
interaction, total)` of `(.004, .010, .001, .015)` and
`(.001, .002, .001, .004)`, period portfolio/benchmark returns of
`(.061, .046)` and `(.018, .014)`, and cumulative values `.005`, `.012`,
`.002`, `.019`, `.080098`, and `.06064400000000014`. Task 54's public leaf
tests lock those goldens, ordered schema/types/index preservation, explicit
selection, malformed-schema rejection, empty input, non-unit/negative weights,
and direct IEEE NaN/signed-infinity propagation.

Before `include/fincore/attribution.hpp` existed, the newly registered
`AttributionUmbrella` consumer caused `cmake --build build --parallel 4` to
exit `1` at `tests/unit/attribution_umbrella_test.cpp:1`:

```text
fatal error: 'fincore/attribution.hpp' file not found
```

The following filtered CTest found no executable test after that compilation
failure, so no stale binary was treated as TDD evidence. After adding exactly
the one-header export, `ctest --test-dir build -R AttributionUmbrella
--output-on-failure` passed `1/1`. The current focused command
`ctest --test-dir build -R 'AttributionUmbrella|Brinson|Dataframe|PerfAttrib|Positions' --output-on-failure`
passed `38/38`; the configured full suite passed `239/239`.

## Fresh build evidence

All build directories below were newly allocated by `mktemp -d` and retained.

| Gate | Command boundary and result |
| --- | --- |
| AppleClang headless | `/tmp/fincore-phase10-apple.5RTHKE`; Debug configure with `FINCORE_BUILD_IMPLOT=OFF`, build, and CTest all exited `0`. CMake identified AppleClang `17.0.0.17000603`; CTest passed `239/239`. |
| Homebrew LLVM 22 ASan+UBSan headless | `/tmp/fincore-phase10-llvm.FUoVid`; Debug configure using `/opt/homebrew/opt/llvm/bin/clang++` with `FINCORE_ENABLE_ASAN=ON`, `FINCORE_ENABLE_UBSAN=ON`, and `FINCORE_BUILD_IMPLOT=OFF`, followed by build and CTest, all exited `0`. CMake identified Clang `22.1.8`; CTest passed `239/239`. `AddressSanitizer`, `UndefinedBehaviorSanitizer`, and `runtime error` scans over configure, build, and CTest logs had no matches. |
| ImPlot/demo compile-link only | `/tmp/fincore-phase10-implot.ogrFCj`; Debug configure with `FINCORE_BUILD_IMPLOT=ON` and `FINCORE_BUILD_IMPLOT_DEMO=ON`, build, and CTest all exited `0`. `fincore_implot_demo` linked and CTest passed `240/240`, including `ImPlotAdapterCompile`. No GUI process was launched. |
| Invalid ImGui negative configuration | `/tmp/fincore-phase10-invalid-imgui.VibhMo`; `cmake -S . -B "$INVALID_DIR" -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui` exited `1` at `cmake/fincore_implot.cmake:47` with `FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required files; received '/definitely-invalid-imgui'`. |

The LLVM build emitted one third-party GoogleTest
`-Wcharacter-conversion` warning in `gtest-printers.h`; it is not a sanitizer
diagnostic. The ImPlot demo build emitted the expected macOS OpenGL deprecation
warnings for `glViewport`, `glClearColor`, and `glClear`; these are warning
records, not a warning-free claim.

## Explicit exclusions

This bounded core excludes the stateful Python `BrinsonAttribution` class,
sector mapping, equal-weight fallbacks, `brinson_hood`, style attribution,
Fama-French attribution, providers, warning/drop alignment behavior, reports,
tearsheets, packaging, release validation, full Python attribution parity, and
GUI-runtime validation. A caller must supply already-sectorized wide tables;
the ImPlot result proves compile/link integration only.
