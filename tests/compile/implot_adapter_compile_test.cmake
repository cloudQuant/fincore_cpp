if (NOT DEFINED BINARY_DIR OR BINARY_DIR STREQUAL "")
    message(FATAL_ERROR
        "BINARY_DIR must name a configured build directory containing fincore_implot")
endif()

if (NOT IS_DIRECTORY "${BINARY_DIR}")
    message(FATAL_ERROR
        "BINARY_DIR is not a configured build directory: ${BINARY_DIR}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BINARY_DIR}" --target fincore_implot
    RESULT_VARIABLE result)
if (NOT result EQUAL 0)
    message(FATAL_ERROR "fincore_implot did not compile")
endif()
