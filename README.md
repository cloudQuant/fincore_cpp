# fincore_cpp

The C++23, DataFrame-native foundation for fincore analytics.

## Optional ImPlot adapter

`fincore` is headless and never links Dear ImGui, ImPlot, GLFW, or OpenGL. Set
`FINCORE_BUILD_IMPLOT=ON` to build the separate `fincore_implot` rendering
adapter. It consumes DataFrame columns while the application owns the active
Dear ImGui context and its rendering lifecycle.

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
