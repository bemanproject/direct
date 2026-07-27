// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This TU must fail to compile: unlike unique_ptr's accessors,
// direct<T,...>::operator-> requires T to be complete at its point of
// instantiation, because ptr() goes through std::launder, which rejects
// incomplete types. That requirement is the documented contract (the
// proposal, "Completeness requirements").
//
// The marker this test matches is our own static_assert message, so what is
// pinned is that *this implementation* names direct and the member at fault
// rather than emitting a bare launder error. That is quality of
// implementation. The proposal guarantees only that the violation is
// ill-formed -- see "How far the requirement can be enforced" -- so no
// program should depend on the wording, and a different implementation
// failing this test is not non-conforming.

#include <beman/direct/direct.hpp>

struct Impl; // never defined in this TU

struct holder {
    beman::direct::direct<Impl, 32, 8> impl_;
    holder();
    ~holder();
};

Impl* get(holder& h) { return h.impl_.operator->(); }
