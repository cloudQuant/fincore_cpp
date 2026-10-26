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

    # Hex encoding keeps the generation binary-safe on Windows (CMake text
    # writes translate newlines) and avoids the MSVC 64 KiB string-literal cap.
    file(READ "${_fincore_echarts_asset}" _fincore_echarts_hex HEX)
    string(LENGTH "${_fincore_echarts_hex}" _fincore_echarts_hex_length)
    math(EXPR _fincore_echarts_size "${_fincore_echarts_hex_length} / 2")

    set(_fincore_echarts_chunks "")
    set(_fincore_echarts_offset 0)
    # MSVC truncates single literals above ~16 KiB even with /utf-8.
    set(_fincore_echarts_chunk_hex_chars 16000)
    while(_fincore_echarts_offset LESS _fincore_echarts_hex_length)
        string(SUBSTRING "${_fincore_echarts_hex}"
            ${_fincore_echarts_offset} ${_fincore_echarts_chunk_hex_chars}
            _fincore_echarts_chunk)
        string(APPEND _fincore_echarts_chunks
            "    \"${_fincore_echarts_chunk}\",\n")
        math(EXPR _fincore_echarts_offset
            "${_fincore_echarts_offset} + ${_fincore_echarts_chunk_hex_chars}")
    endwhile()

    set(_fincore_generated_dir
        "${CMAKE_CURRENT_BINARY_DIR}/generated/fincore/report")
    file(MAKE_DIRECTORY "${_fincore_generated_dir}")
    set(_fincore_generated_source
        "${_fincore_generated_dir}/echarts_asset.cpp")
    file(WRITE "${_fincore_generated_source}"
        "#include <cstddef>\n"
        "#include <string>\n"
        "#include <string_view>\n"
        "namespace fincore::detail {\n"
        "namespace {\n"
        "constexpr unsigned char hex_digit(const char value) noexcept {\n"
        "    return value <= '9'\n"
        "        ? static_cast<unsigned char>(value - '0')\n"
        "        : static_cast<unsigned char>(value - 'a' + 10);\n"
        "}\n"
        "constexpr const char* echarts_hex_chunks[] = {\n"
        "${_fincore_echarts_chunks}"
        "};\n"
        "constexpr std::size_t echarts_asset_size = ${_fincore_echarts_size};\n"
        "}  // namespace\n"
        "std::string_view echarts_asset() noexcept {\n"
        "    static const std::string asset = [] {\n"
        "        std::string value;\n"
        "        value.reserve(echarts_asset_size);\n"
        "        for (const char* chunk_pointer : echarts_hex_chunks) {\n"
        "            const std::string_view chunk{chunk_pointer};\n"
        "            for (std::size_t index = 0; index < chunk.size();\n"
        "                 index += 2) {\n"
        "                const auto high = hex_digit(chunk[index]);\n"
        "                const auto low = hex_digit(chunk[index + 1]);\n"
        "                value.push_back(\n"
        "                    static_cast<char>((high << 4) | low));\n"
        "            }\n"
        "        }\n"
        "        return value;\n"
        "    }();\n"
        "    return {asset.data(), asset.size()};\n"
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
