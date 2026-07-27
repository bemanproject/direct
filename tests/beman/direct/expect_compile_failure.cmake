# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Invoked via `cmake -P` from a ctest COMMAND. Builds TARGET_NAME (an
# EXCLUDE_FROM_ALL object library containing exactly one intended
# compile failure) and asserts BOTH that the build failed AND that its
# output contains our own static_assert message -- not merely that
# *some* compiler error occurred.

foreach(required_variable BUILD_DIR TARGET_NAME EXPECTED_TEXT)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} was not provided")
    endif()
endforeach()

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
