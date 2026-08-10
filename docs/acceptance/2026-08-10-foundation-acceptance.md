# fincore C++ Foundation Acceptance

## Scope and status

This acceptance record covers the C++23, DataFrame-native foundation through
the first migration batch: the headless analytics library, its CTest suite,
and the optional ImPlot adapter's configuration boundary. It is not a
full-product, full-Python-parity, or graphical-demo acceptance claim.

Verification starts from `5032d69` (`fix: validate optional ImPlot adapter
dependencies`). The checklist was first recorded with every item unchecked;
the evidence below then records the actual verification outcomes.

## Acceptance checklist (recorded before verification)

- [x] A fresh C++23 headless configure/build links `fincore` to
  `DataFrame::DataFrame` without enabling graphics targets.
- [x] A fresh headless CTest run covers the DataFrame boundary, supported core
  metrics, `analysis_context`, and `rolling_engine` with every registered test
  passing.
- [x] Configuring `FINCORE_BUILD_IMPLOT=ON` without a valid local
  `FINCORE_IMGUI_SOURCE_DIR` fails with the intended diagnostic and does not
  fetch, clone, vendor, or fabricate Dear ImGui.
- [ ] The optional `fincore_implot` target compiles when a valid local Dear
  ImGui checkout is supplied. This independent positive-build gate remains
  open; a negative configuration diagnostic does not satisfy it.
- [x] A fresh Homebrew LLVM build with both ASan and UBSan links
  `fincore_tests`, then passes all registered CTest tests without sanitizer
  diagnostics.
- [x] Each supported first-batch C++ public API is mapped to its Python
  behavioral counterpart, and remaining migration phases are inventoried.

## Verification evidence

### Fresh headless C++23 core

The following commands used the default AppleClang 17.0.0.17000603 compiler,
CMake 4.0.2, Ninja 1.12.1, and a previously unused binary directory:

```bash
cmake -S . -B /tmp/fincore-task7-core -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-task7-core --parallel 4
ctest --test-dir /tmp/fincore-task7-core --output-on-failure
```

Each command exited `0`. `fincore` links `DataFrame::DataFrame` publicly; the
generated core test executable linked the locally built DataFrame archive. No
ImGui, ImPlot, GLFW, or OpenGL target was configured. CTest reported `45/45`
passing in 0.34 seconds:

| Test group | Count |
| --- | ---: |
| Build contract | 1 |
| DataFrame boundary | 12 |
| Returns | 8 |
| Drawdown | 4 |
| Ratios | 6 |
| Risk | 3 |
| Alpha/beta | 5 |
| `analysis_context` | 2 |
| `rolling_engine` | 4 |
| **Total** | **45** |

### Sanitizer regression, root cause, and repair

The pre-fix failure was reproduced in a separate clean directory:

```bash
cmake -S . -B /tmp/fincore-task7-sanitize-pre -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_ENABLE_ASAN=ON \
  -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
ninja -C /tmp/fincore-task7-sanitize-pre -v fincore_tests
```

Configuration exited `0`; the build exited `1`. The eight `fincore` object
compilations each carried `-fsanitize=address` and `-fsanitize=undefined`, but
the final `fincore_tests` link invocation carried neither flag. It therefore
failed with unresolved `___asan_*` and `___ubsan_*` runtime symbols.

Root cause: `fincore` is a static library and therefore has no link step.
`PRIVATE target_link_options` did not become an
`INTERFACE_LINK_OPTIONS` usage requirement, leaving the final executable
without the sanitizer runtimes. The helper now retains private compile flags
for static-library objects and publishes only their sanitizer link flags to
the final consumer. No global compiler/linker flags were introduced, and no
graphics target or graphics dependency was enabled.

The fixed clean validation used Homebrew LLVM clang 22.1.8:

```bash
cmake -S . -B /tmp/fincore-task7-sanitize -G Ninja \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_ENABLE_ASAN=ON \
  -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build /tmp/fincore-task7-sanitize --parallel 4
ctest --test-dir /tmp/fincore-task7-sanitize --output-on-failure
```

All three commands exited `0`; CTest reported `45/45` passing in 2.67 seconds
with no ASan or UBSan diagnostic. The generated final link command contained
both `-fsanitize=address` and `-fsanitize=undefined`. GoogleTest emitted one
third-party Clang `-Wcharacter-conversion` warning while it compiled; it was
not a sanitizer diagnostic.

The clean sanitizer build itself is the narrow regression: before the helper
change its `fincore_tests` link step failed, and after the change it links and
executes the full registered suite. No separate production CTest target was
added solely to inspect generated link flags.

### ImPlot dependency boundary

The missing-Dear-ImGui diagnostic was exercised without acquiring any
dependency:

```bash
cmake -S . -B /tmp/fincore-task7-implot-no-imgui -G Ninja \
  -DFINCORE_BUILD_TESTING=OFF \
  -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-task7-missing-imgui
```

The command exited `1` as intended at `cmake/fincore_implot.cmake:47` with:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received '/tmp/fincore-task7-missing-imgui'
```

No Dear ImGui source was fetched, cloned, vendored, or fabricated. The test
deliberately used a nonexistent path, and no user-supplied, in-scope complete
Dear ImGui checkout is available. Unrelated generated dependency artifacts
were not treated as a checkout or used for this task. Accordingly, the
positive `fincore_implot` and
`fincore_implot_demo` compilation gate remains open until a user supplies an
explicit checkout containing the documented Dear ImGui core sources and
headers. It has not been inferred from the negative diagnostic.

## API parity and remaining migration scope

The Python project at `/Users/yunjinqi/Documents/new_projects/fincore` is the
behavioral oracle. The following is a counterpart map, not a declaration that
all Python input types, extensions, or APIs have reached native parity.

| C++ public surface | Python behavioral counterpart | Scope note |
| --- | --- | --- |
| `timestamp`, `dataframe`, `make_series`, `require_double_column`, `align_on_common_index`, `input_error` | pandas `Series`/`DataFrame` data boundary and validation used throughout `fincore.metrics.*` | Native replacement boundary; deliberately no one-to-one Python public helper. |
| `simple_returns`, `cumulative_returns`, `cumulative_return`, `annual_return` | `fincore.metrics.returns.simple_returns`, `.cum_returns`, `.cum_returns_final`, `.annual_return` (delegates to `metrics.yearly`) | Supported core return path. |
| `annual_volatility` | `fincore.metrics.risk.annual_volatility` | Supported scalar, named-column native form. |
| `drawdown_series`, `max_drawdown` | `fincore.metrics.drawdown.max_drawdown` and its underwater/drawdown computations | `drawdown_series` is a native materialized-series helper, not a separately named Python public function. |
| `sharpe_ratio`, `sortino_ratio`, `calmar_ratio` | `fincore.metrics.ratios` functions of the same names | Supported ratio subset only. |
| `historical_value_at_risk`, `conditional_value_at_risk` | `fincore.metrics.risk.value_at_risk` (documented historical VaR), `.conditional_value_at_risk` | Supported historical-risk subset only. |
| `alpha`, `beta` | `fincore.metrics.alpha_beta.alpha`, `.beta` | Native boundary performs explicit timestamp alignment. |
| `analysis_context` | `fincore.core.context.AnalysisContext` | Native value-owning lazy scalar-summary counterpart. |
| `rolling_metric`, `rolling_engine::compute` | `fincore.core.engine.RollingEngine` and `fincore.metrics.rolling` | Supports mean return, volatility, Sharpe, max drawdown, and beta windows. |
| `plot_cumulative_returns`, `plot_drawdown`, `plot_rolling_metric` | `fincore.viz.base.VizBackend` and `fincore.viz.matplotlib_backend.MatplotlibBackend` plotting methods | Native ImPlot adapter; positive compilation remains externally gated. |
| `version_*` | package metadata only | C++ build metadata; no analytics counterpart is claimed. |

The remaining migration sequence is unchanged:

1. Phase 2 — remaining metrics and empyrical facade APIs: `basic`, `stats`,
   `timing`, `yearly`, `positions`, `transactions`, `consecutive`, `bayesian`,
   `perf_attrib`, and `empyrical`.
2. Phase 3 — portfolio optimization and simulation.
3. Phase 4 — attribution plus EVT/GARCH advanced risk models.
4. Phase 5 — reports, data-provider interfaces, and ImPlot tearsheet
   equivalents.
5. Phase 6 — packaging, benchmarks, parity corpus, documentation, and
   release-quality acceptance.

## Gate decision

The headless C++23 core and ASan/UBSan repair are accepted by the evidence
above. This is not full-project acceptance and not full first-phase
acceptance: the positive ImPlot adapter/demo build remains open pending a real
local Dear ImGui checkout supplied by the user.
