cmake_minimum_required(VERSION 3.25)

project(fincore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FINCORE_DATAFRAME_SOURCE_DIR "/Users/yunjinqi/Documents/new_projects/DataFrame"
    CACHE PATH "Path to the local DataFrame source tree")
set(FINCORE_IMPLOT_SOURCE_DIR "/Users/yunjinqi/Documents/new_projects/implot"
    CACHE PATH "Path to the local ImPlot source tree")
set(FINCORE_IMGUI_SOURCE_DIR "/Users/yunjinqi/Documents/new_projects/imgui"
    CACHE PATH "Path to the Dear ImGui source tree")
option(FINCORE_BUILD_TESTING "Build fincore tests" ON)
option(FINCORE_BUILD_IMPLOT "Build the ImPlot adapter" OFF)
option(FINCORE_BUILD_IMPLOT_DEMO "Build the ImPlot demo" OFF)
option(FINCORE_ENABLE_ASAN "Enable AddressSanitizer for fincore targets" OFF)
option(FINCORE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for fincore targets" OFF)

include(cmake/fincore_dependencies.cmake)
include(cmake/fincore_sanitizers.cmake)

add_library(fincore
    src/calendar.cpp
    src/dataframe.cpp
    src/metrics/basic.cpp
    src/metrics/bayesian.cpp
    src/metrics/returns.cpp
    src/metrics/drawdown.cpp
    src/metrics/consecutive.cpp
    src/metrics/ratios.cpp
    src/performance/returns.cpp
    src/metrics/stats.cpp
    src/metrics/risk.cpp
    src/metrics/alpha_beta.cpp
    src/metrics/timing.cpp
    src/metrics/yearly.cpp
    src/metrics/rolling.cpp
    src/metrics/perf_stats.cpp
    src/metrics/positions.cpp
    src/metrics/transactions.cpp
    src/metrics/round_trips.cpp
    src/metrics/perf_attrib.cpp
    src/attribution/brinson.cpp
    src/attribution/factor_regression.cpp
    src/attribution/factor_exposures.cpp
    src/attribution/factor_attribution.cpp
    src/attribution/factor_prediction.cpp
    src/attribution/style.cpp
    src/simulation/scenarios.cpp
    src/simulation/correlation.cpp
    src/simulation/parameters.cpp
    src/simulation/result.cpp
    src/simulation/paths.cpp
    src/simulation/monte_carlo.cpp
    src/simulation/bootstrap.cpp
    src/analysis_context.cpp
    src/rolling_engine.cpp)
add_library(fincore::fincore ALIAS fincore)

target_compile_features(fincore PUBLIC cxx_std_23)
target_link_libraries(fincore PUBLIC DataFrame::DataFrame)
target_include_directories(fincore PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

fincore_enable_sanitizers(fincore)

if (FINCORE_BUILD_IMPLOT_DEMO AND NOT FINCORE_BUILD_IMPLOT)
    message(FATAL_ERROR
        "FINCORE_BUILD_IMPLOT_DEMO requires FINCORE_BUILD_IMPLOT=ON")
endif()

if (FINCORE_BUILD_IMPLOT)
    include(cmake/fincore_implot.cmake)
endif()

if(FINCORE_BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
