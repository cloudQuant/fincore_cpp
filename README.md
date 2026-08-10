# fincore_cpp

The C++23, DataFrame-native foundation for fincore analytics.

## Accepted Phase 2A scalar metrics

Use the native scalar-metric surface through one public header:

```cpp
#include <fincore/metrics.hpp>
```

It exposes the accepted DataFrame-native basic, return, drawdown, ratio, risk,
statistics, alpha/beta, and timing APIs. The Phase 2A acceptance record,
including tested boundaries and explicitly deferred work, is available at
[`docs/acceptance/2026-08-10-phase-2a-scalar-metrics-acceptance.md`](docs/acceptance/2026-08-10-phase-2a-scalar-metrics-acceptance.md).

## Headless core verification

The core target is headless. It links `DataFrame::DataFrame` and does not
configure Dear ImGui, ImPlot, GLFW, or OpenGL unless the optional adapter is
explicitly enabled:

```bash
cmake -S . -B build-core -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure
```

For the core's ASan/UBSan gate, use Homebrew LLVM on this host:

```bash
cmake -S . -B build-sanitize -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON \
  -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build-sanitize --parallel 4
ctest --test-dir build-sanitize --output-on-failure
```

Sanitizer compilation remains target-scoped. When `fincore` is a static
library, its sanitizer link options are usage requirements of the final
consumer so the sanitizer runtimes are linked without mutating unrelated
targets. The current commands and observed results are recorded in
[`docs/acceptance/2026-08-10-foundation-acceptance.md`](docs/acceptance/2026-08-10-foundation-acceptance.md).

## Optional ImPlot adapter

`fincore` is headless and never links Dear ImGui, ImPlot, GLFW, or OpenGL. Set
`FINCORE_BUILD_IMPLOT=ON` to build the separate `fincore_implot` rendering
adapter. It consumes DataFrame columns while the application owns the active
Dear ImGui and ImPlot contexts and their rendering lifecycle.

The adapter uses only local source checkouts; CMake does not fetch dependencies.
The supplied ImPlot checkout is the default. Dear ImGui must be supplied
explicitly as a complete checkout containing `imgui.h`, `imgui_internal.h`,
the `imstb_*.h` headers, `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
and `imgui_widgets.cpp`:

```bash
cmake -S . -B build-implot -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/absolute/path/to/imgui
cmake --build build-implot --target fincore_implot
```

The missing-ImGui configuration diagnostic is verified, but it is not a
positive adapter-build result. The positive `fincore_implot` and demo gates
remain open until a real local `FINCORE_IMGUI_SOURCE_DIR` is supplied; do not
fetch, clone, vendor, or fabricate Dear ImGui to satisfy them.

Use the compile contract with that same, known configured binary directory:

```bash
cmake -DBINARY_DIR="$(pwd)/build-implot" \
  -P tests/compile/implot_adapter_compile_test.cmake
```

The GLFW/OpenGL dashboard is deliberately opt-in and is not a CTest dependency:

```bash
cmake -S . -B build-implot-demo -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/absolute/path/to/imgui
cmake --build build-implot-demo --target fincore_implot_demo
```
