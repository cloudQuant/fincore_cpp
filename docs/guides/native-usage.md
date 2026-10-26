# Native Usage, Packaging and Migration Notes

The delivery is a C++23/DataFrame library. It has no Python bindings or
`import fincore` compatibility. Use the [current iteration-8 acceptance](../acceptance/2026-10-24-iteration-8-native-parity-acceptance.md)
for measured compiler/platform/test results and the [route ledger](../iterations/iteration-8-full-python-parity/route-ledger.tsv)
for exact route mappings and exclusions. Historical counts in older phase
records describe those earlier candidates.

## Build and install

CMake 3.25+ and a C++23 compiler are required. Pin DataFrame to
`90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`. Either provide an existing
`DataFrame::DataFrame` target, set `FINCORE_DATAFRAME_SOURCE_DIR` for a source
build, or install DataFrame first so that CMake can find its package:

```sh
cmake -S /path/to/DataFrame -B dataframe-build \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/path/to/prefix \
  -DHMDF_TESTING=OFF -DHMDF_EXAMPLES=OFF -DHMDF_BENCHMARKS=OFF
cmake --build dataframe-build -j2
cmake --install dataframe-build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/prefix -DCMAKE_INSTALL_PREFIX=/path/to/prefix
cmake --build build -j2
ctest --test-dir build --output-on-failure
cmake --install build --component fincore
```

The `fincore` component installs the core library, public headers, CMake
config/version/targets, report asset and notices. GoogleTest is needed only
when `FINCORE_BUILD_TESTING=ON`; offline test builds require its source to be
prepopulated, for example with `FETCHCONTENT_SOURCE_DIR_GOOGLETEST`. An offline
build with supplied dependencies is different from the first dependency download.

Enable `FINCORE_BUILD_IMPLOT=ON` with `FINCORE_IMGUI_SOURCE_DIR` and
`FINCORE_IMPLOT_SOURCE_DIR` for the optional adapter. The current submodule
pins are ImGui `4e99c0ab1a28c2b3d1f250ec0266dbf1e455ef58` and ImPlot
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`; older phase records that mention
another ImGui revision describe an earlier candidate. Install that separate component with
`cmake --install build --component implot`. The demo additionally needs
`FINCORE_BUILD_IMPLOT_DEMO=ON` and its platform window/OpenGL dependencies.
Core headers do not require GUI headers.

## Consume a relocated installed prefix

```cmake
cmake_minimum_required(VERSION 3.25)
project(consumer LANGUAGES CXX)
find_package(fincore 0.1 CONFIG REQUIRED COMPONENTS core)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE fincore::fincore)
```

Configure the consumer with `-DCMAKE_PREFIX_PATH=/relocated/prefix`.
`fincoreConfig.cmake` resolves the installed DataFrame dependency automatically
unless its target already exists. Consumers need neither source checkout.
GUI consumers request `COMPONENTS implot` and link `fincore::implot` in addition
to the core as needed. A missing required component fails configuration;
`OPTIONAL_COMPONENTS implot` can be queried through `fincore_implot_FOUND`.

The executable public-only examples used for acceptance are under
`tests/consumer/`. `tools/verify_installed_package.py` builds in a fresh scratch
root, relocates the prefix, makes source/build roots unavailable, and executes
headless and optional GUI consumers plus negative component checks. C++ ABI
compatibility across arbitrary compilers/standard libraries is not guaranteed;
build DataFrame, fincore and the consumer with compatible toolchains.

## Data, runtime and report contracts

Timestamps are UTC nanoseconds (`fincore::timestamp`); `utc_timestamp` builds
them. Labelled frames generally require sorted unique indices. Native numeric
columns are `double`; assets, groups, horizons and metadata use typed owning
fields. Multi-input alignment follows each API's declared strict, inner or
outer-drop-NaN policy. MWR is positional; XIRR sorts and aggregates UTC days
and uses /365. Validation uses native exceptions or explicit status values.

Result models own their data. `portfolio_inputs` preserves absent versus
supplied-empty optional tables and copies at ingestion/materialization.
Factor processing is `prepare_*` → `analyze_factor` → report/chart projection;
renderers consume already computed values. Requested full-universe group
snapshots differ from independent subgroup analyses.

`builtin_catalog()` contains 270 registered native operation IDs in the
iteration-8 candidate, re-measured as `operations=270` on 2026-10-26 at the
iteration-10 commit; the three live-provider IDs are deliberately excluded
and remain `EXTERNAL_OPTIONAL` ([registration evidence](../evidence/iteration-8/p105/registered-operation-ids.txt)).
`plan` is a flat ordered request list, not a dependency graph. `run`/`batch`
use typed inputs; config is provenance and is not forwarded implicitly.
Sessions cache deterministic operations only, own inputs/outputs and reject
execution after close. The ledger lists each omitted operation separately.

Shared report schema 2 includes units, legends and offline asset text. HTML
and XLSX have explicit missing/nonfinite policies and do not promise Python
byte parity. `report_html_options.language` accepts `en` or `zh`; standard
labels are localized while caller text and computed values are preserved.
The offline ECharts 5.5.0 backend embeds the pinned asset. Its SHA-256 is
`42f8329d989b6f6539dd2b15bbdf0d82025762ac112fbb60dc57b27d7bcf3946`;
license and notices are installed with the core. The default inline SVG
backend remains available.

The native runtime serialization envelope is version 1, the report schema is
version 2, and artifact bundle manifest is version 2; these are separate
version domains. Runtime serialization supports its documented variant,
including exact numeric tags, not arbitrary Python objects or a full factor
model transport envelope. Artifact bundles optionally attach per-path media
type/provenance descriptors. Writers calculate byte size and SHA-256 from
actual bytes, reject unsafe/preexisting destinations and clean up their own
partial outputs. Undescribed legacy artifacts default to
`application/octet-stream` with empty provenance.

## Validation and migration limits

Use the candidate record for macOS Release/ASan+UBSan, local Linux x86_64,
installed consumers and visual evidence. Local QEMU execution is not hosted
CI; Windows and Linux GUI are not claimed. Sanitizers are enabled with
`FINCORE_ENABLE_ASAN` and `FINCORE_ENABLE_UBSAN`; the macOS sanitizer gate uses
Homebrew LLVM rather than inferring success from an AppleClang build.

The benchmark runner covers scalar, batched, rolling, portfolio and panel
workloads with separate conversion/compute/copy/render/I/O measurements,
repetitions and per-process RSS. Retained comparator failures and documented
tradeoffs are not speedup claims. Full Python/native factor-model and report
costs are not equivalent workloads.

Read the [iteration-8 scope and claim boundaries](../acceptance/2026-10-24-iteration-8-native-parity-acceptance.md#1-scope-and-claim-boundaries)
before migrating Python workflows. Network SDKs, Python discovery/object
facades and backend artist objects are excluded; specialized plots, calendar
helpers and additional runtime registrations have individually named deferred
rows. Native GARCH fits reject nonfinite/zero-variance observations after NaN
cleaning, and IRR rejects a returned rate whose normalized NPV residual exceeds
1e-10. These explicit failure policies can differ from Python edge behavior.
Native capability acceptance does not establish GIPS compliance.
