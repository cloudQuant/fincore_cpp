include(GNUInstallDirs)

function(fincore_embed_report_assets target)
    if (NOT TARGET ${target})
        message(FATAL_ERROR
            "fincore_embed_report_assets requires an existing target: ${target}")
    endif()

    set(_fincore_echarts_asset
        "${PROJECT_SOURCE_DIR}/assets/report/echarts.min.js")
    set(_fincore_echarts_license
        "${PROJECT_SOURCE_DIR}/licenses/echarts-5.5.0/LICENSE.txt")
    set(_fincore_echarts_notice
        "${PROJECT_SOURCE_DIR}/licenses/echarts-5.5.0/NOTICE.txt")
    set(_fincore_echarts_manifest
        "${PROJECT_SOURCE_DIR}/licenses/echarts-5.5.0/manifest.json")
    set(_fincore_echarts_expected_sha256
        "42f8329d989b6f6539dd2b15bbdf0d82025762ac112fbb60dc57b27d7bcf3946")

    foreach(_fincore_file IN ITEMS
            "${_fincore_echarts_asset}"
            "${_fincore_echarts_license}"
            "${_fincore_echarts_notice}"
            "${_fincore_echarts_manifest}")
        if (NOT EXISTS "${_fincore_file}")
            message(FATAL_ERROR "Required report asset is missing: ${_fincore_file}")
        endif()
    endforeach()

    file(SHA256 "${_fincore_echarts_asset}" _fincore_echarts_actual_sha256)
    if (NOT _fincore_echarts_actual_sha256 STREQUAL
            _fincore_echarts_expected_sha256)
        message(FATAL_ERROR
            "Pinned ECharts asset digest mismatch: ${_fincore_echarts_actual_sha256}")
    endif()

    file(READ "${_fincore_echarts_asset}" _fincore_echarts_source)
    string(SUBSTRING "${_fincore_echarts_expected_sha256}" 0 10
        _fincore_echarts_short_sha256)
    set(_fincore_echarts_delimiter
        "FC_EC_${_fincore_echarts_short_sha256}")
    set(_fincore_echarts_terminator
        ")${_fincore_echarts_delimiter}\"")
    string(FIND "${_fincore_echarts_source}"
        "${_fincore_echarts_terminator}" _fincore_echarts_terminator_position)
    if (NOT _fincore_echarts_terminator_position EQUAL -1)
        message(FATAL_ERROR
            "Pinned ECharts source collides with the generated C++ raw string delimiter")
    endif()

    set(_fincore_generated_dir
        "${CMAKE_CURRENT_BINARY_DIR}/generated/fincore/report")
    file(MAKE_DIRECTORY "${_fincore_generated_dir}")
    set(_fincore_generated_source
        "${_fincore_generated_dir}/echarts_asset.cpp")
    file(WRITE "${_fincore_generated_source}"
        "#include <string_view>\n"
        "namespace fincore::detail {\n"
        "std::string_view echarts_asset() noexcept {\n"
        "    return R\"${_fincore_echarts_delimiter}(\n"
        "${_fincore_echarts_source}\n"
        ")${_fincore_echarts_delimiter}\";\n"
        "}\n"
        "}  // namespace fincore::detail\n")
    target_sources(${target} PRIVATE "${_fincore_generated_source}")

    install(FILES "${_fincore_echarts_asset}"
        COMPONENT fincore
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fincore/report/assets")
    install(FILES
        "${_fincore_echarts_license}"
        "${_fincore_echarts_notice}"
        "${_fincore_echarts_manifest}"
        COMPONENT fincore
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/fincore/echarts-5.5.0")
endfunction()
