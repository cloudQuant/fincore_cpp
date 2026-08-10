cmake_minimum_required(VERSION 3.25)

project(fincore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FINCORE_DATAFRAME_SOURCE_DIR "/Users/yunjinqi/Documents/new_projects/DataFrame"
    CACHE PATH "Path to the local DataFrame source tree")
set(FINCORE_IMPLOT_SOURCE_DIR "/Users/yunjinqi/Documents/new_projects/implot"
    CACHE PATH "Path to the local ImPlot source tree")
set(FINCORE_IMGUI_SOURCE_DIR "" CACHE PATH "Path to the Dear ImGui source tree")
option(FINCORE_BUILD_TESTING "Build fincore tests" ON)
option(FINCORE_BUILD_IMPLOT "Build the ImPlot adapter" OFF)
option(FINCORE_BUILD_IMPLOT_DEMO "Build the ImPlot demo" OFF)
option(FINCORE_ENABLE_ASAN "Enable AddressSanitizer for fincore targets" OFF)
option(FINCORE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for fincore targets" OFF)

include(cmake/fincore_dependencies.cmake)
include(cmake/fincore_sanitizers.cmake)

add_library(fincore INTERFACE)
add_library(fincore::fincore ALIAS fincore)

target_link_libraries(fincore INTERFACE DataFrame::DataFrame)
target_include_directories(fincore INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

fincore_enable_sanitizers(fincore)

if(FINCORE_BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
