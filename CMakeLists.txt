cmake_minimum_required(VERSION 3.15)
project(sap_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

option(SAP_CORE_BUILD_SHARED "Build shared library" ON)
option(SAP_CORE_BUILD_STATIC "Build static library" ON)

set(SAP_CORE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sap_core/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sap_core/result.h
)

set(SAP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/serialization.cpp)

# Shared library
if(SAP_CORE_BUILD_SHARED)
    add_library(sap_core_shared SHARED 
        ${SAP_CORE_SOURCES}
    )
    target_include_directories(sap_core_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    if(WIN32)
        target_link_libraries(sap_core_shared PRIVATE ws2_32)
    endif()

    set_target_properties(sap_core_shared PROPERTIES
        OUTPUT_NAME sap_core
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endif()

# Static library
if(SAP_CORE_BUILD_STATIC)
    add_library(sap_core_static STATIC ${SAP_CORE_SOURCES})
    target_include_directories(sap_core_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    if(WIN32)
        target_link_libraries(sap_core_static PRIVATE ws2_32)
    endif()

    set_target_properties(sap_core_static PROPERTIES
        OUTPUT_NAME sap_core
        ARCHIVE_OUTPUT_NAME sap_core_static
    )
endif()

# Create alias
if(SAP_CORE_BUILD_SHARED)
    add_library(sap::core ALIAS sap_core_shared)
elseif(SAP_CORE_BUILD_STATIC)
    add_library(sap::core ALIAS sap_core_static)
endif()

# Installation
option(SAP_CORE_INSTALL "Install library" ON)
if(SAP_CORE_INSTALL)
    include(GNUInstallDirs)

    if(SAP_CORE_BUILD_SHARED)
        install(TARGETS sap_core_shared
            EXPORT sap_core_targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    if(SAP_CORE_BUILD_STATIC)
        install(TARGETS sap_core_static
            EXPORT sap_core_targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT sap_core_targets
        FILE sap_core_targets.cmake
        NAMESPACE sap::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sap_core
    )

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/sap_core_config_version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sap_core_config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/sap_core_config.cmake"
        @ONLY
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/sap_core_config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/sap_core_config_version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sap_core
    )
endif()
