// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Worked example: the pimpl idiom using beman::direct::direct instead of
// std::unique_ptr, avoiding a heap allocation per widget.

#ifndef BEMAN_DIRECT_EXAMPLES_WIDGET_HPP
#define BEMAN_DIRECT_EXAMPLES_WIDGET_HPP

#include <beman/direct/direct.hpp>

#include <string>

// Impl is forward-declared only -- consumers of this header never see
// its definition, and Impl may change size/shape freely without
// recompiling them (as long as it still fits in the reserved 32 bytes).
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

  private:
    struct Impl;
    // 64 bytes for an Impl that currently holds one std::string:
    // libc++: 24 bytes
    // MSVC: 32
    // MSVC (debug): 40
    beman::direct::direct<Impl, 64, alignof(std::max_align_t)> impl_;
};

#endif // BEMAN_DIRECT_EXAMPLES_WIDGET_HPP
