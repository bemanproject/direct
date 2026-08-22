# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

include(CheckCXXSourceCompiles)

function(beman_direct_check_cxx20)
    # The selected standard and other user flags can change when an existing
    # build tree is reconfigured, so do not reuse a result from older flags.
    unset(BEMAN_DIRECT_HAS_REQUIRED_CXX20 CACHE)
    check_cxx_source_compiles(
        [[
#include <compare>
#include <concepts>
#include <memory>

template <class T>
concept Integer = std::same_as<T, int>;

struct Value {
    int value;
    auto operator<=>(const Value&) const = default;
};

int main() {
    static_assert(Integer<int>);
    alignas(Value) unsigned char storage[sizeof(Value)];
    auto* value = std::construct_at(reinterpret_cast<Value*>(storage), Value{1});
    const bool ordered = (Value{0} <=> *value) < 0;
    std::destroy_at(value);
    return ordered ? 0 : 1;
}
]]
        BEMAN_DIRECT_HAS_REQUIRED_CXX20
    )

    if(NOT BEMAN_DIRECT_HAS_REQUIRED_CXX20)
        message(
            FATAL_ERROR
            "beman.direct requires C++20 language and library support. "
            "Select C++20 or later in your compiler settings (for example, "
            "configure with -DCMAKE_CXX_STANDARD=20)."
        )
    endif()
endfunction()
