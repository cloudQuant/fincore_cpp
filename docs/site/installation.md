# Installation

## Requirements

- CMake 3.25 or newer.
- A C++23 compiler: MSVC 19.4+, Clang 17+, or GCC 13+.
- Git submodules for the pinned DataFrame, ImGui and ImPlot sources.

The core library does not require Python, Doxygen or Sphinx at build time.
Those tools are only needed to build this documentation.

## Build from a checkout

```bash
git clone https://github.com/cloudQuant/fincore_cpp.git
cd fincore_cpp
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Windows, use a Visual Studio developer environment and a CMake generator
supported by the installed MSVC toolchain. The pinned submodule sources are
used by default; a consumer-provided DataFrame can be selected with
`FINCORE_DATAFRAME_SOURCE_DIR` or `find_package(DataFrame CONFIG)`.

## Install and consume

```bash
cmake --install build --prefix /path/to/prefix
```

A downstream CMake project can then use:

```cmake
find_package(fincore CONFIG REQUIRED COMPONENTS fincore)
target_link_libraries(my_target PRIVATE fincore::fincore)
```

The optional ImPlot component is available only when the library was built
with `FINCORE_BUILD_IMPLOT=ON` and its GUI dependencies are installed.

## Optional documentation build

```bash
python -m venv .venv-docs
.venv-docs/bin/pip install -r docs/requirements-docs.txt
cd docs/site
doxygen Doxyfile
sphinx-build -b html -W -n --keep-going . ../_build/html/en
```

The exact commands for the supported platforms and the current validation
status are recorded in the repository's iteration-10 acceptance record.
