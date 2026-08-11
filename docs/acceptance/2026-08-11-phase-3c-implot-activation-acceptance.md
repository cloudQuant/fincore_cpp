# Phase 3C ImPlot activation acceptance

## Scope and traceability

This is a bounded build-and-compile acceptance for the optional ImPlot adapter
and GLFW/OpenGL demo. It does not claim an interactive render, a visual smoke
test, whole-project completion, or full Python public-API parity.

- Task 27 implementation commit:
  `e60d64ec65560e80e079f89e3d433007b8d3f688`
  (`test: register ImPlot adapter build contract`).
- Task 28 initial documentation commit: recorded by the documentation-only
  traceability follow-up after the initial acceptance commit is created; that
  follow-up does not amend history or need to self-reference.
- Local, user-supplied dependencies used by the positive builds:
  - Dear ImGui: `/Users/yunjinqi/Documents/new_projects/imgui`
  - ImPlot: `/Users/yunjinqi/Documents/new_projects/implot`

The current cache default for `FINCORE_IMGUI_SOURCE_DIR` is the supplied Dear
ImGui path above. `FINCORE_BUILD_IMPLOT` and `FINCORE_BUILD_IMPLOT_DEMO` remain
`OFF` by default, and callers may override the cache path explicitly when they
have another complete checkout.

## Accepted boundary

With `FINCORE_BUILD_IMPLOT=ON` and the default cache path, the separate
`fincore_implot` adapter compiles, and with
`FINCORE_BUILD_IMPLOT_DEMO=ON` the `fincore_implot_demo` executable compiles
and links. In ImPlot-enabled test configurations, CTest registers exactly the
`ImPlotAdapterCompile` compile contract; both positive configurations listed
it and passed it.

This is only a compile/build and headless CTest boundary. The adapter expects
the embedding application to own current Dear ImGui and ImPlot contexts and to
call it within that application's frame/render lifecycle. The GLFW/OpenGL demo
owns its own event loop, but it was not launched by this acceptance. There is
no automated hidden window, GUI runtime, screenshot, or interactive visual
claim here.

The three macOS OpenGL deprecation warnings from `glViewport`,
`glClearColor`, and `glClear` are recorded below as warnings. Successful build
and CTest results are not a claim that the graphics configuration is warning
free.

## Fresh positive evidence

All builds used independent `mktemp -d` roots, Ninja, CMake `4.0.2`, and the
same source worktree. The two positive configure commands intentionally omit
`FINCORE_IMGUI_SOURCE_DIR`; their generated cache files resolve it to the
supplied default path.

### AppleClang default-path positive build

Fresh root: `/tmp/fincore-task28-apple.kT2pDc`.

`/usr/bin/c++ --version` reported Apple clang `17.0.0`
(`clang-1700.6.3.2`); CMake identified `AppleClang 17.0.0.17000603` and used
`/usr/bin/c++`.

```bash
cmake -S . -B /tmp/fincore-task28-apple.kT2pDc/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot
cmake --build /tmp/fincore-task28-apple.kT2pDc/build --parallel 4
ctest --test-dir /tmp/fincore-task28-apple.kT2pDc/build -N
ctest --test-dir /tmp/fincore-task28-apple.kT2pDc/build --output-on-failure
```

All commands exited `0`. The cache recorded:

```text
FINCORE_IMGUI_SOURCE_DIR:PATH=/Users/yunjinqi/Documents/new_projects/imgui
```

The full build linked `fincore_implot_demo`. `ctest -N` listed
`ImPlotAdapterCompile` as test `#147` and reported `Total Tests: 147`; the full
run passed `147/147` with `0` failures. The build emitted exactly three macOS
OpenGL deprecation warnings, for `glViewport`, `glClearColor`, and `glClear`.

### Homebrew LLVM 22 ASan/UBSan default-path positive build

Fresh root: `/tmp/fincore-task28-llvm-asan-ubsan.ZT4qnR`.

`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake identified `Clang 22.1.8` and used the same compiler for C and
C++.

```bash
cmake -S . -B /tmp/fincore-task28-llvm-asan-ubsan.ZT4qnR/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_ENABLE_ASAN=ON \
  -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot
cmake --build /tmp/fincore-task28-llvm-asan-ubsan.ZT4qnR/build --parallel 4
ctest --test-dir /tmp/fincore-task28-llvm-asan-ubsan.ZT4qnR/build -N
ctest --test-dir /tmp/fincore-task28-llvm-asan-ubsan.ZT4qnR/build --output-on-failure
```

All commands exited `0`. The cache recorded both sanitizer options as `ON` and
resolved `FINCORE_IMGUI_SOURCE_DIR` to
`/Users/yunjinqi/Documents/new_projects/imgui`. The full build linked
`fincore_implot_demo`. `ctest -N` listed `ImPlotAdapterCompile` as test `#147`
and reported `Total Tests: 147`; the full run passed `147/147` with `0`
failures.

No `AddressSanitizer`, `UndefinedBehaviorSanitizer`, or `runtime error`
diagnostic appeared in the preserved LLVM build or CTest logs. The compiler
did emit one third-party GoogleTest `-Wcharacter-conversion` warning and the
same three macOS OpenGL deprecation warnings; neither is a sanitizer finding,
and this acceptance does not describe the build as warning free.

## Fresh headless non-regression

Fresh root: `/tmp/fincore-task28-headless.yvO3qh`. `/usr/bin/c++ --version`
reported Apple clang `17.0.0` (`clang-1700.6.3.2`); CMake identified
`AppleClang 17.0.0.17000603`.

```bash
cmake -S . -B /tmp/fincore-task28-headless.yvO3qh/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task28-headless.yvO3qh/build --parallel 4
ctest --test-dir /tmp/fincore-task28-headless.yvO3qh/build -N
ctest --test-dir /tmp/fincore-task28-headless.yvO3qh/build --output-on-failure
```

All commands exited `0`. `ctest -N` reported `Total Tests: 146` and did not
list `ImPlotAdapterCompile`; the full run passed `146/146` with `0` failures.
The headless build log had no ImGui, ImPlot, GLFW, or OpenGL target/source
reference. Thus the registered adapter compile contract and its graphics
dependencies remain absent when the optional feature is off.

## Fresh invalid override diagnostic

Fresh root: `/tmp/fincore-task28-invalid-imgui-final.fmXJEH`.

```bash
cmake -S . -B /tmp/fincore-task28-invalid-imgui-final.fmXJEH/build -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task28-invalid-imgui-final.fmXJEH/not-a-dear-imgui-checkout
```

The configure exited `1`, as required, and emitted:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-task28-invalid-imgui-final.fmXJEH/not-a-dear-imgui-checkout'
```

This is a negative dependency-diagnostic check, not evidence against the
positive default-path build. No Dear ImGui checkout was fetched, cloned,
vendored, fabricated, or substituted for either the positive or negative
paths.

## Historical status and remaining work

The dated 2026-08-10 foundation and Phase 2C/3A/3B acceptance documents
remain historical observations. Their former positive-ImPlot OPEN item is
superseded by this Phase 3C acceptance; they are not rewritten. This does not
expand their metric-slice claims or establish full fincore parity.

Work explicitly still open includes interactive visual smoke testing on a user
display; multi-series and tearsheet visualizations; unimplemented multi-column
and full-index rolling APIs; positions and transactions; round trips;
attribution; Bayesian/bootstrap and advanced risk models; optimization;
simulation; reports; data providers; packaging and release; and full Python
public-API parity.
