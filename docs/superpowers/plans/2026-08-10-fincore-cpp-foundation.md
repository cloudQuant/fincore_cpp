# fincore C++ Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first executable, DataFrame-native fincore C++23 product
slice: core return/risk analytics, lazy context, rolling calculations, and an
optional ImPlot rendering adapter.

**Architecture:** The public data representation is
`hmdf::StdDataFrame<std::int64_t>`, with a one-column DataFrame representing a
time series. Stateless algorithms read validated typed columns; `analysis_context`
and `rolling_engine` compose those algorithms without reimplementing formulas.
An optional ImPlot target consumes calculated frames but never becomes a core
library dependency.

**Tech Stack:** C++23, CMake 3.25, cloudQuant/DataFrame, GoogleTest/CTest,
cloudQuant/implot, Dear ImGui, GLFW, OpenGL.

## Global Constraints

- C++ standard is exactly C++23 because the supplied DataFrame source requires it.
- Public tables and time series use `hmdf::StdDataFrame<std::int64_t>`; do not
  introduce a custom Series/DataFrame replacement.
- The local DataFrame and ImPlot source paths are CMake cache variables; neither
  dependency is fetched from the network.
- The core target must build without ImGui, ImPlot, GLFW, OpenGL, or a display.
- Every implementation step begins with a focused failing GoogleTest and ends
  with a passing focused test.
- Structural input violations throw `fincore::input_error`; mathematically
  undefined metric values are `std::numeric_limits<double>::quiet_NaN()`.

---

### Task 1: Build system and dependency boundary

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/fincore_dependencies.cmake`
- Create: `cmake/fincore_sanitizers.cmake`
- Create: `include/fincore/version.hpp`
- Create: `tests/CMakeLists.txt`
- Create: `tests/unit/build_contract_test.cpp`

**Interfaces:**
- Consumes: local DataFrame CMake target `DataFrame::DataFrame`.
- Produces: CMake targets `fincore`, `fincore::fincore`, and
  `fincore_tests`; cache options specified in the design.

- [ ] **Step 1: Write the failing build-contract test**

```cpp
#include <gtest/gtest.h>
#include <fincore/version.hpp>

TEST(BuildContract, PublishesSemanticVersion) {
    EXPECT_EQ(fincore::version_major, 0);
    EXPECT_EQ(fincore::version_minor, 1);
    EXPECT_EQ(fincore::version_patch, 0);
}
```

- [ ] **Step 2: Configure and run it to verify the expected failure**

Run:
```bash
cmake -S . -B build -G Ninja -DFINCORE_BUILD_TESTING=ON
cmake --build build --target fincore_tests
ctest --test-dir build -R BuildContract --output-on-failure
```

Expected: configuration or compilation fails because the `fincore` target and
`fincore/version.hpp` do not exist.

- [ ] **Step 3: Add the minimum CMake contract**

```cmake
cmake_minimum_required(VERSION 3.25)
project(fincore VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(cmake/fincore_dependencies.cmake)
add_library(fincore)
add_library(fincore::fincore ALIAS fincore)
target_link_libraries(fincore PUBLIC DataFrame::DataFrame)
target_include_directories(fincore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
```

Define `version_major`, `version_minor`, and `version_patch` as
`inline constexpr int` values in `fincore::version.hpp`. The dependency
module validates `FINCORE_DATAFRAME_SOURCE_DIR` and adds it with a dedicated
binary directory only if `DataFrame::DataFrame` does not already exist.

- [ ] **Step 4: Re-run the focused test**

Run:
```bash
cmake -S . -B build -G Ninja -DFINCORE_BUILD_TESTING=ON
cmake --build build --target fincore_tests
ctest --test-dir build -R BuildContract --output-on-failure
```

Expected: `BuildContract.PublishesSemanticVersion` passes.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt cmake include/fincore/version.hpp tests
git commit -m "build: initialize fincore C++23 project"
```

### Task 2: DataFrame-native series boundary

**Files:**
- Create: `include/fincore/error.hpp`
- Create: `include/fincore/dataframe.hpp`
- Create: `src/dataframe.cpp`
- Create: `tests/unit/dataframe_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `DataFrame::DataFrame`.
- Produces:
  `using fincore::dataframe = hmdf::StdDataFrame<fincore::timestamp>`,
  `make_series(std::vector<timestamp>, std::string_view, std::vector<double>)`,
  `require_double_column(const dataframe&, std::string_view)`, and
  `align_on_common_index(const dataframe&, std::string_view, const dataframe&,
  std::string_view)`.

- [ ] **Step 1: Write failing DataFrame boundary tests**

```cpp
TEST(Dataframe, MakesADataframeNativeSeries) {
    const auto series = fincore::make_series({10, 20}, "returns", {0.01, -0.02});
    EXPECT_EQ(series.get_index(), (std::vector<fincore::timestamp>{10, 20}));
    EXPECT_EQ(series.get_column<double>("returns"),
              (std::vector<double>{0.01, -0.02}));
}

TEST(Dataframe, RejectsMismatchedIndexAndValueLengths) {
    EXPECT_THROW(fincore::make_series({10}, "returns", {0.01, -0.02}),
                 fincore::input_error);
}

TEST(Dataframe, AlignsBenchmarkPairsOnTheirCommonTimestampIndex) {
    const auto left = fincore::make_series({1, 2, 3}, "returns",
                                           {0.01, 0.02, 0.03});
    const auto right = fincore::make_series({2, 3, 4}, "benchmark",
                                            {0.04, 0.06, 0.08});
    const auto [aligned_left, aligned_right] =
        fincore::align_on_common_index(left, "returns", right, "benchmark");
    EXPECT_EQ(aligned_left.get_index(),
              (std::vector<fincore::timestamp>{2, 3}));
    EXPECT_EQ(aligned_right.get_index(),
              (std::vector<fincore::timestamp>{2, 3}));
}
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `ctest --test-dir build -R Dataframe --output-on-failure`

Expected: compilation fails because the boundary API does not exist.

- [ ] **Step 3: Implement the smallest valid boundary**

The implementation moves equal-length index/value vectors into
`hmdf::StdDataFrame<timestamp>::load_index` and `load_column`. It rejects
empty column names, non-equal vector lengths, absent typed columns, and
invalid alignment column requests with `input_error`.
`align_on_common_index` returns the intersection of two named columns' index
values in the left series' original index order, matching the Python
`pd.concat(..., sort=False)` oracle. Return `std::span` over the
DataFrame-owned vector only for the duration of the call.

- [ ] **Step 4: Re-run focused tests**

Run: `ctest --test-dir build -R Dataframe --output-on-failure`

Expected: all `Dataframe` tests pass.

- [ ] **Step 5: Commit**

```bash
git add include/fincore/error.hpp include/fincore/dataframe.hpp src/dataframe.cpp \
  tests/unit/dataframe_test.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add DataFrame-native time series boundary"
```

### Task 3: Return, annualization, and drawdown algorithms

**Files:**
- Create: `include/fincore/metrics/returns.hpp`
- Create: `include/fincore/metrics/drawdown.hpp`
- Create: `src/metrics/returns.cpp`
- Create: `src/metrics/drawdown.cpp`
- Create: `tests/unit/returns_test.cpp`
- Create: `tests/unit/drawdown_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `fincore::dataframe` and `returns_column`.
- Produces:
  `simple_returns`, `cumulative_returns`, `cumulative_return`,
  `annual_return`, `annual_volatility`, `drawdown_series`, and
  `max_drawdown`.

- [ ] **Step 1: Write failing behavior tests**

```cpp
TEST(Returns, ComputesSimpleAndCumulativeReturnsWithOriginalIndex) {
    const auto prices = fincore::make_series({1, 2, 3}, "price", {100., 110., 99.});
    const auto simple = fincore::simple_returns(prices, "price");
    EXPECT_EQ(simple.get_index(), (std::vector<fincore::timestamp>{2, 3}));
    EXPECT_THAT(simple.get_column<double>(fincore::returns_column.data()),
                ::testing::ElementsAre(0.10, -0.10));
    EXPECT_NEAR(fincore::cumulative_return(simple), -0.01, 1e-12);
}

TEST(Drawdown, UsesPeakToTroughCompoundedReturns) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                              {0.10, -0.20, 0.05});
    EXPECT_NEAR(fincore::max_drawdown(returns), -0.20, 1e-12);
}

TEST(Returns, TreatsNanAsZeroDuringCumulativeCompounding) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.02});
    const auto cumulative = fincore::cumulative_returns(returns);
    const auto values = cumulative.get_column<double>(
        fincore::returns_column.data());
    EXPECT_NEAR(values[0], 0.01, 1e-12);
    EXPECT_NEAR(values[1], 0.01, 1e-12);
    EXPECT_NEAR(values[2], 0.0302, 1e-12);
}

TEST(Returns, HandlesEmptyAndNonPositiveEndingWealth) {
    const auto wiped_out = fincore::make_series({1}, "returns", {-1.0});
    const auto empty = fincore::make_series({}, "returns", {});
    EXPECT_DOUBLE_EQ(fincore::annual_return(wiped_out), -1.0);
    EXPECT_TRUE(std::isnan(fincore::annual_return(empty)));
}

TEST(Returns, IgnoresNanSamplesForAnnualVolatility) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.03});
    EXPECT_NEAR(fincore::annual_volatility(returns),
                0.2244994432064365, 1e-12);
}
```

- [ ] **Step 2: Run focused tests to verify they fail**

Run: `ctest --test-dir build -R "Returns|Drawdown" --output-on-failure`

Expected: compilation fails because the metrics headers/functions do not exist.

- [ ] **Step 3: Implement only the tested formulas**

`simple_returns` emits `p[t] / p[t-1] - 1` and removes the first index.
`cumulative_returns` treats a `NaN` return as a zero return and emits
`prod(1 + r[0..t]) - 1`.
`annual_return` is `pow(1 + cumulative_return, annualization / n) - 1` for
positive ending wealth, `-1` for non-positive ending wealth, and `NaN` for an
empty series. `annual_volatility` ignores `NaN` samples, uses sample standard
deviation (`n - 1`) over valid samples, and returns `NaN` if fewer than two
valid observations remain.
`drawdown_series` compares every cumulative wealth value to the running
maximum wealth, and `max_drawdown` returns its minimum.

- [ ] **Step 4: Re-run focused tests**

Run: `ctest --test-dir build -R "Returns|Drawdown" --output-on-failure`

Expected: the return and drawdown test groups pass.

- [ ] **Step 5: Commit**

```bash
git add include/fincore/metrics src/metrics tests/unit CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: implement DataFrame-native return and drawdown metrics"
```

### Task 4: Risk-adjusted ratios and benchmark metrics

**Files:**
- Create: `include/fincore/metrics/ratios.hpp`
- Create: `include/fincore/metrics/risk.hpp`
- Create: `include/fincore/metrics/alpha_beta.hpp`
- Create: `src/metrics/ratios.cpp`
- Create: `src/metrics/risk.cpp`
- Create: `src/metrics/alpha_beta.cpp`
- Create: `tests/unit/ratios_test.cpp`
- Create: `tests/unit/risk_test.cpp`
- Create: `tests/unit/alpha_beta_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  `sharpe_ratio`, `sortino_ratio`, `calmar_ratio`,
  `historical_value_at_risk`, `conditional_value_at_risk`, `beta`, and
  `alpha`.

- [ ] **Step 1: Write failing ratio and benchmark tests**

```cpp
TEST(Ratios, AnnualizesSharpeFromSampleStandardDeviation) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, -0.01});
    EXPECT_NEAR(fincore::sharpe_ratio(returns, 0.0, 252),
                6.928203230275507, 1e-12);
}

TEST(Risk, ComputesHistoricalVarAndTailAverage) {
    const auto returns = fincore::make_series({1, 2, 3, 4, 5}, "returns",
                                              {-0.10, -0.05, 0.00, 0.02, 0.10});
    EXPECT_NEAR(fincore::historical_value_at_risk(returns, 0.20), -0.06, 1e-12);
    EXPECT_NEAR(fincore::conditional_value_at_risk(returns, 0.20), -0.10, 1e-12);
}

TEST(AlphaBeta, UsesCovarianceOverPopulationBenchmarkVariance) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, 0.03});
    const auto benchmark = fincore::make_series({1, 2, 3}, "benchmark",
                                                {0.02, 0.04, 0.06});
    EXPECT_NEAR(fincore::beta(returns, benchmark), 0.5, 1e-12);
}

TEST(AlphaBeta, AlignsDataframesBeforeEstimatingBeta) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, 0.03});
    const auto benchmark = fincore::make_series({2, 3, 4}, "benchmark",
                                                {0.04, 0.06, 0.08});
    EXPECT_NEAR(fincore::beta(returns, benchmark), 0.5, 1e-12);
}
```

- [ ] **Step 2: Run focused tests to verify they fail**

Run: `ctest --test-dir build -R "Ratios|Risk|AlphaBeta" --output-on-failure`

Expected: compilation fails because the metric APIs do not exist.

- [ ] **Step 3: Implement the tested metric contract**

The ratio and annual-volatility family ignore `NaN` samples and require at
least two valid observations. `sharpe_ratio` uses mean excess return divided
by sample standard deviation and annualizes by `sqrt(annualization)`.
`sortino_ratio` uses root-mean-square downside deviation relative to the
required return. `calmar_ratio` is annual return divided by absolute maximum
drawdown. VaR and CVaR return `NaN` when any input is non-finite.
`historical_value_at_risk` uses the same linearly interpolated percentile as
`numpy.percentile`; its `cutoff` is a left-tail probability in `[0, 1]`, not a
confidence level. CVaR averages observations at or below that interpolated
threshold. `beta` first aligns DataFrames on common timestamps and then uses
covariance divided by the benchmark's population variance. `alpha` follows
`pow(1 + mean(returns - beta * benchmark), annualization) - 1` after the same
alignment.

- [ ] **Step 4: Re-run focused tests**

Run: `ctest --test-dir build -R "Ratios|Risk|AlphaBeta" --output-on-failure`

Expected: all ratio, risk, and benchmark tests pass.

- [ ] **Step 5: Commit**

```bash
git add include/fincore/metrics src/metrics tests/unit CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add risk-adjusted and benchmark metrics"
```

### Task 5: Analysis context and rolling engine

**Files:**
- Create: `include/fincore/analysis_context.hpp`
- Create: `include/fincore/rolling_engine.hpp`
- Create: `src/analysis_context.cpp`
- Create: `src/rolling_engine.cpp`
- Create: `tests/unit/analysis_context_test.cpp`
- Create: `tests/unit/rolling_engine_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: all Task 3/4 metrics.
- Produces:
  `analysis_context(dataframe returns, std::optional<dataframe> benchmark,
  annualization)`, `performance_summary`, and
  `rolling_engine::compute(std::span<const rolling_metric>)`.

- [ ] **Step 1: Write failing integration tests**

```cpp
TEST(AnalysisContext, CachesAndPublishesCoreSummary) {
    fincore::analysis_context context{
        fincore::make_series({1, 2, 3}, "returns", {0.01, -0.02, 0.03})};
    const auto summary = context.summary();
    EXPECT_TRUE(summary.contains("Sharpe ratio"));
    EXPECT_NEAR(summary.at("Max drawdown"), -0.02, 1e-12);
    EXPECT_EQ(context.computation_count("Max drawdown"), 1U);
}

TEST(RollingEngine, PreservesWindowEndIndex) {
    fincore::rolling_engine engine{
        fincore::make_series({1, 2, 3, 4}, "returns", {0.01, 0.02, -0.01, 0.03}),
        std::nullopt, 3U};
    const std::array metrics{fincore::rolling_metric::mean_return};
    const auto result = engine.compute(metrics);
    EXPECT_EQ(result.at("mean_return").get_index(),
              (std::vector<fincore::timestamp>{3, 4}));
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `ctest --test-dir build -R "AnalysisContext|RollingEngine" --output-on-failure`

Expected: compilation fails because these classes do not exist.

- [ ] **Step 3: Implement composition without duplicated formulas**

`analysis_context` stores its frame values and caches scalar results in a
`std::unordered_map<std::string, double>`; `summary()` returns a
`std::map<std::string, double>` with stable keys. `rolling_engine` validates
`window >= 2`, returns only complete windows, uses each window's last index,
and delegates to the non-rolling public metric functions. Its valid enum values
are `mean_return`, `volatility`, `sharpe`, `max_drawdown`, and `beta`.

- [ ] **Step 4: Re-run focused tests**

Run: `ctest --test-dir build -R "AnalysisContext|RollingEngine" --output-on-failure`

Expected: both test groups pass.

- [ ] **Step 5: Commit**

```bash
git add include/fincore src tests/unit CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add analysis context and rolling engine"
```

### Task 6: Optional ImPlot rendering adapter and demo

**Files:**
- Create: `cmake/fincore_implot.cmake`
- Create: `include/fincore/viz/implot_backend.hpp`
- Create: `src/viz/implot_backend.cpp`
- Create: `examples/implot_dashboard.cpp`
- Create: `tests/compile/implot_adapter_compile_test.cmake`
- Modify: `CMakeLists.txt`
- Modify: `README.md`

**Interfaces:**
- Consumes: `fincore::dataframe`, local ImPlot source, and a caller-owned
  Dear ImGui context.
- Produces optional target `fincore_implot` and:
  `plot_cumulative_returns`, `plot_drawdown`, and
  `plot_rolling_metric`.

- [ ] **Step 1: Write a failing compile contract**

```cmake
execute_process(
  COMMAND ${CMAKE_COMMAND} --build ${BINARY_DIR} --target fincore_implot
  RESULT_VARIABLE result)
if (NOT result EQUAL 0)
  message(FATAL_ERROR "fincore_implot did not compile")
endif()
```

- [ ] **Step 2: Run the compile contract to verify it fails**

Run:
```bash
cmake -S . -B build-implot -G Ninja -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/path/to/imgui
```

Expected: configuration fails with a precise error that
`FINCORE_IMGUI_SOURCE_DIR` must name a Dear ImGui checkout.

- [ ] **Step 3: Implement the optional target**

Build an `imgui` target from the given ImGui sources, an `implot` target
from `implot.cpp` and `implot_items.cpp`, and link `fincore_implot`
against both. The adapter reads only the requested DataFrame column and calls:

```cpp
if (ImPlot::BeginPlot(title.data())) {
    ImPlot::PlotLine(label.data(), x.data(), y.data(),
                     static_cast<int>(y.size()));
    ImPlot::EndPlot();
}
```

The adapter must return without drawing for an empty frame, and it must throw
`input_error` for absent columns. The GLFW/OpenGL dashboard is built only by
`FINCORE_BUILD_IMPLOT_DEMO=ON` and is not part of core CTest.

- [ ] **Step 4: Re-run the compile test with a valid ImGui source path**

Run:
```bash
cmake -S . -B build-implot -G Ninja -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
  -DFINCORE_IMGUI_SOURCE_DIR=/path/to/imgui
cmake --build build-implot --target fincore_implot
```

Expected: `fincore_implot` compiles without pulling DataFrame analytics into
the graphics backend.

- [ ] **Step 5: Commit**

```bash
git add cmake/fincore_implot.cmake include/fincore/viz src/viz examples \
  tests/compile CMakeLists.txt README.md
git commit -m "feat: add optional ImPlot visualization adapter"
```

### Task 7: Full first-phase verification and migration handoff

**Files:**
- Create: `docs/acceptance/2026-08-10-foundation-acceptance.md`
- Modify: `README.md`
- Modify: `docs/design/2026-08-10-fincore-cpp-dataframe-implot-design.md`

**Interfaces:**
- Consumes: all first-phase targets.
- Produces: reproducible build/test/sanitizer evidence and a phase-2 API
  inventory.

- [ ] **Step 1: Write the acceptance checklist before final verification**

```markdown
- [ ] C++23 core build links DataFrame::DataFrame.
- [ ] CTest covers DataFrame boundary, core metrics, context, and rolling engine.
- [ ] ImPlot target has a dependency diagnostic for missing ImGui.
- [ ] Optional ImPlot target compiles when a valid ImGui checkout is supplied.
- [ ] ASan and UBSan builds pass with the selected compiler.
- [ ] Each supported first-phase C++ API maps to its Python source counterpart.
```

- [ ] **Step 2: Run the full core verification**

Run:
```bash
cmake -S . -B build -G Ninja -DFINCORE_BUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: every registered core test passes.

- [ ] **Step 3: Run a sanitizer build**

Run:
```bash
cmake -S . -B build-sanitize -G Ninja -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

Expected: all tests pass and no sanitizer diagnostic is emitted.

- [ ] **Step 4: Record actual results and scope still remaining**

Replace every checklist item with its actual command, exit status, and test
count. Link first-phase C++ APIs to Python modules and list all remaining
phases from the design document without presenting this phase as full product
parity.

- [ ] **Step 5: Commit**

```bash
git add docs/acceptance README.md docs/design
git commit -m "docs: record fincore C++ foundation acceptance"
```
