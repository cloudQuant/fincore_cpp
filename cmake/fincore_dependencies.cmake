include_guard(GLOBAL)

if(NOT TARGET DataFrame::DataFrame)
    if(NOT FINCORE_DATAFRAME_SOURCE_DIR)
        message(FATAL_ERROR
            "FINCORE_DATAFRAME_SOURCE_DIR must name a local DataFrame source tree.")
    endif()

    if(NOT EXISTS "${FINCORE_DATAFRAME_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "FINCORE_DATAFRAME_SOURCE_DIR is not a DataFrame CMake source tree: "
            "${FINCORE_DATAFRAME_SOURCE_DIR}")
    endif()

    add_subdirectory(
        "${FINCORE_DATAFRAME_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/dataframe-build"
        EXCLUDE_FROM_ALL)
endif()
