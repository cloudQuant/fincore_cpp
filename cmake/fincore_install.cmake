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

install(FILES README.md LICENSE
    COMPONENT fincore
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/fincore)
