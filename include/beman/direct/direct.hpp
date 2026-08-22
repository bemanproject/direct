// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_DIRECT_DIRECT_HPP
#define BEMAN_DIRECT_DIRECT_HPP

#include <beman/direct/detail/config.hpp>

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace beman::direct {

// [direct] Class template direct
template <class T, std::size_t Size, std::size_t Align = alignof(std::max_align_t)>
class direct {
    static_assert(std::is_object_v<T>, "T must be an object type");
    static_assert(!std::is_array_v<T>, "T must not be an array type");
    static_assert(!std::is_const_v<T> && !std::is_volatile_v<T>, "T must not be cv-qualified");
    static_assert(Size > 0, "Size must be positive");
    static_assert(Align > 0 && (Align & (Align - 1)) == 0, "Align must be a power of two");

  public:
    using value_type                   = T;
    static constexpr std::size_t size  = Size;
    static constexpr std::size_t align = Align;

    // [direct.ctor]

    direct() noexcept(std::is_nothrow_default_constructible_v<T>) {
        check_fits();
        static_assert(std::is_default_constructible_v<T>, "T must be default constructible");
        std::construct_at(reinterpret_cast<T*>(storage_));
    }

    template <class U = T>
        requires(!std::is_same_v<std::remove_cvref_t<U>, direct> &&
                 !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>)
    explicit direct(U&& u) noexcept(std::is_nothrow_constructible_v<T, U>) {
        check_fits();
        static_assert(std::is_constructible_v<T, U>, "T must be constructible from U");
        std::construct_at(reinterpret_cast<T*>(storage_), std::forward<U>(u));
    }

    template <class... Args>
    explicit direct(std::in_place_t, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        check_fits();
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible from Args...");
        std::construct_at(reinterpret_cast<T*>(storage_), std::forward<Args>(args)...);
    }

    template <class I, class... Args>
    explicit direct(std::in_place_t,
                    std::initializer_list<I> ilist,
                    Args&&... args) noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<I>&, Args...>) {
        check_fits();
        static_assert(std::is_constructible_v<T, std::initializer_list<I>&, Args...>,
                      "T must be constructible from initializer_list<I>&, Args...");
        std::construct_at(reinterpret_cast<T*>(storage_), ilist, std::forward<Args>(args)...);
    }

    direct(const direct& other) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        check_fits();
        static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");
        std::construct_at(reinterpret_cast<T*>(storage_), *other.ptr());
    }

    direct(direct&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        check_fits();
        static_assert(std::is_move_constructible_v<T>, "T must be move constructible");
        std::construct_at(reinterpret_cast<T*>(storage_), std::move(*other.ptr()));
    }

    // [direct.assign]

    direct& operator=(const direct& other) noexcept(std::is_nothrow_copy_assignable_v<T>) {
        check_fits();
        static_assert(std::is_copy_assignable_v<T>, "T must be copy assignable");
        if (this != &other) {
            *ptr() = *other.ptr();
        }
        return *this;
    }

    direct& operator=(direct&& other) noexcept(std::is_nothrow_move_assignable_v<T>) {
        check_fits();
        static_assert(std::is_move_assignable_v<T>, "T must be move assignable");
        if (this != &other) {
            *ptr() = std::move(*other.ptr());
        }
        return *this;
    }

    // [direct.dtor]

    ~direct() {
        check_fits();
        ptr()->~T();
    }

    // [direct.modifiers]

    // Destroys the contained T, then constructs a new one from args.
    //
    // Preconditions (violation is undefined behavior):
    // - The selected constructor of T must not throw. The old T is
    //   destroyed before the new one is constructed; if construction
    //   throws, no T lives in the storage and every subsequent operation
    //   on *this -- including its destructor -- is undefined. (direct has
    //   no valueless state to fall back to, by design.)
    // - args must not reference the contained value: d.emplace(*d) is
    //   undefined for the same destroy-first reason.
    template <class... Args>
    T& emplace(Args&&... args) {
        check_fits();
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible from Args...");
        ptr()->~T();
        return *std::construct_at(reinterpret_cast<T*>(storage_), std::forward<Args>(args)...);
    }

    template <class I, class... Args>
    T& emplace(std::initializer_list<I> ilist, Args&&... args) {
        check_fits();
        static_assert(std::is_constructible_v<T, std::initializer_list<I>&, Args...>,
                      "T must be constructible from initializer_list<I>&, Args...");
        ptr()->~T();
        return *std::construct_at(reinterpret_cast<T*>(storage_), ilist, std::forward<Args>(args)...);
    }

    // [direct.obs]
    //
    // Note: unlike unique_ptr's accessors, these DO require T to be
    // complete at their point of instantiation -- ptr() goes through
    // std::launder, which rejects incomplete types. In the pimpl pattern
    // this is harmless: accessor calls happen in TUs that use the result,
    // which need a complete T anyway.

    T&        operator*() & noexcept { return *ptr(); }
    const T&  operator*() const& noexcept { return *ptr(); }
    T&&       operator*() && noexcept { return std::move(*ptr()); }
    const T&& operator*() const&& noexcept { return std::move(*ptr()); }

    T*       operator->() noexcept { return ptr(); }
    const T* operator->() const noexcept { return ptr(); }

    // [direct.relops]
    //
    // These are templates, following indirect's shape, for two reasons.
    //
    // A non-template friend whose return type depends on T has that return
    // type computed when the class is instantiated, which requires T to be
    // complete at the point direct<T, Size, Align> is merely named -- the one
    // property this type exists to provide. Making them templates defers it
    // to the point of use. (A deduced `auto` return type also defers, but
    // leaves the signature unstated; an explicit return type on a template
    // gives both.)
    //
    // The explicit return type additionally makes these SFINAE-friendly, so a
    // T with no comparison operators removes the overload rather than relying
    // on its body never being instantiated.
    //
    // Being templates, they also compare across reservations: two direct
    // objects holding comparable types compare regardless of their Size and
    // Align.

    template <class U, std::size_t S2, std::size_t A2>
    friend auto operator==(const direct& lhs, const direct<U, S2, A2>& rhs) noexcept(noexcept(*lhs == *rhs))
        -> decltype(static_cast<bool>(*lhs == *rhs)) {
        return *lhs == *rhs;
    }

    template <class U, std::size_t S2, std::size_t A2>
    friend auto operator<=>(const direct& lhs, const direct<U, S2, A2>& rhs) -> detail::synth_three_way_result<T, U> {
        return detail::synth_three_way(*lhs, *rhs);
    }

  private:
    static void check_fits() noexcept {
        static_assert(BEMAN_DIRECT_IS_COMPLETE(T),
                      "T must be complete at the point direct<T, Size, Align> is constructed, "
                      "destroyed, assigned, or emplaced");
        static_assert(sizeof(T) <= Size, "T does not fit in direct<T, Size, Align>'s storage; increase Size");
        static_assert(alignof(T) <= Align, "T's alignment exceeds direct<T, Size, Align>'s Align; increase Align");
    }

    // used to create a consistent diagnostic across toolchains; best effort.
    static void check_complete_for_access() noexcept {
        static_assert(BEMAN_DIRECT_IS_COMPLETE(T),
                      "T must be complete to dereference direct<T, Size, Align>; declare the "
                      "accessor in the header and define it where T is complete");
    }

    T* ptr() noexcept {
        check_complete_for_access();
        return std::launder(reinterpret_cast<T*>(storage_));
    }
    const T* ptr() const noexcept {
        check_complete_for_access();
        return std::launder(reinterpret_cast<const T*>(storage_));
    }

    alignas(Align) std::byte storage_[Size];
};

} // namespace beman::direct

#endif // BEMAN_DIRECT_DIRECT_HPP
