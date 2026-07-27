// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This TU must fail to compile: Payload (64 bytes) does not fit in an
// 8-byte direct<> storage buffer. See expect_compile_failure.cmake.

#include <beman/direct/direct.hpp>

struct Payload {
    char data[64];
};

void use() {
    beman::direct::direct<Payload, 8, alignof(char)> d(std::in_place);
    (void)d;
}
