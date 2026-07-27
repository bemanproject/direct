// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This TU must fail to compile: Payload requires 64-byte alignment but
// the direct<> storage is only declared with 8-byte alignment.

#include <beman/direct/direct.hpp>

struct alignas(64) Payload {
    int x;
};

void use() {
    beman::direct::direct<Payload, 128, 8> d(std::in_place);
    (void)d;
}
