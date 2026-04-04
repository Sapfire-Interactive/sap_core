#include "sap_core/stl/stack_allocator.h"
#include "sap_core/stl/vector.h"
#include <gtest/gtest.h>

#include <new>

// =============================================================================
// stack_arena
// =============================================================================

TEST(StackArena, InitialState) {
    stl::stack_arena<1024> arena;
    EXPECT_EQ(arena.capacity(), 1024u);
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.remaining(), 1024u);
}

TEST(StackArena, SingleAllocation) {
    stl::stack_arena<1024> arena;
    void* p = arena.allocate(64, 1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(arena.used(), 64u);
    EXPECT_EQ(arena.remaining(), 960u);
}

TEST(StackArena, AlignedAllocation) {
    stl::stack_arena<1024> arena;
    arena.allocate(1, 1);
    void* p = arena.allocate(8, 16);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0u);
}

TEST(StackArena, ExhaustCapacity) {
    stl::stack_arena<64> arena;
    void* p = arena.allocate(64, 1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(arena.remaining(), 0u);
    EXPECT_EQ(arena.allocate(1, 1), nullptr);
}

TEST(StackArena, OverflowReturnsNull) {
    stl::stack_arena<64> arena;
    EXPECT_EQ(arena.allocate(65, 1), nullptr);
}

TEST(StackArena, Reset) {
    stl::stack_arena<256> arena;
    arena.allocate(200, 1);
    arena.reset();
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.remaining(), 256u);
    void* p = arena.allocate(256, 1);
    ASSERT_NE(p, nullptr);
}

TEST(StackArena, DeallocateIsNoop) {
    stl::stack_arena<256> arena;
    void* p = arena.allocate(100, 1);
    arena.deallocate(p, 100, 1);
    EXPECT_EQ(arena.used(), 100u);
}

TEST(StackArena, ManySmallAllocations) {
    stl::stack_arena<256> arena;
    for (int i = 0; i < 256; ++i) {
        EXPECT_NE(arena.allocate(1, 1), nullptr);
    }
    EXPECT_EQ(arena.allocate(1, 1), nullptr);
}

// =============================================================================
// stack_allocator
// =============================================================================

TEST(StackAllocator, AllocateSingle) {
    stl::stack_arena<1024> arena;
    stl::stack_allocator<int> alloc(arena);
    int* p = alloc.allocate(1);
    ASSERT_NE(p, nullptr);
    *p = 42;
    EXPECT_EQ(*p, 42);
}

TEST(StackAllocator, ThrowsOnOverflow) {
    stl::stack_arena<16> arena;
    stl::stack_allocator<int> alloc(arena);
    alloc.allocate(4); // 16 bytes
    EXPECT_THROW(alloc.allocate(1), std::bad_alloc);
}

TEST(StackAllocator, Rebind) {
    stl::stack_arena<1024> arena;
    stl::stack_allocator<int> int_alloc(arena);
    stl::stack_allocator<double> dbl_alloc(int_alloc);
    double* p = dbl_alloc.allocate(1);
    ASSERT_NE(p, nullptr);
}

TEST(StackAllocator, Equality) {
    stl::stack_arena<1024> a1;
    stl::stack_arena<1024> a2;
    stl::stack_allocator<int> alloc1(a1);
    stl::stack_allocator<int> alloc2(a1);
    stl::stack_allocator<int> alloc3(a2);
    EXPECT_EQ(alloc1, alloc2);
    EXPECT_NE(alloc1, alloc3);
}

TEST(StackAllocator, CapacityAccessor) {
    stl::stack_arena<2048> arena;
    stl::stack_allocator<int> alloc(arena);
    EXPECT_EQ(alloc.capacity(), 2048u);
}

TEST(StackAllocator, UseWithVector) {
    stl::stack_arena<4096> arena;
    stl::stack_allocator<int> alloc(arena);
    stl::vector<int, stl::stack_allocator<int>> vec(alloc);
    for (int i = 0; i < 50; ++i)
        vec.push_back(i);
    EXPECT_EQ(vec.size(), 50u);
    for (int i = 0; i < 50; ++i)
        EXPECT_EQ(vec[i], i);
}

TEST(StackAllocator, NullArena) {
    stl::stack_allocator<int> alloc;
    EXPECT_EQ(alloc.capacity(), 0u);
}
