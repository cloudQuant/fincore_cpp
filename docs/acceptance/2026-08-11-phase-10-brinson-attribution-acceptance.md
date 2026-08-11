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

The following directories were allocated afresh with four separate
`mktemp -d /tmp/fincore-phase10-<gate>.XXXXXX` calls and were retained. Every
configure, build, and CTest invocation was captured with `2>&1 | tee` into the
named log; these claims are limited to those captured invocations.

| Gate | Directory, commands, logs, and result |
| --- | --- |
| AppleClang headless | `/tmp/fincore-phase10-apple.Y1K8U4`. `cmake -S . -B /tmp/fincore-phase10-apple.Y1K8U4 -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF`, `cmake --build /tmp/fincore-phase10-apple.Y1K8U4 --parallel 4`, and `ctest --test-dir /tmp/fincore-phase10-apple.Y1K8U4 --output-on-failure` each exited `0`, captured respectively in `configure.log`, `build.log`, and `ctest.log`. The configure log identifies AppleClang `17.0.0.17000603`; CTest reports `239/239` passed. The additional focused CTest command `ctest --test-dir /tmp/fincore-phase10-apple.Y1K8U4 -R 'AttributionUmbrella|Brinson|Dataframe|PerfAttrib|Positions' --output-on-failure` exited `0`, reports `38/38`, and is retained as `focused-ctest.log`. `warnings-scan.log` records its three log inputs, exact `rg` command, expected no-match exit `1`, and zero warning matches. |
| Homebrew LLVM 22 ASan+UBSan headless | `/tmp/fincore-phase10-llvm.lpX2wZ`. `cmake -S . -B /tmp/fincore-phase10-llvm.lpX2wZ -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF`, `cmake --build /tmp/fincore-phase10-llvm.lpX2wZ --parallel 4`, and `ctest --test-dir /tmp/fincore-phase10-llvm.lpX2wZ --output-on-failure` each exited `0`, captured in `configure.log`, `build.log`, and `ctest.log`. CMake identifies Clang `22.1.8`, while `compiler-version.log` records `/opt/homebrew/opt/llvm/bin/clang++ --version` as Homebrew clang `22.1.8`; CTest reports `239/239` passed. `sanitizer-scan.log` explicitly lists those three input logs and records `rg -n -e 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error'` over them; its `rg` exit status is `1`, explicitly treated as the expected zero-match result, so no diagnostic matching those three patterns was present in the captured logs. |
| ImPlot/demo compile-link only | `/tmp/fincore-phase10-implot.zalvUO`. `cmake -S . -B /tmp/fincore-phase10-implot.zalvUO -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON`, `cmake --build /tmp/fincore-phase10-implot.zalvUO --parallel 4`, and `ctest --test-dir /tmp/fincore-phase10-implot.zalvUO --output-on-failure` each exited `0`, captured in `configure.log`, `build.log`, and `ctest.log`. The build log records `fincore_implot_demo` linking; CTest reports `240/240`, including `ImPlotAdapterCompile`. This is a compile/link contract only: no GUI process was started. |
| Invalid ImGui negative configuration | `/tmp/fincore-phase10-invalid-imgui.StLqLf`. `cmake -S . -B /tmp/fincore-phase10-invalid-imgui.StLqLf -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui 2>&1 | tee /tmp/fincore-phase10-invalid-imgui.StLqLf/configure.log` exited `1` as expected. The complete retained diagnostic identifies `cmake/fincore_implot.cmake:47` and says `FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required files; received '/definitely-invalid-imgui'`. |

Warning evidence is retained rather than inferred. AppleClang's
`warnings-scan.log` has zero matches. LLVM's `warnings-scan.log` records one
warning from the downloaded third-party GoogleTest
`gtest-printers.h:524:35`: implicit `char8_t` to `char32_t` conversion under
`-Wcharacter-conversion`; this is not one of the sanitizer patterns scanned
above. ImPlot's `warnings-scan.log` records exactly three macOS OpenGL
deprecation warnings in `examples/implot_dashboard.cpp` for `glViewport`,
`glClearColor`, and `glClear` (and the build log reports `3 warnings
generated`).

## Explicit exclusions

This bounded core excludes the stateful Python `BrinsonAttribution` class,
sector mapping, equal-weight fallbacks, `brinson_hood`, style attribution,
Fama-French attribution, providers, warning/drop alignment behavior, reports,
tearsheets, packaging, release validation, full Python attribution parity, and
GUI-runtime validation. A caller must supply already-sectorized wide tables;
the ImPlot result proves compile/link integration only.
