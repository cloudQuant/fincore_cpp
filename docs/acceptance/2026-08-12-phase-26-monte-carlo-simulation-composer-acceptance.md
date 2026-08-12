# Phase 26 Monte Carlo simulation composer acceptance

## Accepted leaf, facade, and review baseline

The Phase 26 design and executable plan is
`23bf1ddee28ec6d62ac34aea40cef8c98f20a293` (`docs: plan phase 26 monte
carlo simulation composer`). The Task 113 leaf implementation is its direct
child, `d8a808d1c547bf7788fae3861a560042082b0f88` (`feat: add native monte
carlo simulation composer`), and changes exactly:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `include/fincore/simulation/monte_carlo.hpp`
- `src/simulation/monte_carlo.cpp`
- `tests/unit/monte_carlo_simulation_test.cpp`

Its independent read-only review concluded **Spec PASS / Task-Quality PASS**
with Critical/Important/Minor findings `0/0/0`. This acceptance exports that
accepted leaf through `<fincore/simulation.hpp>` by adding only
`<fincore/simulation/monte_carlo.hpp>`.

Before that one façade include, the newly registered consumer included only
`<fincore/simulation.hpp>` and a fresh `cmake --build build --parallel 4`
failed because `fincore::simulate_monte_carlo` was absent. The raw RED is
retained in
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-26-monte-carlo-simulation-composer/task-114-logs/facade-red-build.log`.
CTest was not run after this expected missing-export failure. The GREEN
consumer includes no other fincore header; it static-asserts the exact function
type, invokes the composer through the façade, proves raw native index and
typed schema/order, all terminal statistics and both risk methods, antithetic
path count, and that the total schema contains no extra non-`double` column.

## Public contract, mapping, and native policies

```cpp
[[nodiscard]] simulation_result simulate_monte_carlo(
    const dataframe& returns,
    std::size_t path_count = 1000U,
    std::size_t horizon = 252U,
    bool antithetic = false,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);
```

The source mapping is Python `MonteCarlo(returns).simulate(n_paths, horizon,
antithetic=antithetic, seed=seed)`, but this native surface is a stateless
function rather than Python's mutable `MonteCarlo`/`SimResult` object model.
Python exposes drift and volatility as keyword-only `simulate` parameters, but
its simulation path does not use them; this native API intentionally exposes
neither. It composes the accepted public leaves exactly once:
`gbm_from_returns(returns, horizon, path_count, 252U, seed, column)`, then
optionally `antithetic_variates`, then `analyze_simulation_paths`. The fixed
`252U` is source-compatible daily annualization owned by the calibrated GBM
leaf; there is no caller annualization, drift, volatility, callback, class,
cache, global engine, or local RNG.

Native output has the established DataFrame orientation: raw ordinal rows are
observations, and named genuine-`double` columns are paths. The result owns
the complete path frame, terminal values, terminal statistics, VaR, and CVaR;
subsequent input mutation/destruction cannot change it. Antithetic operation
appends the accepted `antithetic_path_N` columns and doubles path count. An
explicit seed repeats for the supported C++ implementation only; no NumPy
random-stream equality is claimed.

Terminal validity and risk semantics are owned by `analyze_simulation_paths`:
nonfinite terminals raise `fincore::input_error`, while valid finite terminals
produce its mean, sample standard deviation, median, 1/5/25/50/75/95/99 linear
percentiles, terminal VaR, and inclusive-tail terminal CVaR. A zero horizon is
not fabricated into a result: `gbm_from_returns` produces the accepted empty
return-path shape and the result invariant raises native `fincore::input_error`.

The mandatory Anaconda oracle from the Task 113 report uses constant `0.01`
returns, `n_paths=2`, `horizon=4`, and seed `42`. Without antithetic paths it
reports terminal `0.0001587427570282074`; with antithetic paths it reports
terminals `0.0001587427570282074` and `-7.937610293851094e-05`, mean
`3.968332704484823e-05`, and sample standard deviation
`0.00013747798790091163`. Python's zero horizon raises `IndexError`; the
native error boundary above is intentional and covered by the leaf test.

## RED, GREEN, and fresh gates

Raw current build, focused/full CTest, and diff-check logs are retained in
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-26-monte-carlo-simulation-composer/task-114-logs/`.
After the façade addition, the configured build passed; focused
`MonteCarloSimulation|MonteCarloSimulationUmbrella` passed `6/6`, full CTest
passed `371/371`, and `git diff --check` was clean.

| Gate | Evidence |
| --- | --- |
| Fresh AppleClang headless | `/tmp/fincore-phase26-task114-apple.qZdDaZ/build`: Debug Ninja, C++23, testing on/ImPlot off; configure, build, and full CTest passed `371/371`. AppleClang is `17.0.0.17000603`; the raw warning scan has no matches (expected `rg` exit `1`). |
| Fresh Homebrew LLVM ASan+UBSan | `/tmp/fincore-phase26-task114-llvm.0UlaAg/build`: Homebrew Clang `22.1.8`, Debug Ninja, testing on/ImPlot off, ASan and UBSan on. Configure/build passed; `ctest -N` registered exactly `371`, then full CTest passed `371/371`. The raw sanitizer scan has no AddressSanitizer, UndefinedBehaviorSanitizer, or runtime-error match (expected `rg` exit `1`). |
| ImPlot/demo compile-link only | `/tmp/fincore-phase26-task114-implot.tWRO70/build`: AppleClang Debug, testing/ImPlot/demo on; configure/build/full CTest passed `372/372`, including `ImPlotAdapterCompile`. The build-root `fincore_implot_demo` passed `test -f` and `test -x`, and was not launched. The only expected warnings are three macOS OpenGL deprecations: `glViewport`, `glClearColor`, and `glClear`. |
| Invalid ImGui expected failure | `/tmp/fincore-phase26-task114-invalid-imgui.r63HEV/build`: ImPlot on/testing off with a nonexistent `FINCORE_IMGUI_SOURCE_DIR` exited `1` at `cmake/fincore_implot.cmake:47`. The whitespace-tolerant multiline PCRE `FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files` matched the wrapped diagnostic with exit `0`; the raw audit first proves the document's former literal-double-backslash variant exits `1`. |

The retained logs contain raw terminal output. The following are the exact
gate commands and their outcomes; each successful configure/build/CTest command
exited `0` unless explicitly marked otherwise.

```sh
# AppleClang headless: /tmp/fincore-phase26-task114-apple.qZdDaZ
/usr/bin/c++ --version                                      # exit 0
cmake -S . -B /tmp/fincore-phase26-task114-apple.qZdDaZ/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF                                # exit 0
cmake --build /tmp/fincore-phase26-task114-apple.qZdDaZ/build --parallel 4  # exit 0
ctest --test-dir /tmp/fincore-phase26-task114-apple.qZdDaZ/build --output-on-failure  # exit 0; 371/371
rg -n 'warning:' /tmp/fincore-phase26-task114-apple.qZdDaZ/{configure,build,ctest}.log  # exit 1; expected no match

# Homebrew LLVM 22 ASan+UBSan: /tmp/fincore-phase26-task114-llvm.0UlaAg
/opt/homebrew/opt/llvm/bin/clang++ --version                 # exit 0
cmake -S . -B /tmp/fincore-phase26-task114-llvm.0UlaAg/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON         # exit 0
cmake --build /tmp/fincore-phase26-task114-llvm.0UlaAg/build --parallel 4  # exit 0
ctest --test-dir /tmp/fincore-phase26-task114-llvm.0UlaAg/build -N          # exit 0; 371 registered
ctest --test-dir /tmp/fincore-phase26-task114-llvm.0UlaAg/build --output-on-failure  # exit 0; 371/371
rg -n -e 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error' \
  /tmp/fincore-phase26-task114-llvm.0UlaAg/{configure,build,ctest}.log      # exit 1; expected no match

# ImPlot/demo compile-link only: /tmp/fincore-phase26-task114-implot.tWRO70
cmake -S . -B /tmp/fincore-phase26-task114-implot.tWRO70/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON   # exit 0
cmake --build /tmp/fincore-phase26-task114-implot.tWRO70/build --parallel 4 # exit 0
test -f /tmp/fincore-phase26-task114-implot.tWRO70/build/fincore_implot_demo # exit 0
test -x /tmp/fincore-phase26-task114-implot.tWRO70/build/fincore_implot_demo # exit 0
ctest --test-dir /tmp/fincore-phase26-task114-implot.tWRO70/build --output-on-failure # exit 0; 372/372
rg -n 'glViewport|glClearColor|glClear' \
  /tmp/fincore-phase26-task114-implot.tWRO70/build.log 2>&1 | tee \
  /tmp/fincore-phase26-task114-implot.tWRO70/opengl-warnings.log             # exit 0; exactly 3 deprecation warnings

# Invalid ImGui negative configuration: /tmp/fincore-phase26-task114-invalid-imgui.r63HEV
cmake -S . -B /tmp/fincore-phase26-task114-invalid-imgui.r63HEV/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase26-task114-invalid-imgui.r63HEV/not-a-dear-imgui-checkout # exit 1; expected
rg -U -P 'FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files' \
  /tmp/fincore-phase26-task114-invalid-imgui.r63HEV/configure.log           # exit 0
```

The two `test` commands above are the only demo interaction: no executable was
launched. The raw PCRE audit in
`task-114-logs/invalid-imgui-pcre-audit.log` first ran the literal
`all required\\s+files` pattern (exit `1`), then the shown single-backslash
pattern (exit `0`).

The fresh positive builds record actual FetchContent provenance: their CMake
caches leave `FETCHCONTENT_SOURCE_DIR_GOOGLETEST` empty and set
`FETCHCONTENT_FULLY_DISCONNECTED=OFF`; the LLVM raw provenance records the
generated `_deps/googletest-src` remote
`https://github.com/google/googletest.git` at
`b514bdc898e2951020cbdca1304b75f5950d1f59`, while `tests/CMakeLists.txt`
declares `v1.15.2`. This is actual FetchContent checkout provenance, not a
claim that GoogleTest was locally supplied or unfetched.

LLVM's `build.log:9` has one third-party warning from generated FetchContent
GoogleTest, `googletest/include/gtest/gtest-printers.h:524`,
`-Wcharacter-conversion` for `char8_t` to `char32_t`. It is neither a fincore
warning nor an ASan/UBSan finding; the dedicated sanitizer scan above had no
matches.

## Scope and exclusions

This accepts only the DataFrame-native, value-owning historical-return Monte
Carlo composer and its simulation-façade export. It does not accept a mutable
Python object lifecycle, a random engine or caller RNG, NumPy stream parity,
drift/volatility/annualization controls, duplicated GBM/result calculation,
return pre-cleaning, normal/correlated/LHS/bootstrap/stress changes, providers,
reports, packaging/release work, or GUI runtime behavior. ImPlot evidence is
compile/link-only; no GUI executable is launched.

This document deliberately contains no Task 114 commit identifier. The
independent Task 114 document/gate review remains required before staging and
commit.
