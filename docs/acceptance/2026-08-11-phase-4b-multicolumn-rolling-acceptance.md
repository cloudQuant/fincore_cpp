# Phase 4B multi-column rolling metrics acceptance

## Scope and traceability

This bounded acceptance completes the public function list in Python
`fincore.metrics.rolling` for the native DataFrame API. It is not whole-project
fincore parity, release readiness, a Python-shaped DataFrame facade, or an
interactive ImPlot result.

- Task 31 implementation: `9bfedc0ea6c401366009c13e0b44a05dacb37ca9`
  (`feat: add multicolumn rolling metrics`).
- Task 31 follow-up delegation coverage:
  `9e82194c9cca72b7e6755c85d6c7e7c6e47f8848`
  (`test: cover custom roll alpha beta delegation`).
- Task 32 acceptance publication: recorded by the subsequent docs-only
  traceability commit after this initial acceptance commit; this document does
  not self-reference or amend history.

## Accepted public contract

`<fincore/metrics.hpp>` exposes `roll_alpha_beta`, both `rolling_beta`
overloads, and `rolling_regression` through the existing rolling header.

- `roll_alpha_beta` returns exactly two double columns, `"alpha"` then
  `"beta"`, at complete aligned-window endpoints and delegates each window to
  public `alpha_beta`.
- Single-factor `rolling_beta` returns the requested returns-column name on
  the full native aligned index.
- Explicit multi-factor `rolling_beta` takes a non-empty ordered factor-name
  selection and returns the selected factor names in that order on the full
  native aligned index; it does not infer heterogeneous numeric metadata.
- `rolling_regression` returns compact, nonannualized `"alpha"` and `"beta"`
  endpoint columns.
- Native paired alignment retains only common timestamps in the left/returns
  supplied order. This does not claim Python Series outer-shaped pairing.
- Fixed-width beta/regression windows with a nonfinite pair or zero factor
  variance are NaN. This is local rolling behavior, not a new scalar or global
  nonfinite policy.

## Acceptance checklist

- [x] Public-only `MetricsUmbrella` consumer constructs a multi-column factor
  DataFrame and checks the endpoint/full-index shapes and single/two/multi-
  column names for every new API.
- [x] The Task 31 public API RED is retained as the genuine pre-implementation
  evidence; no accepted umbrella include was removed to manufacture another
  RED.
- [x] AppleClang headless configure, build, and full CTest run were executed
  in a fresh `mktemp -d` directory.
- [x] Homebrew LLVM 22 ASan+UBSan headless configure, build, and full CTest
  run were executed in a separate fresh `mktemp -d` directory.
- [x] The default local ImGui/ImPlot adapter/demo compile-and-link CTest
  contract was executed without launching a GUI.
- [x] An explicit invalid-ImGui configure exited `1` with the expected CMake
  diagnostic and without fetching, cloning, vendoring, fabricating, or
  substituting dependencies.
- [ ] Commit SHAs, warnings, remaining scope, and owned-path audit are based
  only on completed commands and commits.

## Fresh verification

All commands used CMake `4.0.2`, Ninja `1.12.1`, and the source worktree
`/Users/yunjinqi/Documents/new_projects/fincore_cpp/.worktrees/fincore-cpp-foundation`.

### AppleClang headless core

Fresh root: `/tmp/fincore-task32-core.8kL8g2`. `/usr/bin/c++ --version`
reported Apple clang `17.0.0` (`clang-1700.6.3.2`); CMake identified
`AppleClang 17.0.0.17000603` and used `/usr/bin/c++`.

```bash
cmake -S . -B /tmp/fincore-task32-core.8kL8g2/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task32-core.8kL8g2/build --parallel 4
ctest --test-dir /tmp/fincore-task32-core.8kL8g2/build -N
ctest --test-dir /tmp/fincore-task32-core.8kL8g2/build --output-on-failure
```

Every command exited `0`. `ctest -N` reported `Total Tests: 156`; the full
run passed `156/156` with `0` failures. The captured build output had no
compiler warning diagnostic.

### Homebrew LLVM 22 ASan+UBSan headless core

Fresh root: `/tmp/fincore-task32-sanitize.dqvwqI`.
`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake identified `Clang 22.1.8` and used that compiler for C and
C++. The configured cache enables both ASan and UBSan.

```bash
cmake -S . -B /tmp/fincore-task32-sanitize.dqvwqI/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build /tmp/fincore-task32-sanitize.dqvwqI/build --parallel 4
ctest --test-dir /tmp/fincore-task32-sanitize.dqvwqI/build -N
ctest --test-dir /tmp/fincore-task32-sanitize.dqvwqI/build --output-on-failure
```

Every command exited `0`. `ctest -N` reported `Total Tests: 156`; the full
run passed `156/156` with `0` failures. The build emitted one third-party
GoogleTest `-Wcharacter-conversion` warning at `gtest-printers.h:524` for
`char8_t` to `char32_t`; it is not a fincore or sanitizer finding. No
`AddressSanitizer`, `UndefinedBehaviorSanitizer`, or `runtime error`
diagnostic appeared in the captured build or CTest output.

### Default local ImGui/ImPlot adapter and demo compile boundary

Fresh root: `/tmp/fincore-task32-implot.bRojk5`. This AppleClang configuration
intentionally passed neither `FINCORE_IMGUI_SOURCE_DIR` nor
`FINCORE_IMPLOT_SOURCE_DIR`; the generated cache resolved them to the supplied
local checkouts:

```text
FINCORE_IMGUI_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/imgui
FINCORE_IMPLOT_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/implot
```

```bash
cmake -S . -B /tmp/fincore-task32-implot.bRojk5/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-task32-implot.bRojk5/build --parallel 4
ctest --test-dir /tmp/fincore-task32-implot.bRojk5/build -N
ctest --test-dir /tmp/fincore-task32-implot.bRojk5/build --output-on-failure
```

Every command exited `0`. The build linked `fincore_implot_demo`; `ctest -N`
listed `ImPlotAdapterCompile` as test `#157`, and the full run passed `157/157`
with `0` failures. The build emitted exactly three macOS OpenGL deprecation
warnings for `glViewport`, `glClearColor`, and `glClear`. No GUI executable was
launched: this is compile/link plus headless CTest evidence, not an interactive
visual claim.

### Explicit invalid ImGui override

Fresh root: `/tmp/fincore-task32-invalid-imgui.DRHuWE`.

```bash
cmake -S . -B /tmp/fincore-task32-invalid-imgui.DRHuWE/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task32-invalid-imgui.DRHuWE/not-a-dear-imgui-checkout
```

The configure exited `1` at `cmake/fincore_implot.cmake:47` with:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task32-invalid-imgui.DRHuWE/not-a-dear-imgui-checkout'
```

This negative diagnostic is separate from the successful default-path build.
No dependency was fetched, cloned, vendored, fabricated, or substituted by
these verification commands.

## Deferred scope

Portfolio positions and transactions; round trips; performance attribution;
bootstrap/distribution statistics; Bayesian and advanced-risk models;
optimization; simulation; reports/tearsheets; a full native facade;
packaging/release; and interactive ImPlot visual smoke testing remain separate
work. This acceptance is not full fincore parity or project completion.
