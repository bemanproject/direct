# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

find_package(GTest REQUIRED)

add_executable(beman.direct.tests.direct)
target_sources(beman.direct.tests.direct PRIVATE direct.test.cpp)
target_link_libraries(
    beman.direct.tests.direct
    PRIVATE beman::direct GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(beman.direct.tests.direct DISCOVERY_TIMEOUT 60)

# The pimpl boundary is enforced structurally, not just by convention: the
# TU that defines widget::Impl is compiled into its own library, and the test
# program links against it. Nothing but a link symbol crosses between them,
# so the test program cannot accidentally acquire Impl's definition.
#
# TODO: add a SHARED-library variant of this test.
#
# A static library proves the TUs are separately compiled and linked, which
# is what the incomplete-type property needs. It does not exercise the other
# half of the motivation for pimpl: a stable ABI across a shared-library
# boundary, where the point of the fixed-size storage is that rebuilding the
# library after Impl changes shape does not require rebuilding consumers.
#
# Doing this properly needs symbol-visibility handling that differs per
# platform -- __declspec(dllexport)/dllimport on Windows versus
# -fvisibility=hidden plus attributes elsewhere -- and getting the Windows
# side right requires iterating on an actual Windows machine rather than
# guessing. Deferred until that can be done.
add_library(beman.direct.tests.widget STATIC)
target_sources(beman.direct.tests.widget PRIVATE widget_impl.test.cpp)
target_link_libraries(beman.direct.tests.widget PUBLIC beman::direct)
target_include_directories(
    beman.direct.tests.widget
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(beman.direct.tests.incomplete)
target_sources(beman.direct.tests.incomplete PRIVATE incomplete.test.cpp)
target_link_libraries(
    beman.direct.tests.incomplete
    PRIVATE beman.direct.tests.widget GTest::gtest_main
)
gtest_discover_tests(beman.direct.tests.incomplete DISCOVERY_TIMEOUT 60)

function(beman_direct_add_compile_failure_test name source expected_text)
    set(target "beman.direct.tests.${name}")
    add_library("${target}" OBJECT EXCLUDE_FROM_ALL "${source}")
    target_link_libraries("${target}" PRIVATE beman::direct)

    add_test(
        NAME "beman.direct.tests.${name}"
        COMMAND
            "${CMAKE_COMMAND}" "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DTARGET_NAME=${target}" "-DCONFIG=$<CONFIG>"
            "-DEXPECTED_TEXT=${expected_text}" -P
            "${CMAKE_CURRENT_SOURCE_DIR}/expect_compile_failure.cmake"
    )
    set_tests_properties(
        "beman.direct.tests.${name}"
        PROPERTIES
            RESOURCE_LOCK beman_direct_compile_failure_build_tree
            LABELS "compile-failure"
    )
endfunction()

beman_direct_add_compile_failure_test(
    fail_undersized_storage
    "${CMAKE_CURRENT_SOURCE_DIR}/fail_undersized_storage.test.cpp"
    "T does not fit in direct<T, Size, Align>'s storage; increase Size"
)

beman_direct_add_compile_failure_test(
    fail_misaligned_storage
    "${CMAKE_CURRENT_SOURCE_DIR}/fail_misaligned_storage.test.cpp"
    "T's alignment exceeds direct<T, Size, Align>'s Align; increase Align"
)

beman_direct_add_compile_failure_test(
    fail_accessor_incomplete
    "${CMAKE_CURRENT_SOURCE_DIR}/fail_accessor_incomplete.test.cpp"
    "incomplete type"
)

add_library(
    beman.direct.tests.compile_success_fits
    OBJECT
    compile_success_fits.test.cpp
)
target_link_libraries(
    beman.direct.tests.compile_success_fits
    PRIVATE beman::direct
)
add_test(
    NAME beman.direct.tests.compile_success_fits
    COMMAND
        "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target
        beman.direct.tests.compile_success_fits --config $<CONFIG>
)
set_tests_properties(
    beman.direct.tests.compile_success_fits
    PROPERTIES RESOURCE_LOCK beman_direct_compile_failure_build_tree
)
