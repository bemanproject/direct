// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_DIRECT_DETAIL_CONFIG_HPP
#define BEMAN_DIRECT_DETAIL_CONFIG_HPP

#include <compare>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace beman::direct::detail {

// Completeness probe, reached through BEMAN_DIRECT_IS_COMPLETE below.
//
// This exists only to improve a diagnostic. Using direct where T must be
// complete is ill-formed either way -- sizeof and std::launder see to that --
// but the diagnostics they produce name neither direct nor the member at
// fault. The macro lets the class say so itself. It is quality of
// implementation, not a guarantee: a program in which two translation units
// disagree about T's completeness is ill-formed with no diagnostic required,
// and nothing here can detect that.
//
// The check is written as overloads rather than as a trait variable because
// the obvious trait is worse in two ways. `template <class T, class = void>
// constexpr bool v = false;` plus a partial specialization has one entity,
// v<T, void>, whose meaning depends on where it is instantiated; it is frozen
// at the first instantiation, so it still reads false after T is defined, and
// two translation units reaching different answers for it is itself ill-formed
// with no diagnostic required. The overloads below share no such entity: a
// true answer comes from is_complete_probe<T, sizeof(T)>, which cannot exist
// unless T is complete, and a false answer from the separate varargs overload.
// libstdc++ implements its own completeness check this way for the same
// reason, and the trait was rejected for standardization on these grounds:
// https://lists.isocpp.org/std-proposals/2021/11/3310.php
//
// Use only where a false answer is an immediate hard error, as both call sites
// in direct.hpp do. Branching a definition on the answer would move the ODR
// problem to whatever was branched.

template <class T>
struct type_identity_ {
    using type = T;
};

template <class T, std::size_t = sizeof(T)>
constexpr std::true_type is_complete_probe(type_identity_<T>) {
    return {};
}

constexpr std::false_type is_complete_probe(...) { return {}; }

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

} // namespace beman::direct::detail

// True if T is complete at the point of expansion.
//
// This is a macro rather than a template on purpose. Because it expands
// textually, the overload resolution above is performed afresh wherever it
// is written, which is what makes the answer correct at each point. Wrapping
// the same expression in a variable template or a function template would
// read better and be wrong: the wrapper is instantiated once per translation
// unit and would freeze the first answer, reintroducing exactly the staleness
// this form avoids.
#define BEMAN_DIRECT_IS_COMPLETE(...)                     \
    (decltype(::beman::direct::detail::is_complete_probe( \
        ::beman::direct::detail::type_identity_<__VA_ARGS__>{}))::value)

#endif // BEMAN_DIRECT_DETAIL_CONFIG_HPP
