# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Invoked via `cmake -P` from a ctest COMMAND. Builds TARGET_NAME (an
# EXCLUDE_FROM_ALL object library containing exactly one intended
# compile failure) and asserts BOTH that the build failed AND that its
# output contains our own static_assert message -- not merely that
# *some* compiler error occurred.

# EXPECTED_TEXT may be empty. A marker is only checkable when the diagnostic
# is one this project emits: our own static_assert text is reproduced
# verbatim by every compiler, but a failure raised inside the standard
# library is worded differently by each one. For those, verifying that the
# build fails is all that is portably available.
foreach(required_variable BUILD_DIR TARGET_NAME)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} was not provided")
    endif()
endforeach()

if(NOT DEFINED EXPECTED_TEXT)
    set(EXPECTED_TEXT "")
endif()

set(build_command
    "${CMAKE_COMMAND}"
    --build
    "${BUILD_DIR}"
    --target
    "${TARGET_NAME}"
)

if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND build_command --config "${CONFIG}")
endif()

execute_process(
    COMMAND ${build_command}
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
    ENCODING UTF-8
)

set(build_output "${build_stdout}\n${build_stderr}")

if(build_result EQUAL 0)
    message(
        FATAL_ERROR
        "Target '${TARGET_NAME}' unexpectedly compiled successfully.\n\n"
        "Build output:\n${build_output}"
    )
endif()

if(EXPECTED_TEXT STREQUAL "")
    message(STATUS "Target '${TARGET_NAME}' failed to compile, as required")
    return()
endif()

string(FIND "${build_output}" "${EXPECTED_TEXT}" marker_position)

if(marker_position EQUAL -1)
    message(
        FATAL_ERROR
        "Target '${TARGET_NAME}' failed to compile, but not with the expected "
        "diagnostic marker.\nExpected marker:\n  ${EXPECTED_TEXT}\n\n"
        "Build output:\n${build_output}"
    )
endif()

message(
    STATUS
    "Target '${TARGET_NAME}' failed with the expected diagnostic marker"
)
