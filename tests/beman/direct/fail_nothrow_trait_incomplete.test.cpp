// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// DOCUMENTS A PROBLEM, NOT A DESIRED PROPERTY. This is expected to change.
//
// direct's non-template special members carry exception specifications that
// depend on T:
//
//     direct(direct&&) noexcept(std::is_nothrow_move_constructible_v<T>);
//
// Those specifications are instantiated lazily, so *declaring* a
// direct<Incomplete, ...> member is fine -- widget.hpp does exactly that and
// the incomplete tests pass. But anything that *queries* the specification
// forces T to be complete, and that query is easy to reach without meaning
// to. The case below is the shortest reproduction; the realistic one is a
// std::vector<Holder>, since vector consults
// is_nothrow_move_constructible_v to choose between moving and copying on
// reallocation.
//
// So `direct` currently has a completeness requirement that is not stated
// anywhere in its interface and that surfaces from a standard container
// rather than from user code. Reported in review by Patrick Roberts -- the trade-off
// is still under discussion, so this test pins current behavior rather than
// intended behavior.
//
// When that is resolved, this file should be deleted and replaced by a
// positive test asserting the query succeeds.

#include <beman/direct/direct.hpp>

#include <type_traits>

struct Incomplete;

// Declaring the member is fine -- that is the property the library provides.
struct Holder {
    beman::direct::direct<Incomplete, 64, 8> impl_;
    Holder();
    ~Holder();
};

// Querying the exception specification is not: this requires Incomplete to
// be complete, and fails to compile.
void use() { static_assert(std::is_nothrow_move_constructible_v<Holder> || true); }
