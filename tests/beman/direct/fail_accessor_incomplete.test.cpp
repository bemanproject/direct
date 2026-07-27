// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This TU must fail to compile: unlike unique_ptr's accessors,
// direct<T,...>::operator-> requires T to be complete at its point of
// instantiation, because ptr() goes through std::launder, which rejects
// incomplete types. This pins the documented contract (the proposal,
// "Completeness requirements").

#include <beman/direct/direct.hpp>

struct Impl; // never defined in this TU

struct holder {
    beman::direct::direct<Impl, 32, 8> impl_;
    holder();
    ~holder();
};

Impl* get(holder& h) { return h.impl_.operator->(); }
