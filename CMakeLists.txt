cmake_minimum_required(VERSION 3.15)
project(sap_core VERSION 1.0.0 LANGUAGES CXX)

# Guard against being added more than once via nested submodules.
# Consumers can safely call add_subdirectory(sap_core) unconditionally —
# the second and subsequent invocations become no-ops.
if(TARGET sap::core)
    return()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

option(SAP_CORE_BUILD_SHARED "Build shared library" ON)
option(SAP_CORE_BUILD_STATIC "Build static library" ON)
option(SAP_CORE_BUILD_TESTS "Build tests" ON)
option(SAP_CORE_BUILD_BENCHMARKS "Build benchmarks" OFF)

set(SAP_CORE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sap_core/types.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sap_core/result.h
)

set(SAP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/job_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/serialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/timer.cpp
    $<$<PLATFORM_ID:Linux>:${CMAKE_CURRENT_SOURCE_DIR}/src/posix_clock.cpp>
    $<$<PLATFORM_ID:Linux>:${CMAKE_CURRENT_SOURCE_DIR}/src/io/epoll_reactor.cpp>
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/src/windows_clock.cpp>
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/src/io/iocp_reactor.cpp>
)

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

    # dllexport while building the DLL; consumers see dllimport.
    target_compile_definitions(sap_core_shared PRIVATE SAP_CORE_EXPORTS $<$<OR:$<CONFIG:Debug>,$<BOOL:${SAP_CORE_BUILD_TESTS}>>:SAP_CORE_ARENA_STATS>)

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

    # Static lib + its consumers: API macro is a no-op.
    target_compile_definitions(sap_core_static PUBLIC SAP_CORE_STATIC_DEFINE $<$<OR:$<CONFIG:Debug>,$<BOOL:${SAP_CORE_BUILD_TESTS}>>:SAP_CORE_ARENA_STATS>)

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

# Tests
if(SAP_CORE_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(sap_core_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/arena_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/allocator_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/stack_allocator_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/result_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixed_string_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/spsc_queue_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/generational_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/container_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/smart_ptr_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/serialization_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/job_system_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/types_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/clock_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/timer_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/task_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/when_all_tests.cpp
        $<$<PLATFORM_ID:Linux>:${CMAKE_CURRENT_SOURCE_DIR}/tests/reactor_tests.cpp>
    )

    if(SAP_CORE_BUILD_STATIC)
        target_link_libraries(sap_core_tests PRIVATE sap_core_static)
    else()
        target_link_libraries(sap_core_tests PRIVATE sap_core_shared)
    endif()

    target_link_libraries(sap_core_tests PRIVATE
        GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(sap_core_tests)
endif()

# Benchmarks
if(SAP_CORE_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.4
    )
    FetchContent_MakeAvailable(benchmark)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(benchmark PRIVATE -Wno-c2y-extensions)
        target_compile_options(benchmark_main PRIVATE -Wno-c2y-extensions)
    endif()

    file(GLOB SAP_CORE_BENCHMARK_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp
    )
    if(NOT SAP_CORE_BENCHMARK_SOURCES)
        # Placeholder so the target is valid before any benchmark .cpp files exist.
        set(SAP_CORE_BENCHMARK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/main.cpp)
    endif()

    add_executable(sap_core_benchmarks ${SAP_CORE_BENCHMARK_SOURCES})

    if(SAP_CORE_BUILD_STATIC)
        target_link_libraries(sap_core_benchmarks PRIVATE sap_core_static)
    else()
        target_link_libraries(sap_core_benchmarks PRIVATE sap_core_shared)
    endif()

    target_link_libraries(sap_core_benchmarks PRIVATE benchmark::benchmark_main)
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
