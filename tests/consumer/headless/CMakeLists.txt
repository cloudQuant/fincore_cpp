cmake_minimum_required(VERSION 3.25)
project(fincore_headless_consumer LANGUAGES CXX)

if(NOT DEFINED EXPECTED_PACKAGE_PREFIX)
    message(FATAL_ERROR "EXPECTED_PACKAGE_PREFIX must be provided")
endif()

find_package(fincore CONFIG REQUIRED COMPONENTS fincore)

function(assert_package_dir_is_under_prefix variable_name package_name)
    if(NOT DEFINED ${variable_name})
        message(FATAL_ERROR "${package_name} did not set ${variable_name}")
    endif()
    file(REAL_PATH "${EXPECTED_PACKAGE_PREFIX}" _expected_prefix)
    file(REAL_PATH "${${variable_name}}" _actual_package_dir)
    cmake_path(IS_PREFIX _expected_prefix "${_actual_package_dir}" NORMALIZE _is_under_prefix)
    if(NOT _is_under_prefix)
        message(FATAL_ERROR
            "${package_name} was loaded from outside the installed prefix: ${${variable_name}}")
    endif()
endfunction()

assert_package_dir_is_under_prefix(fincore_DIR fincore)
assert_package_dir_is_under_prefix(DataFrame_DIR DataFrame)

if(NOT TARGET fincore::fincore)
    message(FATAL_ERROR "fincore::fincore was not exported")
endif()

add_executable(fincore_headless_consumer main.cpp)
target_link_libraries(fincore_headless_consumer PRIVATE fincore::fincore)
