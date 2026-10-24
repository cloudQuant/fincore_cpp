include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS fincore
    EXPORT fincoreTargets
    COMPONENT fincore
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/fincore
    COMPONENT fincore
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp")

install(EXPORT fincoreTargets
    COMPONENT fincore
    FILE fincoreTargets.cmake
    NAMESPACE fincore::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fincore)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fincoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/fincoreConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fincore)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/fincoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/fincoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/fincoreConfigVersion.cmake"
    COMPONENT fincore
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fincore)

if(TARGET fincore_implot)
    set(_fincore_imgui_install_headers)
    foreach(_fincore_header IN LISTS _fincore_imgui_required_headers)
        list(APPEND _fincore_imgui_install_headers
            "${FINCORE_IMGUI_SOURCE_DIR}/${_fincore_header}")
    endforeach()
    set(_fincore_implot_install_headers)
    foreach(_fincore_header IN LISTS _fincore_implot_required_headers)
        list(APPEND _fincore_implot_install_headers
            "${FINCORE_IMPLOT_SOURCE_DIR}/${_fincore_header}")
    endforeach()

    install(TARGETS fincore_implot imgui implot
        EXPORT fincoreImPlotTargets
        COMPONENT implot
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(EXPORT fincoreImPlotTargets
        COMPONENT implot
        FILE fincoreImPlotTargets.cmake
        NAMESPACE fincore::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fincore)

    install(FILES
        ${_fincore_imgui_install_headers}
        COMPONENT implot
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fincore/third_party/imgui)
    install(FILES
        ${_fincore_implot_install_headers}
        COMPONENT implot
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fincore/third_party/implot)
    install(FILES "${FINCORE_IMGUI_SOURCE_DIR}/LICENSE.txt"
        COMPONENT implot
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/fincore/implot
        RENAME imgui-LICENSE.txt)
    install(FILES "${FINCORE_IMPLOT_SOURCE_DIR}/LICENSE"
        COMPONENT implot
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/fincore/implot
        RENAME implot-LICENSE)
endif()

install(FILES README.md LICENSE
    COMPONENT fincore
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/fincore)
