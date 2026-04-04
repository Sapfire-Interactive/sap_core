#include "sap_core/stl/shared_ptr.h"
#include "sap_core/stl/unique_ptr.h"
#include "sap_core/stl/allocator.h"
#include <gtest/gtest.h>

#include <string>

// =============================================================================
// stl::unique_ptr
// =============================================================================

TEST(UniquePtr, MakeUnique) {
    auto p = stl::make_unique<int>(42);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(UniquePtr, MakeUniqueString) {
    auto p = stl::make_unique<std::string>("hello");
    EXPECT_EQ(*p, "hello");
}

TEST(UniquePtr, MoveSemantics) {
    auto p1 = stl::make_unique<int>(42);
    auto p2 = std::move(p1);
    EXPECT_EQ(p1, nullptr);
    EXPECT_EQ(*p2, 42);
}

TEST(UniquePtr, Reset) {
    auto p = stl::make_unique<int>(42);
    p.reset();
    EXPECT_EQ(p, nullptr);
}

TEST(UniquePtr, Release) {
    auto p = stl::make_unique<int>(42);
    int* raw = p.release();
    EXPECT_EQ(p, nullptr);
    EXPECT_EQ(*raw, 42);
    delete raw;
}

// =============================================================================
// stl::arena_unique_ptr
// =============================================================================

struct TrackDestruction {
    bool* destroyed;
    TrackDestruction(bool* d) : destroyed(d) {}
    ~TrackDestruction() { *destroyed = true; }
};

TEST(ArenaUniquePtr, MakeUniqueWithArena) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));

    bool destroyed = false;
    {
        auto p = stl::make_unique<TrackDestruction>(arena, &destroyed);
        ASSERT_NE(p.get(), nullptr);
        EXPECT_FALSE(destroyed);
    }
    EXPECT_TRUE(destroyed); // destructor called
}

TEST(ArenaUniquePtr, DestructorOnlyNoFree) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));

    auto p = stl::make_unique<int>(arena, 42);
    EXPECT_EQ(*p, 42);
    size_t used_before = arena.used();
    p.reset(); // destructor only, no dealloc
    EXPECT_EQ(arena.used(), used_before); // memory not freed
}

TEST(ArenaUniquePtr, MultipleObjects) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));

    auto p1 = stl::make_unique<int>(arena, 1);
    auto p2 = stl::make_unique<int>(arena, 2);
    auto p3 = stl::make_unique<int>(arena, 3);
    EXPECT_EQ(*p1, 1);
    EXPECT_EQ(*p2, 2);
    EXPECT_EQ(*p3, 3);
}

TEST(ArenaUniquePtr, MoveSemantics) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));

    auto p1 = stl::make_unique<int>(arena, 42);
    auto p2 = std::move(p1);
    EXPECT_EQ(p1, nullptr);
    EXPECT_EQ(*p2, 42);
}

// =============================================================================
// stl::shared_ptr
// =============================================================================

TEST(SharedPtr, MakeShared) {
    auto p = stl::make_shared<int>(42);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(SharedPtr, RefCounting) {
    auto p1 = stl::make_shared<int>(42);
    EXPECT_EQ(p1.use_count(), 1);
    {
        auto p2 = p1;
        EXPECT_EQ(p1.use_count(), 2);
        EXPECT_EQ(*p2, 42);
    }
    EXPECT_EQ(p1.use_count(), 1);
}

TEST(SharedPtr, WithCustomAllocator) {
    alignas(64) uint8_t buf[65536];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<int> alloc(&arena);

    auto p = stl::make_shared<int, stl::linear_allocator<int>>(alloc, 42);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(*p, 42);
}

TEST(SharedPtr, MoveSemantics) {
    auto p1 = stl::make_shared<int>(42);
    auto p2 = std::move(p1);
    EXPECT_EQ(p1, nullptr);
    EXPECT_EQ(*p2, 42);
}

TEST(SharedPtr, NullCheck) {
    stl::shared_ptr<int> p;
    EXPECT_EQ(p, nullptr);
    EXPECT_FALSE(p);
}

TEST(SharedPtr, ComplexType) {
    auto p = stl::make_shared<std::string>(std::string("hello world"));
    EXPECT_EQ(*p, "hello world");
    EXPECT_EQ(p->size(), 11u);
}
