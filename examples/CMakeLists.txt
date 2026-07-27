# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# widget.cpp is the only TU that sees widget::Impl, so it is built as its own
# library and the program links against it. Compiling both into one
# executable would still work, but it would not demonstrate the property the
# example exists to show: that a consumer never needs Impl's definition.
#
# TODO: a SHARED variant would additionally demonstrate ABI stability across
# a rebuild of the library. Deferred along with the equivalent test; see the
# TODO in tests/beman/direct/CMakeLists.txt for why.
add_library(beman.direct.examples.widget_lib STATIC)
target_sources(beman.direct.examples.widget_lib PRIVATE widget.cpp)
target_link_libraries(beman.direct.examples.widget_lib PUBLIC beman::direct)
target_include_directories(
    beman.direct.examples.widget_lib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(beman.direct.examples.widget)
target_sources(beman.direct.examples.widget PRIVATE widget_main.cpp)
target_link_libraries(
    beman.direct.examples.widget
    PRIVATE beman.direct.examples.widget_lib
)
