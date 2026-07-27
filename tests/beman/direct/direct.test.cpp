// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/direct/direct.hpp>
// For BEMAN_DIRECT_USE_THREE_WAY_COMPARISON, used to guard a test below.
#include <beman/direct/detail/config.hpp>

#include "test_helpers.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <cstddef>

using beman::direct::direct;

TEST(Direct, SizeAndAlignConstants) {
    EXPECT_EQ((direct<int, 16, 8>::size), 16u);
    EXPECT_EQ((direct<int, 16, 8>::align), 8u);
    EXPECT_EQ(sizeof(direct<int, 16, 8>), 16u);
    EXPECT_EQ(alignof(direct<int, 16, 8>), 8u);
}

TEST(Direct, DefaultAlign) { EXPECT_EQ((direct<int, 16>::align), alignof(std::max_align_t)); }

using beman::direct::tests::Tracked;

TEST(Direct, InPlaceConstructAndDestruct) {
    Tracked::reset();
    {
        direct<Tracked, sizeof(Tracked), alignof(Tracked)> d(std::in_place, 7);
        EXPECT_EQ(d->value, 7);
        EXPECT_EQ(Tracked::constructs, 1);
    }
    EXPECT_EQ(Tracked::destructs, 1);
}

TEST(Direct, InPlaceConstructWithNoArgs) {
    Tracked::reset();
    direct<Tracked, sizeof(Tracked), alignof(Tracked)> d(std::in_place);
    EXPECT_EQ(d->value, 0);
    EXPECT_EQ(Tracked::constructs, 1);
}

TEST(Direct, InPlaceConstructWithInitializerList) {
    direct<std::vector<int>, sizeof(std::vector<int>), alignof(std::vector<int>)> d(std::in_place, {1, 2, 3});
    EXPECT_EQ(d->size(), 3u);
    EXPECT_EQ((*d)[0], 1);
}

TEST(Direct, ConvertingConstructFromValue) {
    direct<int, sizeof(int), alignof(int)> d(42);
    EXPECT_EQ(*d, 42);
}

TEST(Direct, ConvertingConstructFromMovedValue) {
    direct<Tracked, sizeof(Tracked), alignof(Tracked)> d(Tracked{5});
    EXPECT_EQ(d->value, 5);
}

TEST(Direct, DereferenceRefQualifiedOverloads) {
    direct<Tracked, sizeof(Tracked), alignof(Tracked)> d(std::in_place, 3);
    const auto&                                        cd = d;

    static_assert(std::is_same_v<decltype(*d), Tracked&>);
    static_assert(std::is_same_v<decltype(*cd), const Tracked&>);
    static_assert(std::is_same_v<decltype(*std::move(d)), Tracked&&>);
    static_assert(std::is_same_v<decltype(*std::move(cd)), const Tracked&&>);

    EXPECT_EQ((*d).value, 3);
    EXPECT_EQ((*cd).value, 3);
}

TEST(Direct, Emplace) {
    direct<Tracked, sizeof(Tracked), alignof(Tracked)> d(std::in_place, 1);
    Tracked::reset();
    Tracked& ref = d.emplace(99);
    EXPECT_EQ(d->value, 99);
    EXPECT_EQ(&ref, &*d);
    EXPECT_EQ(Tracked::destructs, 1);  // old value destroyed
    EXPECT_EQ(Tracked::constructs, 1); // new value constructed
}

TEST(Direct, EqualityCompares) {
    direct<int, sizeof(int), alignof(int)> a(1), b(2), c(1);
    EXPECT_TRUE(a == c);
    EXPECT_FALSE(a == b);
}

#if BEMAN_DIRECT_USE_THREE_WAY_COMPARISON
TEST(Direct, ThreeWayCompares) {
    direct<int, sizeof(int), alignof(int)> a(1), b(2);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE((a <=> b) < 0);
}
#else
TEST(Direct, RelationalFallbackCompares) {
    direct<int, sizeof(int), alignof(int)> a(1), b(2);
    EXPECT_TRUE(a < b);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(b > a);
    EXPECT_TRUE(b >= a);
    EXPECT_TRUE(a != b);
}
#endif

TEST(Direct, ComparisonsDoNotRequireTToBeComparable) {
    struct NoCompare {
        int value;
        explicit NoCompare(int v) : value(v) {}
    };
    direct<NoCompare, sizeof(NoCompare), alignof(NoCompare)> d(std::in_place, 1);
    EXPECT_EQ(d->value, 1);
}

using beman::direct::tests::ThrowsOnCopyAssign;
using beman::direct::tests::ThrowsOnCopyCtor;

TEST(Direct, CopyCtorThrowLeavesSourceUntouchedAndNoLeak) {
    using D = direct<ThrowsOnCopyCtor, sizeof(ThrowsOnCopyCtor), alignof(ThrowsOnCopyCtor)>;
    D d1(std::in_place, 1);
    ThrowsOnCopyCtor::armed = true;
    bool threw              = false;
    try {
        D d2(d1);
        (void)d2;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ThrowsOnCopyCtor::armed = false;
    EXPECT_TRUE(threw);
    EXPECT_EQ(d1->value, 1); // source untouched by the failed copy
    // d2 was never fully constructed, so its destructor never runs --
    // there is nothing to leak or double-destroy here, unlike a
    // heap-backed pointee, which would need an explicit catch+deallocate.
}

TEST(Direct, CopyAssignThrowPropagatesAndDelegatesToT) {
    using D = direct<ThrowsOnCopyAssign, sizeof(ThrowsOnCopyAssign), alignof(ThrowsOnCopyAssign)>;
    D d1(std::in_place, 1);
    D d2(std::in_place, 2);
    ThrowsOnCopyAssign::armed = true;
    bool threw                = false;
    try {
        d1 = d2;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ThrowsOnCopyAssign::armed = false;
    EXPECT_TRUE(threw);
    // T::operator= mutated value before throwing; direct<T> adds no
    // extra guarantee beyond what T itself provides
    EXPECT_EQ(d1->value, 2);
}

// -----------------------------------------------------------------------
// Compile-time SMF contract (static_asserts; no runtime component)
// -----------------------------------------------------------------------

namespace {

struct ThrowingMove {
    ThrowingMove() = default;
    ThrowingMove(ThrowingMove&&) noexcept(false) {}
    ThrowingMove& operator=(ThrowingMove&&) noexcept(false) { return *this; }
};

struct NoDefaultCtor {
    explicit NoDefaultCtor(int) {}
};

using DTracked      = direct<Tracked, sizeof(Tracked), alignof(Tracked)>;
using DThrowingMove = direct<ThrowingMove, sizeof(ThrowingMove), alignof(ThrowingMove)>;

// noexcept of every constructor/assignment forwards T's own noexcept.
static_assert(std::is_nothrow_move_constructible_v<DTracked>);
static_assert(std::is_nothrow_move_assignable_v<DTracked>);
static_assert(!std::is_nothrow_move_constructible_v<DThrowingMove>);
static_assert(!std::is_nothrow_move_assignable_v<DThrowingMove>);

using DInt = direct<int, sizeof(int), alignof(int)>;
static_assert(std::is_nothrow_default_constructible_v<DInt>);
static_assert(std::is_nothrow_copy_constructible_v<DInt>);
static_assert(std::is_nothrow_copy_assignable_v<DInt>);
// Tracked's members are not marked noexcept, so the negatives hold too:
static_assert(!std::is_nothrow_default_constructible_v<DTracked>);
static_assert(!std::is_nothrow_copy_constructible_v<DTracked>);
static_assert(!std::is_nothrow_copy_assignable_v<DTracked>);

// --- Irreducible losses vs a plain T member ---
// See papers/PxxxxR0-beman-direct.md, "Properties of T that are not retained".

// Aggregate status is never propagated: direct has user-provided ctors.
struct Agg {
    int a;
    int b;
};
static_assert(!std::is_aggregate_v<direct<Agg, sizeof(Agg), alignof(Agg)>>);

// An empty T still occupies the full reserved Size -- no empty-base /
// [[no_unique_address]]-style optimization is possible through direct.
struct Empty {};
static_assert(sizeof(direct<Empty, 16, 8>) == 16);

// operator-> return types (const and non-const overloads).
static_assert(std::is_same_v<decltype(std::declval<DTracked&>().operator->()), Tracked*>);
static_assert(std::is_same_v<decltype(std::declval<const DTracked&>().operator->()), const Tracked*>);

// Documented trait pessimism: never trivially copyable/destructible,
// even for trivial T (each special member is user-provided).
static_assert(!std::is_trivially_copyable_v<direct<int, sizeof(int), alignof(int)>>);
static_assert(!std::is_trivially_destructible_v<direct<int, sizeof(int), alignof(int)>>);

// Documented trait over-reporting: the default ctor is constrained by an
// in-body static_assert (to stay incomplete-T-friendly), not SFINAE, so
// the trait reports true even though instantiation would fail.
static_assert(std::is_default_constructible_v<direct<NoDefaultCtor, sizeof(NoDefaultCtor), alignof(NoDefaultCtor)>>);

} // namespace

TEST(Direct, EmplaceInitializerList) {
    direct<std::vector<int>, sizeof(std::vector<int>), alignof(std::vector<int>)> d(std::in_place, {9});
    d.emplace({1, 2, 3});
    EXPECT_EQ(d->size(), 3u);
    EXPECT_EQ((*d)[2], 3);
}

// -----------------------------------------------------------------------
// Over-allocation
//
// Size may exceed sizeof(T) and Align may be stricter than alignof(T).
// This is intended, not merely tolerated: reserving headroom is how an
// author leaves room for T to grow without changing the layout of types
// that embed it. The Mandates are `sizeof(T) <= Size` and
// `alignof(T) <= Align`, never equality.
// -----------------------------------------------------------------------

namespace {
struct Small {
    char c;
};
} // namespace

// Deliberately reserving far more storage, and far stricter alignment,
// than Small requires.
using OverAllocated = direct<Small, 128, 64>;

static_assert(sizeof(Small) < 128);
static_assert(alignof(Small) < 64);
static_assert(sizeof(OverAllocated) == 128);
static_assert(alignof(OverAllocated) == 64);

TEST(Direct, OverAllocatedStorageIsUsable) {
    OverAllocated d(std::in_place, Small{'x'});
    EXPECT_EQ(d->c, 'x');

    // The contained object honors the requested (stricter) alignment.
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&*d) % 64u, 0u);

    d.emplace(Small{'y'});
    EXPECT_EQ(d->c, 'y');

    OverAllocated copy = d;
    EXPECT_EQ(copy->c, 'y');
    copy->c = 'z';
    EXPECT_EQ(d->c, 'y');
}

TEST(Direct, GrowingTWithinTheReservationKeepsTheLayout) {
    // Two payloads of different sizes, both within the same reservation,
    // produce direct types of identical layout -- the property that lets an
    // implementation type change shape without disturbing its users.
    struct V1 {
        int a;
    };
    struct V2 {
        int    a;
        double b;
        char   pad[16];
    };
    static_assert(sizeof(V1) < sizeof(V2));
    static_assert(sizeof(direct<V1, 64, 8>) == sizeof(direct<V2, 64, 8>));
    static_assert(alignof(direct<V1, 64, 8>) == alignof(direct<V2, 64, 8>));

    direct<V2, 64, 8> d(std::in_place, V2{1, 2.0, {}});
    EXPECT_EQ(d->a, 1);
}
