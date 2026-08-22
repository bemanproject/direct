# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

target_sources(
    beman.direct
    PUBLIC
        FILE_SET HEADERS
            BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
            FILES direct.hpp detail/config.hpp
)
