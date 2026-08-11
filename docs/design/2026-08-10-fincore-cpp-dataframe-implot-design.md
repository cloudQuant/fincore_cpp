# fincore C++ DataFrame / ImPlot Migration Design

## Status and scope

This document defines the target architecture for a native C++23 implementation
of the full Python `fincore` product. It is not a plan to wrap Python, NumPy,
or pandas. The local Python project at
`/Users/yunjinqi/Documents/new_projects/fincore` remains the behavioral
oracle during migration. The local `cloudQuant/DataFrame` checkout is the
tabular data implementation, and the local `cloudQuant/implot` checkout is
the plotting implementation.

The repository is starting from an empty C++ implementation. The migration is
therefore staged for reviewable, usable releases, but the final scope remains:
metrics, analysis context, rolling calculations, visualization, optimization,
simulation, attribution, risk models, reports, data providers, and tearsheets.

The 2026-08-10 foundation acceptance is deliberately narrower than full phase
or product acceptance. It is a dated record of the headless C++ core and its
sanitizer gate; its former optional-ImPlot OPEN item is superseded, not
rewritten, by the Phase 3C positive compile acceptance in
`docs/acceptance/2026-08-11-phase-3c-implot-activation-acceptance.md`.
Neither record claims full fincore parity or product completion.

## Design goals

1. Public time-series and table inputs/outputs are native
   `hmdf::StdDataFrame` values; pandas is never a runtime dependency.
2. Result semantics match the Python reference for supported APIs, including
   time-index preservation, annualization, NaN propagation, and invalid-input
   handling.
3. Calculations operate on DataFrame columns through stable typed accessors.
   Short-lived `std::span<const double>` views are acceptable internally; no
   home-grown table or Series abstraction becomes a competing public data model.
4. ImPlot is a rendering adapter, not a hard dependency of the core analytics
   library. Headless users can build and test all analytics without GLFW,
   OpenGL, Dear ImGui, or a display server.
5. Builds are reproducible through CMake cache variables that point to the
   local DataFrame and ImPlot checkouts. The project never fetches either
   requested dependency from the network.

## Public data boundary

```cpp
namespace fincore {

using timestamp = std::int64_t;  // UTC Unix nanoseconds
using dataframe = hmdf::StdDataFrame<timestamp>;

inline constexpr std::string_view returns_column{"returns"};
inline constexpr std::string_view benchmark_column{"benchmark"};

dataframe make_series(
    std::vector<timestamp> index,
    std::string_view column_name,
    std::vector<double> values);

std::span<const double> require_double_column(
    const dataframe& frame,
    std::string_view column_name);

std::pair<dataframe, dataframe> align_on_common_index(
    const dataframe& left,
    std::string_view left_column,
    const dataframe& right,
    std::string_view right_column);

}  // namespace fincore
```

Using signed 64-bit UTC nanoseconds produces a portable, sortable DataFrame
index and maps losslessly to Python `DatetimeIndex`. A Series is represented by
a one-column DataFrame, retaining the requested DataFrame-native public model.
Algorithms receiving two time series call an explicit
`align_on_common_index` boundary helper before calculation. It retains only
common timestamps in the left series' original index order and returns two
one-column DataFrames with the same index. This makes the reference library's
pairwise `pd.concat(..., sort=False)` behavior visible and testable rather
than requiring callers to pre-align inputs or silently relying on positional
pairing.

All numeric missing values use IEEE `NaN`. Empty series, absent columns,
non-finite sample handling, zero variance, and insufficient observations are
defined on each public metric. Structural errors throw `input_error`; scalar
results otherwise preserve the Python oracle's metric-specific behavior. Most
undefined values are `NaN`, while the reference Sharpe and Sortino formulas
intentionally yield signed infinity for a nonzero numerator and zero risk
denominator.

## Core analytics architecture

```
DataFrame-native boundary
        |
        +-- data access / validation
        |
        +-- returns (prices, cumulative, annualized)
        +-- descriptive statistics and drawdown
        +-- risk-adjusted ratios and VaR/CVaR
        +-- benchmark relationships (alpha/beta)
        |
        +-- analysis_context (lazy value cache)
        +-- rolling_engine (one DataFrame per named metric)
        |
        +-- implot_backend (optional rendering adapter)
```

Small, stateless functions form the authoritative metrics layer. The
`analysis_context` owns input frames by value and lazily caches only scalar
outputs; its cache cannot observe mutation through a borrowed DataFrame.
`rolling_engine` owns the requested return frames, emits DataFrames indexed
at the final observation of each completed window, and rejects a benchmark
metric when no benchmark frame was supplied.

The first production batch covers the reference library's primary user path:
simple and cumulative returns, annual return and volatility, maximum drawdown,
Sharpe/Sortino/Calmar, historical VaR/CVaR, alpha/beta, `analysis_context`,
and rolling mean/volatility/Sharpe/max drawdown/beta. Later metric modules add
the remaining public functions without changing this boundary.

## Visualization architecture

`implot` itself is immediate-mode drawing code and requires Dear ImGui plus a
windowing/rendering backend. The `fincore_implot` target is consequently
optional and separate from `fincore`.

```
fincore (DataFrame analytics)
    ^
    |
fincore_implot (converts DataFrame columns to ImPlot calls)
    ^
    |
Dear ImGui + ImPlot + GLFW + OpenGL demo backend
```

The adapter exposes rendering calls intended to run between the host
application's `ImGui::NewFrame()` and `ImGui::Render()` operations:

```cpp
void plot_cumulative_returns(const dataframe&, std::string_view column);
void plot_drawdown(const dataframe&, std::string_view column);
void plot_rolling_metric(const dataframe&, std::string_view column,
                         std::string_view label);
```

The demo owns the GLFW/OpenGL event loop. Library code never creates a hidden
window, changes ImGui global context, or assumes a graphics thread. The
adapter requires caller-owned current Dear ImGui and ImPlot contexts. CMake's
`FINCORE_IMGUI_SOURCE_DIR` cache default is the supplied local checkout
`/Users/yunjinqi/Documents/new_projects/imgui`; callers may override that
cache path with another complete checkout. The graphics options remain OFF by
default, so this local default does not make the core a graphics dependency.

Phase 3C accepts adapter and demo compilation with that default path, including
the conditional `ImPlotAdapterCompile` CTest contract. It does not launch the
demo or claim a GUI runtime, hidden-window test, screenshot, or interactive
render. The accepted macOS builds emitted OpenGL deprecation warnings, which
are warnings rather than a no-warning claim. A bad explicit ImGui override is
still a precise negative dependency diagnostic and never justifies fetching,
cloning, vendoring, fabricating, or substituting Dear ImGui source.

## Build, dependencies, and packaging

The top-level project requires CMake 3.25 and C++23. It accepts:

| Variable | Default | Purpose |
| --- | --- | --- |
| `FINCORE_DATAFRAME_SOURCE_DIR` | local supplied DataFrame path | Adds the local CMake DataFrame target. |
| `FINCORE_IMPLOT_SOURCE_DIR` | local supplied ImPlot path | Compiles `implot.cpp` and `implot_items.cpp` only for the optional adapter. |
| `FINCORE_IMGUI_SOURCE_DIR` | `/Users/yunjinqi/Documents/new_projects/imgui` | Default local Dear ImGui checkout for optional ImPlot/demo builds; callers may override the cache path. |
| `FINCORE_BUILD_TESTING` | `ON` | Enables CTest and GoogleTest. |
| `FINCORE_BUILD_IMPLOT` | `OFF` | Enables the ImPlot adapter when a Dear ImGui source path is supplied. |
| `FINCORE_BUILD_IMPLOT_DEMO` | `OFF` | Enables GLFW/OpenGL demo only when every GUI dependency is present. |
| `FINCORE_ENABLE_ASAN`, `FINCORE_ENABLE_UBSAN` | `OFF` | Target-scoped sanitizer flags. |

`DataFrame::DataFrame` is linked by the core library. The project does not
expose DataFrame implementation headers indirectly without declaring it as a
public target dependency. The repository carries Apache-2.0 notices for its
own source and records third-party dependency provenance.

Sanitizer flags remain target-scoped. For a static target, CMake has no link
step for the archive itself, so instrumentation compile options are private to
the archive's objects while sanitizer link options are propagated through its
interface to the final executable. This supplies the ASan/UBSan runtimes where
they are required without imposing global flags or enabling an optional
graphics dependency.

A failing configuration for an invalid explicit `FINCORE_IMGUI_SOURCE_DIR`
proves only the dependency diagnostic. It does not substitute for the
independent positive `fincore_implot` or demo build gate, which Phase 3C
accepts with the real supplied default checkout. `ImPlotAdapterCompile` is
registered only when the optional adapter target exists; headless builds are
free of that test and graphics dependencies. Neither path may acquire Dear
ImGui by fetching, cloning, vendoring, fabricating, or substituting source.

The remaining migration work includes interactive visual smoke testing on a
user display; multi-series and tearsheet visualizations; unimplemented
multi-column and full-index rolling APIs; positions and transactions; round
trips; attribution; Bayesian/bootstrap and advanced risk models; optimization;
simulation; reports; data providers; packaging and release; and full Python
public-API parity.

## Verification strategy

Each capability follows test-first development:

1. A GoogleTest describes the public C++ behavior and expected mathematical
   result.
2. The test is run while its implementation is absent, confirming a meaningful
   failure.
3. The smallest implementation is added.
4. The focused test, full CTest suite, and enabled sanitizer build run.

Golden examples are deterministic, hand-checkable sequences. Python reference
fixtures are used to extend coverage only when a semantic difference cannot be
reasoned about from the documented formula. Tests demonstrate DataFrame index
and column preservation, not just scalar math.

## Full migration sequence

| Phase | Deliverable | Python reference areas |
| --- | --- | --- |
| 1 | DataFrame boundary, core performance/risk metrics, context, rolling engine, ImPlot adapter/demo | `metrics/returns.py`, `risk.py`, `ratios.py`, `drawdown.py`, `alpha_beta.py`, `core/`, `viz/` |
| 2 | Remaining metric families and all empyrical facade APIs | `metrics/basic.py`, `stats.py`, `timing.py`, `yearly.py`, `positions.py`, `transactions.py`, `consecutive.py`, `bayesian.py`, `perf_attrib.py`, `empyrical.py` |
| 3 | Portfolio optimization and simulation | `optimization/`, `simulation/` |
| 4 | Attribution and advanced risk models | `attribution/`, `risk/evt.py`, `risk/garch.py` |
| 5 | Reports, data-provider interfaces, and ImPlot tearsheet equivalents | `report/`, `data/`, `tearsheets/`, `pyfolio.py` |
| 6 | Packaging, benchmarks, parity corpus, documentation, release-quality acceptance | all public APIs |

No phase is considered full-project completion until the final parity inventory
shows each public Python API as implemented, intentionally unsupported with a
documented native alternative, or retired by an approved compatibility policy.
