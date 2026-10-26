# Installation

## Requirements

| Requirement | Minimum | Notes |
|---|---|---|
| CMake | 3.25 | Modern target and install rules |
| Compiler | MSVC 19.4, Clang 17, GCC 13 | C++23 is a hard requirement |
| Git submodules | pinned revisions | DataFrame, ImGui and ImPlot sources |
| Python | none | Only tooling, benchmarks and documentation use it |

The core library needs no Python, Doxygen or Sphinx at build time. Those tools are
only needed to build this documentation site or run the benchmark harness.

## Build from a checkout

```bash
git clone https://github.com/cloudQuant/fincore_cpp.git
cd fincore_cpp
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The suite is 1092 cases and takes under a minute on a desktop machine. If the
configure step reports a missing `CMakeLists.txt`, the submodules were not
initialised.

## Build options

| Option | Default | Effect |
|---|---|---|
| `FINCORE_BUILD_TESTING` | `ON` | Builds the test targets; turn off for a consumer build |
| `FINCORE_ENABLE_ASAN` | `OFF` | AddressSanitizer on fincore targets |
| `FINCORE_ENABLE_UBSAN` | `OFF` | UndefinedBehaviorSanitizer on fincore targets |
| `FINCORE_BUILD_IMPLOT` | `OFF` | Builds the ImPlot adapter; needs GUI dependencies |
| `FINCORE_BUILD_IMPLOT_DEMO` | `OFF` | Builds the demo; requires `FINCORE_BUILD_IMPLOT=ON` |
| `FINCORE_DATAFRAME_SOURCE_DIR` | empty | Uses a caller-provided DataFrame checkout instead of the submodule |

A sanitizer build is a separate configuration; do not mix it into a release tree:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFINCORE_ENABLE_ASAN=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

## Install and consume

```bash
cmake --install build --prefix /path/to/prefix
```

That writes the headers under `<prefix>/include/fincore`, the library under
`<prefix>/lib` (or `bin` on Windows) and the CMake package under
`<prefix>/lib/cmake/fincore`, with `fincoreConfig.cmake`,
`fincoreConfigVersion.cmake` (same-major compatibility) and the exported targets.

```cmake
find_package(fincore CONFIG REQUIRED COMPONENTS fincore)
target_link_libraries(my_target PRIVATE fincore::fincore)
```

The installed package calls `find_dependency(DataFrame CONFIG REQUIRED)`, so the
consumer environment must also be able to find the DataFrame package that the
library was built against. Point `CMAKE_PREFIX_PATH` at both prefixes if they are
not installed side by side.

The optional ImPlot component is exported separately and only exists when the
library was built with `FINCORE_BUILD_IMPLOT=ON`:

```cmake
find_package(fincore CONFIG REQUIRED COMPONENTS fincore implot)
```

## Verify the install

```cpp
#include <fincore/metrics/basic.hpp>
#include <cstdio>

int main() {
    std::printf("%.0f\n",
                fincore::annualization_factor(fincore::frequency::daily));
    return 0;
}
```

Compile and run it against the installed prefix. If it links, the headers, the
library and the exported target line up; if it fails at configure time, the
`fincore_DIR` or the DataFrame dependency is the thing to inspect.

## Platform notes

- **Windows.** Run CMake from a Visual Studio developer shell so `cl.exe` is on
  `PATH`. Multi-config generators place binaries under `Release/`; pass
  `--config Release` to `cmake --build` and `ctest -C Release`.
- **macOS and Linux.** The pinned submodules build with the system compiler; no
  extra packages are required for the core library.
- **CI.** Windows and macOS are exercised on every push; the GCC 13
  `ubuntu-latest` job has a known constexpr-limit failure recorded in the
  iteration notes.

## Documentation build

The site is one Sphinx project built twice, once per language:

```bash
python -m venv .venv-docs
.venv-docs/bin/pip install -r docs/requirements-docs.txt
cd docs/site
doxygen Doxyfile
python tools/normalize_doxygen_xml.py ../_build/doxygen/xml
sphinx-build -b html -W -n --keep-going . ../_build/html/en
FINCORE_DOCS_LANGUAGE=zh_CN sphinx-build -b html -W -n --keep-going . ../_build/html/zh_CN
cd ../..
python docs/site/tools/check_built_site.py
```

The checker exits non-zero on a broken internal link, a machine path in the built
pages, or an untranslated/fuzzy catalog entry. `docs/site/README.md` records the
maintenance rules, including why a heading must not start with a number.
