// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_DIRECT_TESTS_WIDGET_HPP
#define BEMAN_DIRECT_TESTS_WIDGET_HPP

#include <beman/direct/direct.hpp>

#include <cstddef>
#include <string>

namespace beman::direct::tests {

// Impl is forward-declared only. No translation unit that includes this
// header sees its definition: Impl is defined in widget_impl.test.cpp, which
// is compiled into a separate static library that consumers link against.
class widget {
  public:
    widget();
    explicit widget(std::string label);
    widget(const widget&);
    widget(widget&&) noexcept;
    widget& operator=(const widget&);
    widget& operator=(widget&&) noexcept;
    ~widget();

    const std::string& label() const;
    void               set_label(std::string label);

    // Counts how many times set_label has run on this widget. The counter
    // lives inside Impl, so observing it advance from a TU that cannot see
    // Impl shows the real implementation is reached across the boundary.
    int revision() const;

    // Counters maintained by Impl's special members, letting a consumer TU
    // observe exactly which of them ran -- and how often -- without seeing
    // Impl itself. This is what makes the cross-TU tests non-vacuous: a
    // widget whose operations did not reach the real Impl cannot move them.
    static int  live_count() noexcept;
    static int  construct_count() noexcept; // any constructor
    static int  copy_construct_count() noexcept;
    static int  move_construct_count() noexcept;
    static int  copy_assign_count() noexcept;
    static int  move_assign_count() noexcept;
    static void reset_counts() noexcept;

    // sizeof(Impl) and alignof(Impl), reported from the TU where Impl is
    // complete, so a consumer TU can compare them against the reservation.
    static std::size_t impl_size() noexcept;
    static std::size_t impl_align() noexcept;

    // The reservation itself, usable from any TU.
    static constexpr std::size_t storage_size  = 48;
    static constexpr std::size_t storage_align = alignof(std::max_align_t);

  private:
    struct Impl;
    beman::direct::direct<Impl, storage_size, storage_align> impl_;
};

} // namespace beman::direct::tests

#endif // BEMAN_DIRECT_TESTS_WIDGET_HPP
