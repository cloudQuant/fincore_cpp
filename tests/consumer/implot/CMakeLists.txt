cmake_minimum_required(VERSION 3.25)
project(fincore_implot_consumer LANGUAGES CXX)

if(NOT DEFINED EXPECTED_PACKAGE_PREFIX)
    message(FATAL_ERROR "EXPECTED_PACKAGE_PREFIX must be provided")
endif()

find_package(fincore CONFIG REQUIRED COMPONENTS implot)

foreach(_target fincore::fincore fincore::implot fincore::imgui fincore::implot_vendor)
    if(NOT TARGET ${_target})
        message(FATAL_ERROR "${_target} was not exported by the implot component")
    endif()
endforeach()

file(REAL_PATH "${EXPECTED_PACKAGE_PREFIX}" _expected_prefix)
file(REAL_PATH "${fincore_DIR}" _fincore_dir)
file(REAL_PATH "${DataFrame_DIR}" _dataframe_dir)
cmake_path(IS_PREFIX _expected_prefix "${_fincore_dir}" NORMALIZE _fincore_under_prefix)
cmake_path(IS_PREFIX _expected_prefix "${_dataframe_dir}" NORMALIZE _dataframe_under_prefix)
if(NOT _fincore_under_prefix OR NOT _dataframe_under_prefix)
    message(FATAL_ERROR "fincore or DataFrame was loaded from outside the installed prefix")
endif()

add_executable(fincore_implot_consumer main.cpp)
target_link_libraries(fincore_implot_consumer PRIVATE
    fincore::implot
    fincore::imgui
    fincore::implot_vendor)
