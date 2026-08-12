# Phase 25 simulation-result core acceptance

## Accepted leaf and public facade

The Phase 25 design and executable plan are
`cd588bf648e124bbfca77cd33785992704e2518b` (`docs: plan phase 25
simulation result core`). Its direct child, the accepted Task 109 leaf, is
`4160bfc509597779ae71af22b6e1b1711d80393b` (`feat: add simulation result
core`). No post-review leaf repair was necessary. The leaf commit changes
exactly these five paths:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `include/fincore/simulation/result.hpp`
- `src/simulation/result.cpp`
- `tests/unit/simulation_result_test.cpp`

The required independent Task 109 read-only review returned **Spec PASS /
Task-Quality PASS**, with Critical/Important/Minor findings `0/0/0`. It
independently checked the source oracle, API/invariant, focused result tests,
and the full configured suite.

Before the single facade include was added, a registered consumer that includes
only `<fincore/simulation.hpp>` failed a real `cmake --build build --parallel
4`: its only test-source diagnostics said that `fincore::simulation_result`
and `fincore::analyze_simulation_paths` were absent. No CTest ran after that
expected RED, so no stale binary was used as evidence. The facade now adds
only `<fincore/simulation/result.hpp>`; the GREEN consumer uses no other
fincore header and calls the result API through the facade.

The document/gate audit also detected that the initial RED terminal output had
not been retained with the other raw logs. To preserve independently readable
evidence, it temporarily removed that same one facade include, captured the
same missing-export build failure in `facade-red-build.log`, restored the
identical include, then reran the focused and full configured GREEN suites.
This evidence-capture step made no committed product change.

## Public contract and native orientation

```cpp
struct simulation_terminal_percentiles {
    double percentile_1, percentile_5, percentile_25, percentile_50;
    double percentile_75, percentile_95, percentile_99;
};

struct simulation_terminal_statistics {
    double mean;
    double sample_standard_deviation;
    double median;
    simulation_terminal_percentiles percentiles;
};

class simulation_result {
public:
    [[nodiscard]] const dataframe& paths() const noexcept;
    [[nodiscard]] const simulation_terminal_statistics& statistics() const noexcept;
    [[nodiscard]] std::size_t path_count() const noexcept;
    [[nodiscard]] std::size_t observation_count() const noexcept;
    [[nodiscard]] double terminal_value_at_risk(double alpha = .05) const;
    [[nodiscard]] double terminal_conditional_value_at_risk(
        double alpha = .05) const;
};

[[nodiscard]] simulation_result analyze_simulation_paths(const dataframe&);
```

Native paths use raw DataFrame index rows as observations and named genuine-
`double` columns as paths—the established inverse of the Python ndarray
orientation. Analysis copies that complete path table and obtains terminal
values with `values.back()` in DataFrame column order. It does not sort,
deduplicate, normalize, or filter the raw index. The result has no public
mutable state and owns its paths, terminal distribution, and statistics after
the source mutates or is destroyed.

All path columns must be genuine `double` columns with the index length. Empty
index/no-column/mixed/non-double/malformed input raises `fincore::input_error`.
Nonterminal NaN/infinity is retained unchanged in the owned paths, while a
nonfinite terminal value is rejected before sorting. Finite terminals use the
shared `detail::linear_percentile` primitive for the seven named percentiles
and terminal VaR; CVaR is the mean of terminals `<=` that VaR. A valid
singleton has a `NaN` sample standard deviation and otherwise returns its only
terminal value. Invalid alpha (zero, one, NaN, or signed infinity) returns
`NaN`, rather than throwing.

The facade-only consumer proves the exact function type, raw duplicate index
`{7, 3, 7}`, two named genuine-`double` columns, total-schema count equal to
the double-column count (so no extra non-double column is present), all named
statistics, both terminal-risk methods, and the copied output shape.

## Source oracle, RED, and configured GREEN

The semantic source is
`/Users/yunjinqi/Documents/new_projects/fincore/fincore/simulation/base.py`.
The mandatory Anaconda command and raw output are retained in the ignored Task
109 report. Python's path-major source fixture

```text
[[100.,  95.,  80.],
 [100., 105., 110.],
 [100., 110., 130.],
 [100., 120., 190.]]
```

has terminals `[80, 110, 130, 190]`, mean `127.5`, sample standard deviation
`46.45786621588785`, median `120`, percentiles
`[80.9, 84.5, 102.5, 120, 145, 181, 188.2]`, and documented alpha-dependent
VaR/CVaR values. Native tests transpose that fixture into timestamp-indexed
named path columns with raw index `{9, 3, 9}`.

After the facade export, the current configured build exited `0`; the focused
`SimulationResult|SimulationResultUmbrella` CTest passed `6/6` and full CTest
passed `365/365`. Raw `tee` logs, including the public-facade RED, are retained
under `.superpowers/sdd/2026-08-12-fincore-cpp-phase-25-simulation-result-core/task-110-logs/`.

## Fresh compiler and optional-adapter gates

| Gate | Evidence |
| --- | --- |
| Fresh AppleClang headless | `/tmp/fincore-phase25-task110-apple.mheMbh/build`, AppleClang `17.0.0.17000603`, Debug Ninja, C++23, testing on, ImPlot off: configure/build/full CTest each exited `0`; full CTest passed `365/365`. The raw warning scan had no matches (expected `rg` exit `1`). |
| Fresh LLVM ASan+UBSan | `/tmp/fincore-phase25-task110-llvm.lItbEG/build`, Homebrew Clang `22.1.8`, Debug Ninja, C++23, testing on, ImPlot off, ASan and UBSan on: configure/build exited `0`; `ctest -N` registered exactly `365` tests before execution and full CTest passed `365/365`. The sanitizer scan found no AddressSanitizer, UndefinedBehaviorSanitizer, or runtime-error diagnostic (expected `rg` exit `1`). The only warning was third-party GoogleTest `gtest-printers.h:524` `-Wcharacter-conversion`. |
| ImPlot/demo compile-link only | `/tmp/fincore-phase25-task110-implot.BPE2ce/build`, AppleClang Debug C++23, testing/ImPlot/demo on with the local ImGui and ImPlot directories: configure/build/full CTest each exited `0`; full CTest passed `366/366`, including `ImPlotAdapterCompile`. The build-root `fincore_implot_demo` passed `test -f` and `test -x`; it was not launched and no GUI process was started. Exactly three expected macOS OpenGL deprecation warnings were recorded: `glViewport`, `glClearColor`, and `glClear`. |
| Invalid ImGui expected failure | `/tmp/fincore-phase25-task110-invalid-imgui.WJdQkf/build`, ImPlot on/testing off with an explicitly nonexistent `FINCORE_IMGUI_SOURCE_DIR`: configure exited `1`. The whitespace-tolerant multiline PCRE `FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files` matched the wrapped diagnostic with exit `0`. |

The three fresh positive CMake caches each record
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH=` and
`FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF`. Their generated
`_deps/googletest-src` checkout has remote
`https://github.com/google/googletest.git` at
`b514bdc898e2951020cbdca1304b75f5950d1f59`; `tests/CMakeLists.txt` declares
the pinned FetchContent tag `v1.15.2`. This is actual FetchContent provenance,
not a claim that GoogleTest was locally supplied or unfetched.

## Scope and exclusions

This accepts only the DataFrame-native, value-owning terminal simulation
result and its simulation-facade export. It does not accept the full mutable
Python `SimResult`/`MonteCarlo` lifecycle, return calibration or GBM changes,
antithetic/LHS/bootstrap/stress changes, a caller or global RNG, NumPy random
stream parity, providers, reports, packaging/release work, or GUI runtime
behavior. ImPlot evidence is compile/link-only; no GUI was launched.
