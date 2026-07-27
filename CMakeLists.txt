# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.30...4.3)

project(
    beman.direct
    DESCRIPTION "Inline-storage pimpl vocabulary type"
    LANGUAGES CXX
    VERSION 0.1.0
)

# [CMAKE.SKIP_TESTS]
option(
    BEMAN_DIRECT_BUILD_TESTS
    "Enable building tests and test infrastructure. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# [CMAKE.SKIP_EXAMPLES]
option(
    BEMAN_DIRECT_BUILD_EXAMPLES
    "Enable building examples. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

# for find of beman_install_library and configure_build_telemetry
include(infra/cmake/beman-install-library.cmake)
include(infra/cmake/BuildTelemetryConfig.cmake)
include(cmake/features.cmake)

# Feature detection
beman_direct_check_concepts(COMPILER_SUPPORTS_CONCEPTS)
beman_direct_check_three_way_comparison(COMPILER_SUPPORTS_THREE_WAY)

option(
    BEMAN_DIRECT_USE_CONCEPTS
    "Make use of C++20 concepts. Turn this off for non-conforming compilers."
    ${COMPILER_SUPPORTS_CONCEPTS}
)
option(
    BEMAN_DIRECT_USE_THREE_WAY_COMPARISON
    "Make use of C++20 three-way comparison (<=>). Turn this off for non-conforming compilers."
    ${COMPILER_SUPPORTS_THREE_WAY}
)

configure_file(
    "${PROJECT_SOURCE_DIR}/include/beman/direct/detail/config_generated.hpp.in"
    "${PROJECT_BINARY_DIR}/include/beman/direct/detail/config_generated.hpp"
    @ONLY
)

add_library(beman.direct INTERFACE)
add_library(beman::direct ALIAS beman.direct)

target_sources(
    beman.direct
    PUBLIC
        FILE_SET HEADERS
            BASE_DIRS
                "${CMAKE_CURRENT_SOURCE_DIR}/include"
                "${PROJECT_BINARY_DIR}/include"
            FILES
                "${PROJECT_BINARY_DIR}/include/beman/direct/detail/config_generated.hpp"
)

set_target_properties(
    beman.direct
    PROPERTIES VERIFY_INTERFACE_HEADER_SETS ${PROJECT_IS_TOP_LEVEL}
)

add_subdirectory(include/beman/direct)

beman_install_library(beman.direct TARGETS beman.direct)
configure_build_telemetry()

if(BEMAN_DIRECT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/beman/direct)
endif()

if(BEMAN_DIRECT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
