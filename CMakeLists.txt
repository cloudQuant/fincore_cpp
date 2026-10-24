cmake_minimum_required(VERSION 3.25)

project(fincore VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(FINCORE_DATAFRAME_SOURCE_DIR ""
    CACHE PATH "Path to the DataFrame source tree (defaults to third_party/DataFrame)")
set(FINCORE_IMPLOT_SOURCE_DIR ""
    CACHE PATH "Path to the ImPlot source tree (defaults to third_party/implot)")
set(FINCORE_IMGUI_SOURCE_DIR ""
    CACHE PATH "Path to the Dear ImGui source tree (defaults to third_party/imgui)")
option(FINCORE_BUILD_TESTING "Build fincore tests" ON)
option(FINCORE_BUILD_IMPLOT "Build the ImPlot adapter" OFF)
option(FINCORE_BUILD_IMPLOT_DEMO "Build the ImPlot demo" OFF)
option(FINCORE_ENABLE_ASAN "Enable AddressSanitizer for fincore targets" OFF)
option(FINCORE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for fincore targets" OFF)

if(NOT FINCORE_DATAFRAME_SOURCE_DIR AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/DataFrame/CMakeLists.txt")
    set(FINCORE_DATAFRAME_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/DataFrame")
endif()
if(NOT FINCORE_IMPLOT_SOURCE_DIR AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot/implot.h")
    set(FINCORE_IMPLOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/implot")
endif()
if(NOT FINCORE_IMGUI_SOURCE_DIR AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.h")
    set(FINCORE_IMGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
endif()

include(cmake/fincore_dependencies.cmake)
include(cmake/fincore_report_assets.cmake)
include(cmake/fincore_sanitizers.cmake)

add_library(fincore
    src/calendar.cpp
    src/dataframe.cpp
    src/errors.cpp
    src/data/contracts.cpp
    src/data/providers.cpp
    src/data/snapshots.cpp
    src/extensions/snapshot.cpp
    src/extensions/discovery.cpp
    src/metrics/basic.cpp
    src/metrics/helpers.cpp
    src/metrics/frequencies.cpp
    src/metrics/bayesian.cpp
    src/metrics/returns.cpp
    src/metrics/drawdown.cpp
    src/metrics/consecutive.cpp
    src/metrics/ratios.cpp
    src/performance/returns.cpp
    src/performance/cashflows.cpp
    src/performance/inference.cpp
    src/performance/disclosures.cpp
    src/portfolio/capacity.cpp
    src/factor/calendar.cpp
    src/factor/data.cpp
    src/factor/performance.cpp
    src/factor/costs.cpp
    src/factor/analysis.cpp
    src/factor/report.cpp
    src/factor/research.cpp
    src/factor/tears.cpp
    src/factor/pit.cpp
    src/factor/portfolio.cpp
    src/factor/artifacts.cpp
    src/factor/serialization.cpp
    src/factor/timedelta.cpp
    src/factor/workflows.cpp
    src/report/models.cpp
    src/report/html.cpp
    src/report/risk.cpp
    src/report/xlsx.cpp
    src/report/portfolio.cpp
    src/report/renderers.cpp
    src/report/pdf.cpp
    src/runtime/runtime.cpp
    src/runtime/operations_metrics.cpp
    src/runtime/operations_portfolio.cpp
    src/runtime/operations_factor.cpp
    src/runtime/utilities.cpp
    src/runtime/serialization.cpp
    src/viz/chart_data.cpp
    src/viz/render_model.cpp
    src/viz/plotly_document.cpp
    src/viz/bokeh_document.cpp
    src/viz/html_builder.cpp
    src/optimization/numerics.cpp
    src/optimization/portfolio_solver.cpp
    src/optimization/objectives.cpp
    src/optimization/frontier.cpp
    src/optimization/risk_parity.cpp
    src/detail/digest.cpp
    src/detail/mt19937.cpp
    src/version.cpp
    src/operations.cpp
    src/risk/garch.cpp
    src/risk/evt.cpp
    src/risk/backtesting.cpp
    src/risk/walk_forward.cpp
    src/risk/specs.cpp
    src/risk/report.cpp
    src/risk/models.cpp
    src/metrics/stats.cpp
    src/metrics/risk.cpp
    src/metrics/alpha_beta.cpp
    src/metrics/timing.cpp
    src/metrics/yearly.cpp
    src/metrics/rolling.cpp
    src/metrics/perf_stats.cpp
    src/portfolio/positions.cpp
    src/portfolio/transactions.cpp
    src/portfolio/round_trips.cpp
    src/metrics/perf_attrib.cpp
    src/attribution/brinson.cpp
    src/attribution/factor_regression.cpp
    src/attribution/factor_exposures.cpp
    src/attribution/factor_attribution.cpp
    src/attribution/factor_prediction.cpp
    src/attribution/style.cpp
    src/attribution/fama_french.cpp
    src/attribution/fama_french_patch.cpp
    src/attribution/performance.cpp
    src/simulation/scenarios.cpp
    src/simulation/base.cpp
    src/simulation/correlation.cpp
    src/simulation/parameters.cpp
    src/simulation/result.cpp
    src/simulation/paths.cpp
    src/simulation/monte_carlo.cpp
    src/simulation/bootstrap.cpp
    src/analysis_context.cpp
    src/rolling_engine.cpp)
add_library(fincore::fincore ALIAS fincore)
fincore_embed_report_assets(fincore)

target_compile_features(fincore PUBLIC cxx_std_23)
target_link_libraries(fincore PUBLIC DataFrame::DataFrame)
target_include_directories(fincore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

fincore_enable_sanitizers(fincore)

add_executable(fincore_factor_benchmark EXCLUDE_FROM_ALL
    benchmarks/factor_pipeline_benchmark.cpp)
target_link_libraries(fincore_factor_benchmark PRIVATE fincore)
fincore_enable_sanitizers(fincore_factor_benchmark)

add_executable(fincore_report_render EXCLUDE_FROM_ALL
    examples/render_factor_report.cpp)
target_link_libraries(fincore_report_render PRIVATE fincore)
fincore_enable_sanitizers(fincore_report_render)

add_executable(fincore_i8_artifacts EXCLUDE_FROM_ALL
    examples/render_iteration8_artifacts.cpp)
target_link_libraries(fincore_i8_artifacts PRIVATE fincore)
fincore_enable_sanitizers(fincore_i8_artifacts)

if (FINCORE_BUILD_IMPLOT_DEMO AND NOT FINCORE_BUILD_IMPLOT)
    message(FATAL_ERROR
        "FINCORE_BUILD_IMPLOT_DEMO requires FINCORE_BUILD_IMPLOT=ON")
endif()

if (FINCORE_BUILD_IMPLOT)
    include(cmake/fincore_implot.cmake)
endif()

include(cmake/fincore_install.cmake)

if(FINCORE_BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
