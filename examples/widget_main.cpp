// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "widget.hpp"

#include <cassert>
#include <iostream>
#include <utility>

int main() {
    widget w1("hello");
    widget w2 = w1; // deep copy
    w2.set_label("world");

    assert(w1.label() == "hello");
    assert(w2.label() == "world");

    const widget w3 = std::move(w1);
    assert(w3.label() == "hello");

    std::cout << "w2.label() = " << w2.label() << '\n';
    std::cout << "w3.label() = " << w3.label() << '\n';
    std::cout << "sizeof(widget) = " << sizeof(widget) << '\n';
}
