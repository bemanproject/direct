// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "widget.hpp"

#include <string>
#include <utility>

// Impl becomes complete here. This is the only TU where
// direct<Impl,32,alignof(std::string)>'s special members are
// instantiated -- which is why widget's special members must stay
// declaration-only in the header and be defined (even if just
// `= default`) here, not inline in widget.hpp.
struct widget::Impl {
    std::string label;
    explicit Impl(std::string l) : label(std::move(l)) {}
};

widget::widget() : impl_(std::in_place, std::string{"default"}) {}
widget::widget(std::string label) : impl_(std::in_place, std::move(label)) {}
widget::widget(const widget&)                = default;
widget::widget(widget&&) noexcept            = default;
widget& widget::operator=(const widget&)     = default;
widget& widget::operator=(widget&&) noexcept = default;
widget::~widget()                            = default;

const std::string& widget::label() const { return impl_->label; }
void               widget::set_label(std::string label) { impl_->label = std::move(label); }
