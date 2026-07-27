// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This translation unit is compiled into the static library
// beman.direct.tests.widget. It is the ONLY TU in which widget::Impl is
// complete, and therefore the only one that instantiates any member of
// direct<Impl, ...>. Consumers link against the library and never see Impl.

#include "widget.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace beman::direct::tests {

namespace {
int g_live           = 0;
int g_construct      = 0;
int g_copy_construct = 0;
int g_move_construct = 0;
int g_copy_assign    = 0;
int g_move_assign    = 0;
} // namespace

struct widget::Impl {
    std::string label;
    int         revision = 0;

    explicit Impl(std::string l) : label(std::move(l)) {
        ++g_live;
        ++g_construct;
    }
    Impl(const Impl& other) : label(other.label), revision(other.revision) {
        ++g_live;
        ++g_construct;
        ++g_copy_construct;
    }
    Impl(Impl&& other) noexcept : label(std::move(other.label)), revision(other.revision) {
        ++g_live;
        ++g_construct;
        ++g_move_construct;
    }
    Impl& operator=(const Impl& other) {
        label    = other.label;
        revision = other.revision;
        ++g_copy_assign;
        return *this;
    }
    Impl& operator=(Impl&& other) noexcept {
        label    = std::move(other.label);
        revision = other.revision;
        ++g_move_assign;
        return *this;
    }
    ~Impl() { --g_live; }
};

widget::widget() : impl_(std::in_place, std::string{"default"}) {}
widget::widget(std::string label) : impl_(std::in_place, std::move(label)) {}
widget::widget(const widget&)                = default;
widget::widget(widget&&) noexcept            = default;
widget& widget::operator=(const widget&)     = default;
widget& widget::operator=(widget&&) noexcept = default;
widget::~widget()                            = default;

const std::string& widget::label() const { return impl_->label; }

void widget::set_label(std::string label) {
    impl_->label = std::move(label);
    ++impl_->revision;
}

int widget::revision() const { return impl_->revision; }

int widget::live_count() noexcept { return g_live; }
int widget::construct_count() noexcept { return g_construct; }
int widget::copy_construct_count() noexcept { return g_copy_construct; }
int widget::move_construct_count() noexcept { return g_move_construct; }
int widget::copy_assign_count() noexcept { return g_copy_assign; }
int widget::move_assign_count() noexcept { return g_move_assign; }

void widget::reset_counts() noexcept {
    g_live = g_construct = g_copy_construct = g_move_construct = 0;
    g_copy_assign = g_move_assign = 0;
}

std::size_t widget::impl_size() noexcept { return sizeof(Impl); }
std::size_t widget::impl_align() noexcept { return alignof(Impl); }

} // namespace beman::direct::tests
