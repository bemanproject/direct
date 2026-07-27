// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This translation unit provides the test program (main comes from
// gtest_main) and links against the static library
// beman.direct.tests.widget. It includes widget.hpp, in which widget::Impl
// is only forward-declared, so Impl is incomplete throughout this TU.
// Every observation below therefore crosses a real compile-and-link
// boundary: the behavior it checks was produced by code compiled in
// widget_impl.test.cpp, where Impl is complete.

#include "widget.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

using beman::direct::tests::widget;

namespace {

// Resets the cross-TU lifetime counters before each test that reads them.
struct CountedWidget : ::testing::Test {
    void SetUp() override { widget::reset_counts(); }
};

} // namespace

TEST(Incomplete, ConstructAndReadLabel) {
    widget w("hello");
    EXPECT_EQ(w.label(), "hello");
}

TEST(Incomplete, DefaultConstruct) {
    widget w;
    EXPECT_EQ(w.label(), "default");
}

TEST(Incomplete, CopyIsDeepAndIndependent) {
    widget w1("hello");
    widget w2 = w1;
    w2.set_label("world");
    EXPECT_EQ(w1.label(), "hello");
    EXPECT_EQ(w2.label(), "world");
}

TEST(Incomplete, MoveTransfersValue) {
    widget w1("hello");
    widget w2 = std::move(w1);
    EXPECT_EQ(w2.label(), "hello");
}

TEST(Incomplete, CopyAssign) {
    widget w1("hello");
    widget w2("other");
    w1 = w2;
    EXPECT_EQ(w1.label(), "other");
}

TEST(Incomplete, SizeofWidgetIsFixedByReservedStorage) {
    // sizeof(widget) must not depend on the definition of Impl -- this TU
    // never sees it.
    EXPECT_EQ(sizeof(widget), widget::storage_size);
    EXPECT_EQ(alignof(widget), widget::storage_align);
}

// --- Observations that cross the library boundary ------------------------
//
// Everything below reads state that lives inside Impl, which this TU cannot
// see. Passing requires that the definitions linked in from the widget
// library actually ran.

TEST_F(CountedWidget, ImplIsConstructedAndDestroyedAcrossTheBoundary) {
    EXPECT_EQ(widget::live_count(), 0);
    {
        widget w("hello");
        EXPECT_EQ(widget::live_count(), 1);
        EXPECT_EQ(widget::construct_count(), 1);
    }
    EXPECT_EQ(widget::live_count(), 0);
}

TEST_F(CountedWidget, CopyConstructionRunsImplsCopyConstructor) {
    widget w1("hello");
    EXPECT_EQ(widget::live_count(), 1);
    {
        widget w2 = w1;
        EXPECT_EQ(widget::copy_construct_count(), 1);
        EXPECT_EQ(widget::move_construct_count(), 0);
        EXPECT_EQ(widget::live_count(), 2);
        EXPECT_EQ(w2.label(), "hello");
    }
    EXPECT_EQ(widget::live_count(), 1);
}

TEST_F(CountedWidget, MoveConstructionRunsImplsMoveConstructor) {
    widget w1("hello");
    widget w2 = std::move(w1);
    EXPECT_EQ(widget::move_construct_count(), 1);
    EXPECT_EQ(widget::copy_construct_count(), 0);
    EXPECT_EQ(w2.label(), "hello");
    // Both widgets still hold a live Impl: direct has no valueless state,
    // so a moved-from widget owns a moved-from-but-valid Impl.
    EXPECT_EQ(widget::live_count(), 2);
}

TEST_F(CountedWidget, CopyAssignmentRunsImplsCopyAssignment) {
    widget w1("hello");
    widget w2("other");
    ASSERT_EQ(widget::construct_count(), 2);
    w1 = w2;
    // Assignment runs Impl's assignment operator, not a constructor.
    EXPECT_EQ(widget::copy_assign_count(), 1);
    EXPECT_EQ(widget::construct_count(), 2);
    EXPECT_EQ(widget::live_count(), 2);
    EXPECT_EQ(w1.label(), "other");
}

TEST_F(CountedWidget, MoveAssignmentRunsImplsMoveAssignment) {
    widget w1("hello");
    widget w2("other");
    ASSERT_EQ(widget::construct_count(), 2);
    w1 = std::move(w2);
    EXPECT_EQ(widget::move_assign_count(), 1);
    EXPECT_EQ(widget::copy_assign_count(), 0);
    EXPECT_EQ(widget::construct_count(), 2);
    EXPECT_EQ(w1.label(), "other");
}

TEST_F(CountedWidget, SelfAssignmentIsANoOpAcrossTheBoundary) {
    widget  w("hello");
    widget& alias = w;

    w = alias; // self copy-assign, via an alias to defeat -Wself-assign
    EXPECT_EQ(widget::copy_assign_count(), 0);
    EXPECT_EQ(w.label(), "hello");

    w = std::move(alias); // self move-assign
    EXPECT_EQ(widget::move_assign_count(), 0);
    EXPECT_EQ(w.label(), "hello");
    EXPECT_EQ(widget::live_count(), 1);
}

TEST_F(CountedWidget, AllImplsAreDestroyedWhenAContainerOfWidgetsDies) {
    {
        std::vector<widget> ws;
        ws.reserve(4);
        for (int i = 0; i < 4; ++i) {
            ws.emplace_back("w" + std::to_string(i));
        }
        EXPECT_EQ(widget::live_count(), 4);
        EXPECT_EQ(ws[2].label(), "w2");
    }
    EXPECT_EQ(widget::live_count(), 0);
}

TEST(Incomplete, MutationInsideImplIsObservedAcrossTheBoundary) {
    widget w("hello");
    EXPECT_EQ(w.revision(), 0);

    w.set_label("world");
    EXPECT_EQ(w.label(), "world");
    EXPECT_EQ(w.revision(), 1);

    w.set_label("again");
    EXPECT_EQ(w.label(), "again");
    EXPECT_EQ(w.revision(), 2);
}

TEST(Incomplete, RevisionIsCopiedWithTheWidget) {
    widget w1("hello");
    w1.set_label("world");
    ASSERT_EQ(w1.revision(), 1);

    widget w2 = w1;
    EXPECT_EQ(w2.revision(), 1);

    // The copies are independent: mutating one does not advance the other.
    w2.set_label("third");
    EXPECT_EQ(w2.revision(), 2);
    EXPECT_EQ(w1.revision(), 1);
    EXPECT_EQ(w1.label(), "world");
}

TEST(Incomplete, ImplFitsTheReservation) {
    // sizeof(Impl)/alignof(Impl) are reported from the TU where Impl is
    // complete; this TU knows only the reservation. The library would not
    // have compiled had these been violated, so this records the margin.
    EXPECT_LE(widget::impl_size(), widget::storage_size);
    EXPECT_LE(widget::impl_align(), widget::storage_align);
    EXPECT_GT(widget::impl_size(), 0u);
}
