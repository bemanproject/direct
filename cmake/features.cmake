# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(CheckCXXSourceCompiles)

function(beman_direct_check_concepts result_var)
    check_cxx_source_compiles(
        "
#include <concepts>
template <class T>
concept Addable = requires(T a, T b) { a + b; };
static_assert(Addable<int>);
int main() {}
"
        HAVE_CONCEPTS
    )
    set(${result_var} ${HAVE_CONCEPTS} PARENT_SCOPE)
endfunction()

function(beman_direct_check_three_way_comparison result_var)
    check_cxx_source_compiles(
        "
#include <compare>
struct S {
    int x;
    auto operator<=>(const S&) const = default;
};
int main() {
    S a{1}, b{2};
    return (a <=> b) < 0 ? 0 : 1;
}
"
        HAVE_THREE_WAY
    )
    set(${result_var} ${HAVE_THREE_WAY} PARENT_SCOPE)
endfunction()
