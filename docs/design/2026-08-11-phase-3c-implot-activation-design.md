# fincore C++ Phase 3C ImPlot Activation Design

## Purpose and bounded scope

The user has supplied a valid local Dear ImGui checkout at
`/Users/yunjinqi/Documents/new_projects/imgui`. Phase 3C turns the previously
open positive-ImPlot gate into a reproducible optional-build contract for the
existing DataFrame-native rendering adapter and GLFW/OpenGL dashboard demo.

The phase does not add new metrics, change DataFrame semantics, alter the
existing plotting API, or make graphics a dependency of `fincore`. It verifies
and documents the already implemented optional targets:

- `fincore_implot`, which consumes one DataFrame double column between
  caller-owned current Dear ImGui and ImPlot contexts;
- `fincore_implot_demo`, which owns a GLFW/OpenGL loop solely as an opt-in
  sample application.

## Decision

The supplied ImGui path becomes the default cache value, matching the existing
local defaults for DataFrame and ImPlot. The optional build flags remain OFF,
so a normal core configure remains headless and does not inspect or link
ImGui/ImPlot/GLFW/OpenGL. A caller can still override the cache path, and an
invalid override must retain the precise dependency diagnostic.

When `FINCORE_BUILD_TESTING=ON` and `FINCORE_BUILD_IMPLOT=ON`, CTest registers
the existing compile-contract script as `ImPlotAdapterCompile`. It rebuilds the
already configured `fincore_implot` target, making the positive adapter build
an explicit test without launching a GUI. The dashboard itself is verified by
building its target in a positive demo configuration; it is not run in
automation because interactive GLFW display ownership is deliberately the
application's responsibility.

## Build graph and isolation

```
fincore (headless DataFrame analytics)
    ^
    |   only when FINCORE_BUILD_IMPLOT=ON
fincore_implot (plot_* adapter)
    ^
    |
implot + imgui (local source trees)
    ^
    |   only when FINCORE_BUILD_IMPLOT_DEMO=ON
fincore_implot_demo + GLFW + OpenGL
```

`fincore` continues to link only `DataFrame::DataFrame`; it never exports
ImGui or ImPlot headers. The adapter header remains separate and documents
that callers create/select the current contexts. The demo builds backend
sources from the user-supplied local checkout and owns GLFW initialization,
window creation, render frames, and teardown.

## Positive and negative verification contracts

The primary positive configuration uses a fresh Ninja directory:

```sh
cmake -S . -B <fresh-positive> -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui
cmake --build <fresh-positive> --parallel 4
ctest --test-dir <fresh-positive> --output-on-failure
```

It must compile the core suite, `fincore_implot`, Dear ImGui, ImPlot, and the
demo. `ImPlotAdapterCompile` must pass as a registered CTest. An equivalent
fresh Homebrew LLVM 22 ASan+UBSan configuration confirms sanitizer link
propagation through the optional static graph. macOS OpenGL deprecation
warnings from the demo may be reported by the system headers but are not a
compiler or sanitizer failure.

An explicit invalid `FINCORE_IMGUI_SOURCE_DIR` continues to be configured in a
fresh directory and must fail with the existing actionable diagnostic. Positive
build success proves compilation and linkage only, not a visual screenshot or
automated interactive run. A human application still owns a display and
current ImGui/ImPlot contexts.

## Documentation boundary

The Phase 3C acceptance record will report full commit SHAs, all exact positive
and negative commands, CTest counts, compiler versions, warnings, and the
no-GUI-runtime boundary. README and the foundational architecture design will
be updated to state that the positive adapter/demo compile gate is now accepted
with the supplied local ImGui checkout. Historical acceptance records retain
their time-stamped prior negative-gate evidence; the new record supersedes the
current open status rather than rewriting historical facts.

This phase is not complete fincore parity or project completion. Remaining
Python modules include multi-column/full-index rolling metrics, portfolio
tables, round trips, attribution, Bayesian/bootstrap and risk models,
optimization, simulation, reports/tearsheets, data providers, packaging, and
release-quality project-wide parity.
