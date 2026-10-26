# Phase 19 GBM-from-returns acceptance

## Accepted public facade

`<fincore/simulation.hpp>` exposes the accepted
`fincore::gbm_from_returns` declaration because it already includes
`<fincore/simulation/paths.hpp>`. Task 85 adds no umbrella-header mutation and
does not claim a second umbrella RED: Task 84's paths-header-only RED is the
missing-public-API evidence. The isolated Task 85 consumer includes only
`<fincore/simulation.hpp>` among fincore headers and verifies the deterministic
constant-return fixture: retained index `{1, 2, 3, 4}`, exactly two genuine
`double` columns named `path_0` and `path_1`, and the four source-oracle
returns `0.0025031276057951857`, `0.005012520859401182`,
`0.0075281954445340915`, and `0.010050167084168393` for each path.

The public wrapper remains the narrow source-compatible composition accepted in
Task 84: it obtains annualized parameters through the public estimator;
derives daily drift as `drift / periods_per_year`, daily volatility as
`volatility / sqrt(periods_per_year)`, duration as
`horizon / periods_per_year`, and time step as `1 / periods_per_year`; then
delegates once to public GBM. It strips only the initial price row and retains
the underlying GBM physical-step indexes. The intentionally source-compatible
double transform and delegated truncation mean horizon is not an exact row
count: horizon 3 at 5 periods per year yields exactly two rows
`0.002002001334000303` and `0.004008010677341911`. Horizon zero retains an
empty index and exactly `path_count` empty genuine-`double` path columns.

An explicit seed proves native repeatability, not NumPy stream equivalence.
The existing estimator and GBM leaves retain their validation authority; this
wrapper has no GUI runtime behavior. This acceptance excludes Monte Carlo and
bootstrap infrastructure, antithetic and Latin-hypercube sampling, `SimResult`,
parameter/GBM semantics changes, correlation/stress changes, providers,
reports, packaging/release work, and full NumPy RNG or simulation parity.

## Configured gates

The current configured build rebuilt the facade consumer. The selected CTest
set passed `40/40` and the complete CTest suite passed `315/315`; exact command
output and the fresh-gate raw logs are retained in the ignored Task 85 report:
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-19-gbm-from-returns/task-85-report.md`.

## Fresh retained gates

A fresh AppleClang 17 Debug headless configuration passed `315/315`. A fresh,
non-overlapping Homebrew LLVM 22.1.8 Debug ASan+UBSan configuration passed
`315/315`, with no AddressSanitizer, UndefinedBehaviorSanitizer, or runtime
error diagnostic. A fresh local ImPlot/demo compile-and-link configuration
passed `316/316`, including `ImPlotAdapterCompile`. The ImPlot demo existence
check examines the actual build-root
`fincore_implot_demo` executable only; the executable is not launched. A
separate invalid `FINCORE_IMGUI_SOURCE_DIR` configuration is expected to fail
and its CMake-wrapped diagnostic is matched with the whitespace-tolerant
multiline PCRE `FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files`.
Exact paths, commands, exit statuses, suite counts, and raw logs are retained
in the Task 85 report. The fresh configurations use the local DataFrame source
and, where enabled, the supplied local Dear ImGui/ImPlot source directories.
They do **not** use a local GoogleTest source override: each fresh cache has
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` empty and
`FETCHCONTENT_FULLY_DISCONNECTED=OFF`; its generated
`_deps/googletest-src/.git` is the project's pinned FetchContent checkout of
the GoogleTest `v1.15.2` remote. The report records the fully expanded fresh
configure/build/CTest commands, their individual exit statuses, raw logs, and
this provenance inspection. The LLVM build has one third-party GoogleTest
`-Wcharacter-conversion` warning; the ImPlot demo build has the known macOS
OpenGL deprecation warnings for `glViewport`, `glClearColor`, and `glClear`.
Neither is sanitizer or GUI-runtime evidence.

## Scope and traceability

The exact Task 84 implementation revision is
`cb222e867682fa346c462a487155f12c976ba4dd` (`feat: add calibrated gbm
paths`). Its scope is exactly `include/fincore/simulation/paths.hpp`,
`src/simulation/paths.cpp`, and `tests/unit/gbm_paths_test.cpp`.

Task 85's tracked scope is limited to this acceptance record, `README.md`,
`tests/CMakeLists.txt`, and the isolated
`tests/unit/simulation_gbm_from_returns_umbrella_test.cpp`. It does not alter
Task 84's leaf implementation, the existing `simulation.hpp` umbrella,
calibration or raw GBM behavior, correlation/stress functionality, generic
DataFrame helpers, third-party configuration, or ImPlot source.

## Completed revision identities

The Task 84 implementation is exactly
`cb222e867682fa346c462a487155f12c976ba4dd`; its committed scope is exactly:

- `include/fincore/simulation/paths.hpp`
- `src/simulation/paths.cpp`
- `tests/unit/gbm_paths_test.cpp`

The direct Task 85 initial-acceptance revision is exactly
`b98253f81ab3937561207ce2f36d6bb9ec389896`; its committed scope is exactly:

- `README.md`
- `docs/acceptance/2026-08-12-phase-19-gbm-from-returns-acceptance.md`
- `tests/CMakeLists.txt`
- `tests/unit/simulation_gbm_from_returns_umbrella_test.cpp`

Task 85 directly descends from the Task 84 revision. This Task 86 update is
documentation-only traceability and does not amend either earlier commit.
