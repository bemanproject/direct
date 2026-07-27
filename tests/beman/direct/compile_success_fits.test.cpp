// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Adjacent positive control for the negative-compilation tests: proves
// the harness isn't just permanently red/green regardless of correctness.

#include <beman/direct/direct.hpp>

#include <utility>

struct Payload {
    char data[64];
};

void use() {
    beman::direct::direct<Payload, 64, alignof(char)> d(std::in_place);
    (void)d;
}
