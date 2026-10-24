include_guard(GLOBAL)

if(NOT TARGET DataFrame::DataFrame)
    if(FINCORE_DATAFRAME_SOURCE_DIR)
        if(NOT EXISTS "${FINCORE_DATAFRAME_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "FINCORE_DATAFRAME_SOURCE_DIR is not a DataFrame CMake source tree: "
                "${FINCORE_DATAFRAME_SOURCE_DIR}")
        endif()

        add_subdirectory(
            "${FINCORE_DATAFRAME_SOURCE_DIR}"
            "${CMAKE_BINARY_DIR}/_deps/dataframe-build"
            EXCLUDE_FROM_ALL)
    else()
        find_package(DataFrame CONFIG REQUIRED)
    endif()
endif()
