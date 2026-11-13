# CMake for Iris Header Installation
# 2025 Copyright Ryan Landvater
# Created 3/3/2025
cmake_minimum_required(VERSION 3.11...3.30)

# Iris requires C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(IRIS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/)
set(IRIS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/)
Project (IrisHeadersProj LANGUAGES CXX)
# The pinned third-party versions for every Iris repository (see the file).
include(${CMAKE_CURRENT_LIST_DIR}/cmake/dependencies.cmake)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(IrisHeaders INTERFACE)
target_include_directories(
    IrisHeaders INTERFACE
    $<BUILD_INTERFACE:${IRIS_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_sources(
    IrisHeaders INTERFACE FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include/
    FILES 
    ${IRIS_INCLUDE_DIR}/IrisTypes.hpp
    ${IRIS_INCLUDE_DIR}/IrisCore.hpp
    ${IRIS_INCLUDE_DIR}/IrisCodecTypes.hpp
    ${IRIS_INCLUDE_DIR}/IrisCodecCore.hpp
)
if(NOT ${PROJECT_IS_TOP_LEVEL})
    
endif(NOT ${PROJECT_IS_TOP_LEVEL})

option(IRIS_HEADERS_CHECK_STANDALONE "Compile every header alone and all together" ${PROJECT_IS_TOP_LEVEL})
if (IRIS_HEADERS_CHECK_STANDALONE)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/standalone_headers.cmake)
endif()

# TypeTests_1.3: the shared types' behaviour, with Catch2 at the version and
# hash Iris-Core pins. Separate from IRIS_HEADERS_BUILD_TESTS, which needs
# Highway for the SIMD kernels; the two options merge when those kernels move
# to Iris-Codec (Narrow_3, PinsAndSimdMoved_3.3).
option(IRIS_HEADERS_BUILD_TYPE_TESTS "Build the shared types' Catch2 tests" OFF)
if (IRIS_HEADERS_BUILD_TYPE_TESTS)
    include(FetchContent)
    FetchContent_Declare(Catch2
        URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.15.1.tar.gz
        URL_HASH SHA256=be23a52b85cf04cd9587612147a10b023d59ed9757fa1843cc99e615d6c0893c
        SYSTEM
        EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    enable_testing()
    add_executable(IrisTypesTests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/types_test.cpp
        ${IRIS_SOURCE_DIR}/IrisBuffer.cpp
        ${IRIS_SOURCE_DIR}/IrisAsync.cpp
        ${IRIS_SOURCE_DIR}/IrisMemory.cpp)
    target_include_directories(IrisTypesTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/priv)
    target_compile_definitions(IrisTypesTests PRIVATE IRIS_EXPORT_API=true)
    target_link_libraries(IrisTypesTests PRIVATE IrisHeaders Catch2::Catch2WithMain)
    catch_discover_tests(IrisTypesTests)
endif()

option(IRIS_HEADERS_BUILD_TESTS "Build the Iris-Headers source tests (requires Google Highway)" OFF)
if (IRIS_HEADERS_BUILD_TESTS)
    enable_testing()
    find_package(hwy ${IRIS_DEP_HWY_MIN} REQUIRED)   # the SIMD kernels need >= the floor
    add_executable(
        IrisSIMDTests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/IrisSIMDTests.cpp
        ${IRIS_SOURCE_DIR}/IrisSIMD.cpp
        ${IRIS_SOURCE_DIR}/IrisBuffer.cpp
    )
    target_include_directories(IrisSIMDTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/priv)
    target_compile_definitions(IrisSIMDTests PRIVATE IRIS_EXPORT_API=true)
    target_link_libraries(IrisSIMDTests PRIVATE IrisHeaders hwy::hwy)
    add_test(NAME IrisSIMDTests COMMAND IrisSIMDTests)
endif()

if (${PROJECT_IS_TOP_LEVEL})
    install(
        TARGETS IrisHeaders
        EXPORT IrisHeadersConfig
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Iris
    )
    install(
        EXPORT IrisHeadersConfig
        FILE IrisHeadersConfig.cmake
        NAMESPACE "Iris::"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Iris
    )
endif()
