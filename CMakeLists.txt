cmake_minimum_required(VERSION 3.15)
project(sap_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

option(SAP_CORE_BUILD_SHARED "Build shared library" ON)
option(SAP_CORE_BUILD_STATIC "Build static library" ON)

set(SAP_CORE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core/result.h
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
        OUTPUT_NAME sap_http
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
        OUTPUT_NAME sap_http
        ARCHIVE_OUTPUT_NAME sap_core_static
    )
endif()

# Create alias
if(SAP_CORE_BUILD_SHARED)
    add_library(sap::core ALIAS sap_core_shared)
elseif(SAP_CORE_BUILD_STATIC)
    add_library(sap::core ALIAS sap_core_static)
endif()
