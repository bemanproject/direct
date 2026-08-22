// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_DIRECT_TESTS_TEST_HELPERS_HPP
#define BEMAN_DIRECT_TESTS_TEST_HELPERS_HPP

#include <compare> // IWYU pragma: keep -- completes the return type of int <=> int
#include <stdexcept>

namespace beman::direct::tests {

// Tracks constructs/destructs/copies/moves so tests can assert on exactly
// how many times each special member ran. Call Tracked::reset() at the
// start of a test that cares about the counts.
struct Tracked {
    inline static int constructs = 0;
    inline static int destructs  = 0;
    inline static int copies     = 0;
    inline static int moves      = 0;

    int  value      = 0;
    bool moved_from = false;

    Tracked() { ++constructs; }
    explicit Tracked(int v) : value(v) { ++constructs; }
    Tracked(const Tracked& o) : value(o.value) {
        ++constructs;
        ++copies;
    }
    Tracked(Tracked&& o) noexcept : value(o.value) {
        o.moved_from = true;
        ++constructs;
        ++moves;
    }
    Tracked& operator=(const Tracked& o) {
        value = o.value;
        ++copies;
        return *this;
    }
    Tracked& operator=(Tracked&& o) noexcept {
        value        = o.value;
        o.moved_from = true;
        ++moves;
        return *this;
    }
    ~Tracked() { ++destructs; }

    bool operator==(const Tracked& o) const { return value == o.value; }
    auto operator<=>(const Tracked& o) const { return value <=> o.value; }

    static void reset() { constructs = destructs = copies = moves = 0; }
};

// Throws from copy construction when armed. There is no memory to
// deallocate on failure (unlike a heap-backed pointee) -- direct<T>'s
// constructors need no try/catch at all for this to be exception-safe.
struct ThrowsOnCopyCtor {
    inline static bool armed = false;

    int value;
    explicit ThrowsOnCopyCtor(int v) : value(v) {}
    ThrowsOnCopyCtor(const ThrowsOnCopyCtor& o) : value(o.value) {
        if (armed)
            throw std::runtime_error("ThrowsOnCopyCtor: copy-ctor boom");
    }
    ThrowsOnCopyCtor(ThrowsOnCopyCtor&&)                 = default;
    ThrowsOnCopyCtor& operator=(const ThrowsOnCopyCtor&) = default;
};

// Throws from copy assignment, after mutating state -- demonstrates that
// direct<T>'s copy-assign simply delegates to T::operator=, inheriting
// whatever guarantee (or lack of one) T itself provides.
struct ThrowsOnCopyAssign {
    inline static bool armed = false;

    int value;
    explicit ThrowsOnCopyAssign(int v) : value(v) {}
    ThrowsOnCopyAssign(const ThrowsOnCopyAssign&) = default;
    ThrowsOnCopyAssign& operator=(const ThrowsOnCopyAssign& o) {
        value = o.value; // mutates before the throw
        if (armed)
            throw std::runtime_error("ThrowsOnCopyAssign: copy-assign boom");
        return *this;
    }
};

} // namespace beman::direct::tests

#endif // BEMAN_DIRECT_TESTS_TEST_HELPERS_HPP
