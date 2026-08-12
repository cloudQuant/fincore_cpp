# Phase 24 antithetic path transform acceptance

## Accepted implementation and automatic façade

The Phase 24 design and executable plan are
`687cd63e8fefa5f1fd25bf797dc1f72ae4468dcc` (`docs: plan phase 24
antithetic path transform`). The accepted Task 104 leaf implementation is its
direct child, `0f7ea3d39ba1444bc350e5d063df2612892774d2` (`feat: add
antithetic path transform`). Its exact scope is:

- `include/fincore/simulation/paths.hpp`
- `src/simulation/paths.cpp`
- `tests/CMakeLists.txt`
- `tests/unit/antithetic_variates_test.cpp`
- `tests/unit/simulation_antithetic_variates_umbrella_test.cpp`

The existing `<fincore/simulation.hpp>` includes `paths.hpp`. The green
façade-only consumer contains no other fincore header, which proves automatic
export without a redundant façade modification.

The required independent Task 104 read-only review found **Spec PASS /
Task-Quality PASS**, with Critical/Important/Minor findings `0/0/0`. It
independently reran the focused suite, the existing-build full suite, a fresh
AppleClang Debug focused configuration, and the mandatory source oracle.

## Public contract and native orientation

```cpp
inline constexpr std::string_view antithetic_path_column_prefix{
    "antithetic_"};

dataframe antithetic_variates(const dataframe& paths);
```

Python stores individual paths as rows and time steps as columns. Native
fincore keeps the established inverse: raw timestamp/index rows and named path
columns. For genuine-double source columns `path_0`, `path_1`, ... the native
result preserves the input index exactly, copies those columns first in
DataFrame column order, and appends `antithetic_path_0`,
`antithetic_path_1`, ... in that same source order. It is not a transpose API.

If `initial` is the first value of the first source column, every appended
value is `2.0 * initial - source_value`. The result owns its index, original
column values, and mirror values after the source mutates or is destroyed.
No input timestamp is sorted, deduplicated, normalized, or filtered.

The leaf requires a nonempty index and at least one data column. Every data
column must be a genuine `double` and must have exactly the index length;
empty, mixed/non-double, or malformed input raises `fincore::input_error`.
It validates output count/allocation representability before construction and
rejects a derived name if it collides with any input name or cannot fit
DataFrame's fixed-size column-name representation without truncation. The
transformation intentionally retains ordinary IEEE arithmetic for NaN, signed
infinity, and finite overflow; it does not invent a finite-only policy.

## Source oracle, RED, and configured GREEN

The semantic source is
`/Users/yunjinqi/Documents/new_projects/fincore/fincore/simulation/paths.py`.
The mandated Anaconda command is retained verbatim in the ignored Task 104
report. Its canonical source matrix
`[[100, 110, 120], [100, 90, 80]]` yields original rows followed by
`[[100, 90, 80], [100, 110, 120]]`; its singleton yields two `[[1]]` paths.
The source's direct NumPy expression retains NaN/infinity arithmetic and
raises `IndexError` for all three empty shapes. Native C++ maps the valid
matrix path-major result into its established time-indexed named-column output
and intentionally uses `input_error` for structurally empty tables.

Before production declarations, leaf-only and façade-only public consumers
were registered. `cmake --build build --parallel 4` exited `1` solely because
`fincore::antithetic_variates` and
`fincore::antithetic_path_column_prefix` did not exist. No CTest ran after
that expected RED, so no stale executable was treated as evidence.

After the leaf implementation, the configured build passed. The prescribed
simulation/DataFrame selection passed `51/51`, and full configured CTest
passed `359/359`. The public tests cover the canonical duplicate/nonmonotonic
index `{9, 3, 9}`, exact schema/order, singleton, NaN/infinity arithmetic,
empty/wrong-type/malformed/collision/name-fit errors, copied ownership, and
the façade-only call. Raw command output is retained under
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-24-antithetic-path-transform/task-105-logs/`.

## Fresh compiler and optional-adapter gates

| Gate | Evidence |
| --- | --- |
| Current configured core | `cmake --build build --parallel 4` exited `0`; the prescribed focused CTest passed `51/51` and full CTest passed `359/359`. |
| Fresh AppleClang headless | `/tmp/fincore-phase24-task105-apple.weRC1g/build`, AppleClang `17.0.0.17000603`, Debug Ninja, C++23, testing on, ImPlot off: configure, build, and full CTest each exited `0`; full CTest passed `359/359`. The raw warning scan found no matches (expected `rg` exit `1`). |
| Fresh LLVM ASan+UBSan | `/tmp/fincore-phase24-task105-llvm.g1ip5U/build`, Homebrew Clang `22.1.8`, Debug Ninja, C++23, testing on, ImPlot off, ASan/UBSan on: configure and build exited `0`; `ctest -N` registered exactly `359` tests before execution; full CTest passed `359/359`. The sanitizer scan found no AddressSanitizer, UndefinedBehaviorSanitizer, or runtime-error diagnostic (expected `rg` exit `1`). The only warning is third-party GoogleTest `gtest-printers.h:524` `-Wcharacter-conversion`. |
| ImPlot/demo compile-link only | `/tmp/fincore-phase24-task105-implot.us8r92/build`, AppleClang Debug, testing/ImPlot/demo on with the local ImGui and ImPlot directories: configure, build, and full CTest each exited `0`; CTest passed `360/360`, including `ImPlotAdapterCompile`. The build-root `fincore_implot_demo` passed `test -f` and `test -x`; it was not launched and no GUI process was started. Exactly three macOS OpenGL deprecation warnings were recorded: `glViewport`, `glClearColor`, and `glClear`. |
| Invalid ImGui expected failure | `/tmp/fincore-phase24-task105-invalid-imgui.QOvm0c/build`, ImPlot on/testing off with an explicitly nonexistent `FINCORE_IMGUI_SOURCE_DIR`: configure exited `1` at `cmake/fincore_implot.cmake:47`. The whitespace-tolerant multiline PCRE `FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files` matched the wrapped diagnostic with exit `0`. |

All three fresh positive caches record
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH=` and
`FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF`. Their generated
`_deps/googletest-src` checkout uses remote
`https://github.com/google/googletest.git` at
`b514bdc898e2951020cbdca1304b75f5950d1f59`; `tests/CMakeLists.txt` declares
the pinned `v1.15.2` FetchContent tag. This is actual checkout provenance, not
a claim that GoogleTest was locally supplied or unfetched.

## Scope and exclusions

This accepts only the DataFrame-native antithetic path transform and its
existing simulation façade exposure. It does not add `MonteCarlo`, `SimResult`,
normal/correlated transformations, LHS, GBM, bootstrap, a caller-provided or
global RNG, NumPy random-stream parity, providers, reports, packaging/release,
or GUI runtime validation. The ImPlot evidence is compile/link-only; no GUI
was launched.
