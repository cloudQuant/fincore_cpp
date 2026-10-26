# fincore C++ Phase 3C ImPlot Activation Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Turn the supplied local Dear ImGui checkout into a reproducibly
verified optional ImPlot adapter and GLFW/OpenGL demo build, while preserving
the headless DataFrame analytics core.

**Architecture:** Retain optional graphics flags. Set the supplied ImGui path
as the local cache default, register the existing positive adapter compile
contract only when the adapter target exists, and use fresh positive
AppleClang/Homebrew LLVM builds for acceptance. Documentation records the
positive compilation boundary; it does not claim GUI runtime automation.

**Tech Stack:** CMake 3.25+, C++23, cloudQuant/DataFrame, local ImPlot,
user-supplied local Dear ImGui, GLFW, OpenGL, GoogleTest/CTest, and Homebrew
LLVM 22 for ASan+UBSan verification.

---

## Contract shared by every task

- `FINCORE_BUILD_IMPLOT` and `FINCORE_BUILD_IMPLOT_DEMO` remain OFF by default.
  The core target remains headless and links no ImGui, ImPlot, GLFW, or OpenGL
  dependency when both options are OFF.
- `FINCORE_IMGUI_SOURCE_DIR` defaults to
  `/Users/yunjinqi/Documents/new_projects/imgui`, the real user-supplied
  checkout. It remains a cache path that callers can override. Never fetch,
  clone, vendor, fabricate, or substitute dependencies.
- Positive adapter compilation requires the existing ImGui core and stb files
  plus ImPlot sources. Positive demo compilation additionally requires the
  real GLFW/OpenGL backend files. Do not run the GUI demo as an automated test
  or create a hidden window.
- The adapter's public header stays separate from `metrics.hpp` and the core
  target. The caller continues to own current Dear ImGui/ImPlot contexts.
- An invalid explicit ImGui override must continue to fail with the precise
  CMake diagnostic. A positive result proves configure/build/link and the
  compile contract, not an interactive render screenshot.
- Only public headers may be used by compile consumers. Preserve every
  accepted DataFrame metric, rolling facade, CMake sanitizer behavior, and
  existing historical acceptance evidence.

### Task 27: Register the positive optional-build contract

**Files:**

- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Establish the current positive baseline**

With the user-supplied ImGui path, configure/build a fresh positive adapter
and demo directory. Record that `fincore_implot` and `fincore_implot_demo`
compile. Record the previous absence of a registered positive `CTest` compile
contract in a positive test configuration; do not temporarily remove accepted
adapter behavior to manufacture a source RED.

**Step 2: Implement the minimum CMake contract**

Change only the `FINCORE_IMGUI_SOURCE_DIR` cache default to the supplied local
path. In `tests/CMakeLists.txt`, conditionally register exactly one
`ImPlotAdapterCompile` test only when `TARGET fincore_implot` exists. Invoke
the existing `tests/compile/implot_adapter_compile_test.cmake` with the actual
configured binary directory. Do not register it in a default headless build,
change normal target links, add GUI runtime tests, or alter the adapter/demo
implementation.

**Step 3: Run focused verification**

Use a fresh positive `FINCORE_BUILD_TESTING=ON`,
`FINCORE_BUILD_IMPLOT=ON`, `FINCORE_BUILD_IMPLOT_DEMO=ON` build with the local
ImGui and ImPlot paths. Its CTest run must include and pass
`ImPlotAdapterCompile`; the full build must compile the demo. Verify a fresh
headless build remains free of the optional test/targets, and an invalid ImGui
override still fails with the documented diagnostic.

**Step 4: Commit**

```sh
git add CMakeLists.txt tests/CMakeLists.txt
git commit -m "test: register ImPlot adapter build contract"
```

### Task 28: Accept the positive ImPlot build boundary

**Files:**

- Modify: `README.md`
- Modify: `docs/design/2026-08-10-fincore-cpp-dataframe-implot-design.md`
- Create: `docs/acceptance/2026-08-11-phase-3c-implot-activation-acceptance.md`

**Step 1: Create evidence-first acceptance documentation**

Create the acceptance record before final documentation. It must identify the
Task 27 commit, the Task 28 documentation commit once known (use a follow-up
instead of amending if necessary), user-supplied ImGui path, exact positive
core/adapter/demo commands and results, registered CTest evidence, headless
non-regression, sanitizer results, invalid override diagnostic, compiler
versions, and the no-GUI-runtime boundary.

**Step 2: Publish the current optional-build instructions**

Replace README's obsolete “positive gates remain open” language with the real
local path and successful compile boundary. Preserve examples showing explicit
cache overrides. Update the foundational design's status, ImGui cache-default
table, and positive/negative gate wording without changing historical dated
acceptance documents.

**Step 3: Run final fresh acceptance**

In separate `mktemp -d` directories run:

```sh
cmake -S . -B <positive-core> -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui
cmake --build <positive-core> --parallel 4
ctest --test-dir <positive-core> --output-on-failure

cmake -S . -B <positive-sanitize> -G Ninja \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui
cmake --build <positive-sanitize> --parallel 4
ctest --test-dir <positive-sanitize> --output-on-failure
```

Also run fresh headless configure/build/CTest and fresh invalid-ImGui
configure. Do not launch the demo under automated test conditions.

**Step 4: Commit**

```sh
git add README.md docs/design/2026-08-10-fincore-cpp-dataframe-implot-design.md \
        docs/acceptance/2026-08-11-phase-3c-implot-activation-acceptance.md
git commit -m "docs: accept positive ImPlot build"
```

## Deferred after this plan

Interactive visual smoke testing on a user display, multi-series/tearsheet
visualizations, multi-column/full-index rolling metrics, portfolio positions
and transactions, round trips, attribution, Bayesian/bootstrap and advanced
risk models, optimization, simulation, reports, data providers, packaging,
release, and full Python public-API parity remain separate work. Completing
this plan is not full fincore project completion.
