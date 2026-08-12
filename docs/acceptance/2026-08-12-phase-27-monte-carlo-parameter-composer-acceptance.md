# Phase 27 Monte Carlo parameter-composer acceptance

## Accepted leaf and review baseline

The Phase 27 design and executable plan is
`8cf845c0678111ed6b87abf90f18e75111d9f615` (`docs: plan phase 27 monte
carlo parameter composer`), a direct child of the accepted Phase 26 trace.
Task 117 is the following two-commit chain:

1. `7e328da6716687b8e5596dce82fe539e972b36e3` (`feat: add parameter monte
   carlo composer`) changes exactly
   `include/fincore/simulation/monte_carlo.hpp`,
   `src/simulation/monte_carlo.cpp`, and
   `tests/unit/monte_carlo_simulation_test.cpp`.
2. Its direct child `64d258029568e5e9292613dc082675151984b8e9`
   (`test: harden parameter monte carlo composer`) changes only
   `tests/unit/monte_carlo_simulation_test.cpp`.

The first independent read-only review concluded **Spec PASS** but requested
test-quality hardening (Critical/Important/Minor `0/2/1`). The second commit
added negative drift/volatility direct-public parity, exact inherited error
message parity, and omitted-seed fixed-seed mutation coverage. A fresh
re-review concluded **Spec PASS / Task-Quality PASS**, with findings `0/0/0`.
No accepted commit was amended.

Phase 26 already made `<fincore/simulation.hpp>` include the Monte Carlo leaf.
Task 118 therefore changes no façade or CMake file and fabricates no
missing-export RED. Its existing consumer still has exactly one fincore
include, `<fincore/simulation.hpp>`. The GREEN consumer static-asserts the new
exact function type and default call expression, then proves the complete
zero-volatility price oracle, raw index, column names/order/length/type, total
column count with no extra non-`double` data column, all terminal statistics,
VaR/CVaR, and the valid zero-horizon result.

## Public contract and source-compatible composition

```cpp
[[nodiscard]] simulation_result simulate_monte_carlo_from_parameters(
    double annualized_drift,
    double annualized_volatility,
    double initial_value = 1.0,
    std::size_t path_count = 1000U,
    std::size_t horizon = 252U,
    std::optional<std::uint64_t> seed = std::nullopt);
```

This maps Python
`MonteCarlo.from_parameters(mu, sigma, S0, n_paths, horizon, seed)` to native
names that make the advertised public units explicit. Compatibility follows
the source's actual fixed-252 conversion rather than reinterpreting its
docstrings:

```text
mu_daily    = annualized_drift / 252
sigma_daily = annualized_volatility / sqrt(252)
T           = horizon / 252
dt          = 1 / 252
```

The implementation is exactly one public composition:

```cpp
return analyze_simulation_paths(geometric_brownian_motion(
    initial_value,
    annualized_drift / 252.0,
    annualized_volatility / std::sqrt(252.0),
    static_cast<double>(horizon) / 252.0,
    1.0 / 252.0,
    path_count,
    seed));
```

There is no local path loop, RNG, cache, validation, exception translation,
statistics calculation, or risk calculation. Raw GBM owns model/shape/step/
path-count/seed behavior; `analyze_simulation_paths` owns the value-copying
result, terminal distribution, statistics, VaR, and CVaR.

## Price-path, zero-horizon, seed, and error policies

Raw ordinal rows are observations and named genuine-`double` `path_0` through
`path_N` columns are paths. This function retains the complete raw GBM price
table, including index `0` and every initial value. It differs intentionally
from Phase 26's historical-return composer, whose `gbm_from_returns` removes
the initial price row and subtracts one. Consequently, `horizon == 0` here is
a valid one-row result with every terminal equal to `initial_value`; Phase 26
instead reaches an empty-return-path result error.

An explicit seed is native `std::mt19937_64` repeatability only. An omitted
seed retains the raw leaf's fresh `std::random_device` initialization. No
claim equates either stream with NumPy. The leaf tests compare seeded nonzero
volatility to the explicit native public composition, not a Python random
stream.

Errors are inherited without local catch/reword. Tests cover nonfinite initial
value/drift/volatility, zero path count, unrepresentable output and step
counts, nonfinite propagation, and nonfinite terminal-statistics arithmetic.
They additionally prove exact direct/composer message parity for a GBM model
error (`GBM annualized drift must be finite`), output-shape error
(`GBM output allocation is not representable`), and result error
(`simulation terminal statistics are not finite`). Finite zero or negative
initial values and finite negative volatility remain accepted raw-GBM inputs;
negative drift and volatility match direct public composition cell-for-cell.

## Source oracle and genuine leaf RED/GREEN

The mandatory Anaconda oracle was run from
`/Users/yunjinqi/Documents/new_projects/fincore` with
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python` and exited `0`.
For `mu=.252`, `sigma=0.0`, `S0=2.0`, `n_paths=2`, `horizon=4`, and `seed=42`,
both source paths are:

```text
[2.0, 2.0000079365236836, 2.0000158730788615,
 2.000023809665534, 2.0000317462837005]
```

Mean, median, every recorded percentile, VaR(0.05), and CVaR(0.05) are
`2.0000317462837005`, with sample standard deviation `0.0`. With horizon zero,
both paths are `[2.0]`; mean/median/VaR/CVaR are `2.0` and sample standard
deviation is `0.0`. Raw output is retained in the ignored Task 117 report.

Before the production declaration existed, the leaf-only consumer already
included only `<fincore/simulation/monte_carlo.hpp>`. Its planned function
type, default call, and real call made `cmake --build build --parallel 4` fail
with three `no member named 'simulate_monte_carlo_from_parameters' in
namespace 'fincore'` diagnostics. CTest was intentionally not run after that
failed build. After the exact declaration/composition, build passed; the
initial focused suite passed `50/50` and full CTest passed `377/377`. After
review hardening, focused Monte Carlo CTest passed `14/14` and full CTest
passed `379/379`. Task 118 adds no second RED because the accepted façade
already exports the leaf.

## Current and fresh gate evidence

Raw current and fresh gate root references are retained under
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-27-monte-carlo-parameter-composer/task-118-logs/`.
The positive commands use `tee`; their raw configure/build/CTest outputs live
under the unique roots named below.

| Gate | Evidence |
| --- | --- |
| Current configured build | Build passed; focused `MonteCarloSimulation|MonteCarloSimulationUmbrella` passed `16/16`; full CTest passed `381/381`. |
| Fresh AppleClang headless | `/tmp/fincore-phase27-task118-apple-retry.kuXuEl/build`: AppleClang `17.0.0.17000603`, Debug Ninja, testing on/ImPlot off. Configure and build passed, full CTest passed `381/381`, and the warning scan had no match (`rg` exit `1`, expected). |
| Fresh LLVM ASan+UBSan | `/tmp/fincore-phase27-task118-llvm.DZuOIb/build`: Homebrew Clang `22.1.8`, Debug Ninja, testing on/ImPlot off, ASan and UBSan on. Configure/build passed, `ctest -N` registered exactly `381`, full CTest passed `381/381`, and the sanitizer scan had no match (`rg` exit `1`, expected). |
| ImPlot/demo compile-link only | `/tmp/fincore-phase27-task118-implot.istaZI/build`: AppleClang Debug, testing/ImPlot/demo on. Configure/build/full CTest passed `382/382`, including `ImPlotAdapterCompile`; build-root `fincore_implot_demo` passed both file and executable checks and was never launched. The warning scan found exactly the expected three macOS OpenGL deprecations: `glViewport`, `glClearColor`, and `glClear`. |
| Invalid ImGui expected failure | `/tmp/fincore-phase27-task118-invalid-imgui.tlp9b7/build`: testing off/ImPlot on with a nonexistent `FINCORE_IMGUI_SOURCE_DIR` exited `1` at `cmake/fincore_implot.cmake:47`. The single-real-backslash multiline PCRE `FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files` matched with exit `0`. |

The earlier `/tmp/fincore-phase27-task118-apple.p6zwTX` attempt is retained
only as a non-acceptance diagnostic: a tool time slice allowed overlapping
Ninja work and produced `premature end of file; recovering`. The clean unique
`apple-retry.kuXuEl` result above replaces it for acceptance.

The exact positive and negative commands were:

```sh
# Current configured tree
cmake --build build --parallel 4
ctest --test-dir build \
  -R 'MonteCarloSimulation|MonteCarloSimulationUmbrella' \
  --output-on-failure
ctest --test-dir build --output-on-failure

# Fresh AppleClang headless
/usr/bin/c++ --version
cmake -S . -B /tmp/fincore-phase27-task118-apple-retry.kuXuEl/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase27-task118-apple-retry.kuXuEl/build --parallel 4
ctest --test-dir /tmp/fincore-phase27-task118-apple-retry.kuXuEl/build \
  --output-on-failure
rg -n 'warning:' \
  /tmp/fincore-phase27-task118-apple-retry.kuXuEl/{configure,build,ctest}.log
# rg exit 1: expected no warning match

# Fresh Homebrew LLVM 22 ASan+UBSan
/opt/homebrew/opt/llvm/bin/clang++ --version
cmake -S . -B /tmp/fincore-phase27-task118-llvm.DZuOIb/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
cmake --build /tmp/fincore-phase27-task118-llvm.DZuOIb/build --parallel 4
ctest --test-dir /tmp/fincore-phase27-task118-llvm.DZuOIb/build -N
rg -n '^Total Tests: 381$' \
  /tmp/fincore-phase27-task118-llvm.DZuOIb/ctest-registration.log
ctest --test-dir /tmp/fincore-phase27-task118-llvm.DZuOIb/build \
  --output-on-failure
rg -n -e 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error' \
  /tmp/fincore-phase27-task118-llvm.DZuOIb/{configure,build,ctest}.log
# sanitizer rg exit 1: expected no match

# ImPlot/demo compile-link only; neither test command launches the executable
cmake -S . -B /tmp/fincore-phase27-task118-implot.istaZI/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-phase27-task118-implot.istaZI/build --parallel 4
test -f /tmp/fincore-phase27-task118-implot.istaZI/build/fincore_implot_demo
test -x /tmp/fincore-phase27-task118-implot.istaZI/build/fincore_implot_demo
ctest --test-dir /tmp/fincore-phase27-task118-implot.istaZI/build \
  --output-on-failure
rg -n 'warning:.*(glViewport|glClearColor|glClear)' \
  /tmp/fincore-phase27-task118-implot.istaZI/build.log
# exactly 3 matches

# Invalid ImGui expected configure failure
cmake -S . \
  -B /tmp/fincore-phase27-task118-invalid-imgui.tlp9b7/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase27-task118-invalid-imgui.tlp9b7/not-a-dear-imgui-checkout
# cmake exit 1, expected
rg -U -P 'FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files' \
  /tmp/fincore-phase27-task118-invalid-imgui.tlp9b7/configure.log
# rg exit 0
```

The acceptance text and executed PCRE both contain one regex backslash before
`s+`; the Markdown code spans/blocks above render that actual `\s+` pattern,
not a literal double-backslash regex.

## GoogleTest FetchContent provenance and warnings

The LLVM CMake cache records
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH=` and
`FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF`. Its generated
`build/_deps/googletest-src` checkout has origin
`https://github.com/google/googletest.git` and HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`; `tests/CMakeLists.txt` declares
`GIT_TAG v1.15.2`. These are actual FetchContent checkout facts rather than a
claim that GoogleTest was pre-supplied.

LLVM `build.log:9` contains one third-party GoogleTest warning from
`googletest/include/gtest/gtest-printers.h:524`: `-Wcharacter-conversion` for
an implicit `char8_t` to `char32_t` conversion. It is not a fincore warning
and not an ASan/UBSan finding; the dedicated sanitizer scan had no matches.

The provenance commands were:

```sh
rg -n '^FETCHCONTENT_(SOURCE_DIR_GOOGLETEST|FULLY_DISCONNECTED)' \
  /tmp/fincore-phase27-task118-llvm.DZuOIb/build/CMakeCache.txt
git -C /tmp/fincore-phase27-task118-llvm.DZuOIb/build/_deps/googletest-src \
  remote get-url origin
git -C /tmp/fincore-phase27-task118-llvm.DZuOIb/build/_deps/googletest-src \
  rev-parse HEAD
rg -n 'GIT_(REPOSITORY|TAG)' tests/CMakeLists.txt
```

## Scope and exclusions

Task 118's candidate scope is exactly `README.md`,
`tests/unit/monte_carlo_simulation_umbrella_test.cpp`, and this acceptance
record. It changes no production header/source, façade, CMake registration, or
accepted Task 117 history. It accepts only the stateless explicit-parameter
composer and existing simulation-façade access.

This acceptance does not include Python's mutable `MonteCarlo` lifecycle,
`price_paths`, historical return estimation, the `simulate` keyword
drift/volatility branch, a caller frequency/period, antithetic options, stress
tables, a new RNG or random state, class/cache/options, providers, reports,
packaging/release work, full NumPy stream parity, or GUI runtime behavior.
ImPlot and demo evidence is compile/link only; no GUI executable was launched.

This document intentionally contains no Task 118 commit identifier. Task 119
will add non-self-referential acceptance traceability after Task 118 is
reviewed and committed; Task 119 has not begun.
