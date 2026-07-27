// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_DIRECT_DETAIL_CONFIG_HPP
#define BEMAN_DIRECT_DETAIL_CONFIG_HPP

// ---------------------------------------------------------------------------
// Feature flags
//
// Normally these come from config_generated.hpp, which CMake writes into the
// build tree after probing the compiler once at configure time. Users who
// simply vendor the include/ directory have no generated header, so we fall
// back to the C++20 defaults below -- C++20 is the Beman Standard's
// recommended baseline ([cpp.min_std_version]).
//
// A vendoring user on C++17 can select the fallback paths by defining both
// macros to 0 on the command line before including any beman/direct header.
// ---------------------------------------------------------------------------

// This header is a facade: consumers include it and get the feature macros,
// whichever of the two places they come from. The export pragma says so, so
// that include-cleaner attributes the macros to this header rather than
// asking every user to include the generated one directly.
#if !defined(__has_include) || __has_include(<beman/direct/detail/config_generated.hpp>)
    #include <beman/direct/detail/config_generated.hpp> // IWYU pragma: export
#endif

#if !defined(BEMAN_DIRECT_USE_CONCEPTS)
    #define BEMAN_DIRECT_USE_CONCEPTS 1
#endif

#if !defined(BEMAN_DIRECT_USE_THREE_WAY_COMPARISON)
    #define BEMAN_DIRECT_USE_THREE_WAY_COMPARISON 1
#endif

// ---------------------------------------------------------------------------
// Polyfills and detail utilities
// ---------------------------------------------------------------------------

#include <memory>
#include <new> // IWYU pragma: keep -- used in C++17 build only
#include <type_traits>
#include <utility>

#if BEMAN_DIRECT_USE_THREE_WAY_COMPARISON
    // std::three_way_comparable_with, and the ordering types the deduced
    // return types below resolve to.
    #include <compare>
#endif

namespace beman::direct::detail {

// remove_cvref_t (always our own — trivial, avoids version branching)
template <class T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// is_complete_v: check if T is a complete type at the point of instantiation.
//
// CAUTION: the result is memoized per TU at first instantiation -- once it
// evaluates false for a T, it stays false in that TU even after T becomes
// complete (and differing results across TUs are an ODR hazard). Its ONLY
// sanctioned use is inside direct's check_fits(), where a false result is
// always an immediate hard error; do not reuse it for dispatch.
template <class T, class = void>
inline constexpr bool is_complete_v = false;

template <class T>
inline constexpr bool is_complete_v<T, std::void_t<decltype(sizeof(T))>> = true;

// construct_at polyfill
#if BEMAN_DIRECT_USE_CONCEPTS
template <class T, class... Args>
constexpr T* construct_at_impl(T* p, Args&&... args) {
    return std::construct_at(p, std::forward<Args>(args)...);
}
#else
template <class T, class... Args>
T* construct_at_impl(T* p, Args&&... args) {
    return ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
}
#endif

#if BEMAN_DIRECT_USE_THREE_WAY_COMPARISON

// synth-three-way per [expos.only.entity]
struct synth_three_way_fn {
    template <class T, class U>
    constexpr auto operator()(const T& t, const U& u) const {
        if constexpr (std::three_way_comparable_with<T, U>) {
            return t <=> u;
        } else {
            if (t < u)
                return std::weak_ordering::less;
            if (u < t)
                return std::weak_ordering::greater;
            return std::weak_ordering::equivalent;
        }
    }
};

inline constexpr synth_three_way_fn synth_three_way{};

// synth-three-way-result
template <class T, class U = T>
using synth_three_way_result = decltype(synth_three_way(std::declval<const T&>(), std::declval<const U&>()));

#endif // BEMAN_DIRECT_USE_THREE_WAY_COMPARISON

} // namespace beman::direct::detail

#endif // BEMAN_DIRECT_DETAIL_CONFIG_HPP
