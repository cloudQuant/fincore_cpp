# Native Usage, Packaging and Migration Notes

## Requirements

- CMake 3.25 or newer and a C++23 compiler (validated with Apple clang
  17.0.0 / Xcode 26.5 on macOS arm64).
- A local checkout of cloudQuant/DataFrame. Its path is a cache variable
  (`FINCORE_DATAFRAME_SOURCE_DIR`), not an installed dependency.
- Optional: local ImPlot and Dear ImGui checkouts plus
  `-DFINCORE_BUILD_IMPLOT=ON` (and `-DFINCORE_BUILD_IMPLOT_DEMO=ON` for the
  demo). GUI headers never enter the core library.
- GoogleTest is fetched only when `FINCORE_BUILD_TESTING=ON` (default).

## Build, test and install

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/path/to/DataFrame
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/prefix --component fincore
```

`--component fincore` installs only the library, headers and CMake package
files; omitting it also runs the fetched GoogleTest install rules. The
installed package exports the target `fincore::fincore` and version `0.1.0`.

## Consuming the installed package

```cmake
cmake_minimum_required(VERSION 3.25)
project(consumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(FINCORE_DATAFRAME_SOURCE_DIR "/path/to/DataFrame" CACHE PATH "")
add_subdirectory("${FINCORE_DATAFRAME_SOURCE_DIR}" dataframe EXCLUDE_FROM_ALL)
find_package(fincore 0.1 CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE fincore::fincore)
```

The consumer supplies DataFrame; `fincoreConfig.cmake` only describes the
native library target. A verified run at this revision prints:

```
version=0.1.0 sharpe=6.928203230275507
```

## Data and error conventions

- Timestamps are UTC nanoseconds (`fincore::timestamp`); `utc_timestamp`
  builds them. DataFrames use a sorted unique `timestamp` index.
- Numeric columns are genuine `double` columns. String metadata (symbols,
  groups, periods) uses typed fields in owning model structs instead of
  object-dtype DataFrame columns.
- Validation failures use `fincore::input_error`; factor contracts use
  `fincore::factor_data_error` (loss overruns derive from it); unknown
  runtime operations use `operation_resolution_error`.
- Result models own their vectors and frames; copying a model or report and
  mutating the copy never changes the source.
- The runtime catalog exposes 20 selected native operations under pinned
  source IDs; `builtin_catalog()` returns them and `run`/`plan`/`batch`
  require an operation request and typed `runtime_inputs`.

## Migration from the Python package

There are no Python bindings and no drop-in `import fincore` surface. The
native design maps capabilities with typed C++ models:

- pandas Series/DataFrames become `fincore::dataframe` (one or more double
  columns) or typed result structs (reports, factor models, risk estimates).
- Some routes are renamed: `cum_returns` → `cumulative_returns`,
  `cum_returns_final` → `cumulative_return`, `value_at_risk` →
  `historical_value_at_risk`, `treynor_ratio` → `cal_treynor_ratio`, `extract_pos`/`normalize_and_stack_positions`
  → `stack_positions`, `positions` (factor analysis) → `factor_positions`,
  `research_capabilities` → `research_capabilities_report`, `optimize` →
  `optimize_portfolio`, and the simulation bootstrap/statistics helpers to
  their native equivalents.
- [The program parity audit](../acceptance/2026-10-23-program-parity-audit.md)
  and its machine-readable ledger list exact, renamed, deferred, non-port
  and unresolved routes. Read them before migrating a workflow: event
  cumulative returns, three portfolio round-trip helpers and two attribution
  helpers are not ported.
- Reports are shared `report_document` values rendered by the offline HTML
  and XLSX writers; there is no matplotlib tear-sheet renderer.
- The factor runtime path is `prepare_*` → `analyze_factor` → report/research;
  models are compute-once and renderers never recalculate values.

## Supported platforms and CI

- macOS arm64 is the evidenced platform (722/722 Release suite,
  722/722 ASan+UBSan, install/export and downstream consumer).
- Linux and Windows are **not validated locally**. A GitHub Actions workflow
  (`.github/workflows/ci.yml`) builds and tests on `ubuntu-latest` and
  `macos-latest`, checking out DataFrame at the pinned commit
  `90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`. Its macOS steps were reproduced
  in a clean clone (726/726); the Ubuntu leg has **not been executed** here,
  so no Linux result is claimed.
- Sanitizers are available via `FINCORE_ENABLE_ASAN` and
  `FINCORE_ENABLE_UBSAN`. The full suite passes 722/722 under ASan+UBSan with
  Homebrew LLVM 22.1.8 (Ninja Debug).
- `benchmarks/factor_pipeline_benchmark.cpp` records the Phase 87 native
  measurements
  (`cmake --build build --target fincore_factor_benchmark`), but no automated
  performance-regression gate is configured.
- `examples/implot_dashboard.cpp` supports
  `FINCORE_IMPLOT_CAPTURE=/tmp/dashboard.ppm` to render a hidden window
  offscreen; PPM can be converted to PNG for visual inspection.

## Third-party notices

- DataFrame (cloudQuant) is the required consumer-provided data structure.
- GoogleTest is fetched for the test build only (BSD-3-Clause).
- ImPlot and Dear ImGui are optional, GUI-only dependencies (MIT).
- The Python report asset ECharts is not bundled; HTML uses inline SVG, so
  no asset hash/notice is shipped.
