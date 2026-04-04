#include "sap_core/stl/allocator.h"
#include "sap_core/stl/vector.h"
#include "sap_core/stl/map.h"
#include <gtest/gtest.h>

#include <new>

// =============================================================================
// linear_allocator
// =============================================================================

class LinearAllocatorTest : public ::testing::Test {
protected:
    static constexpr size_t kSize = 65536;
    alignas(64) uint8_t buffer[kSize];
    stl::linear_arena arena{buffer, kSize};
};

TEST_F(LinearAllocatorTest, AllocateSingle) {
    stl::linear_allocator<int> alloc(&arena);
    int* p = alloc.allocate(1);
    ASSERT_NE(p, nullptr);
    *p = 42;
    EXPECT_EQ(*p, 42);
}

TEST_F(LinearAllocatorTest, AllocateArray) {
    stl::linear_allocator<int> alloc(&arena);
    int* p = alloc.allocate(100);
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 100; ++i)
        p[i] = i;
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(p[i], i);
}

TEST_F(LinearAllocatorTest, DeallocateIsNoop) {
    stl::linear_allocator<int> alloc(&arena);
    int* p = alloc.allocate(10);
    alloc.deallocate(p, 10);
    EXPECT_EQ(arena.used(), 10 * sizeof(int)); // still used
}

TEST_F(LinearAllocatorTest, ThrowsOnOverflow) {
    stl::linear_allocator<int> alloc(&arena);
    alloc.allocate(kSize / sizeof(int)); // fill up
    EXPECT_THROW(alloc.allocate(1), std::bad_alloc);
}

TEST_F(LinearAllocatorTest, Rebind) {
    stl::linear_allocator<int> int_alloc(&arena);
    stl::linear_allocator<double> dbl_alloc(int_alloc);
    double* p = dbl_alloc.allocate(1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(dbl_alloc.arena(), &arena);
}

TEST_F(LinearAllocatorTest, Equality) {
    stl::linear_allocator<int> a1(&arena);
    stl::linear_allocator<int> a2(&arena);
    EXPECT_EQ(a1, a2);

    alignas(64) uint8_t other_buf[1024];
    stl::linear_arena other(other_buf, 1024);
    stl::linear_allocator<int> a3(&other);
    EXPECT_NE(a1, a3);
}

TEST_F(LinearAllocatorTest, UseWithVector) {
    stl::linear_allocator<int> alloc(&arena);
    stl::vector<int, stl::linear_allocator<int>> vec(alloc);
    for (int i = 0; i < 100; ++i)
        vec.push_back(i);
    EXPECT_EQ(vec.size(), 100u);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(vec[i], i);
}

TEST_F(LinearAllocatorTest, NullArenaAccessor) {
    stl::linear_allocator<int> alloc;
    EXPECT_EQ(alloc.arena(), nullptr);
}

// =============================================================================
// pool_allocator
// =============================================================================

class PoolAllocatorTest : public ::testing::Test {
protected:
    static constexpr size_t kSize = 65536;
    alignas(64) uint8_t buffer[kSize];
    stl::linear_arena arena{buffer, kSize};
};

TEST_F(PoolAllocatorTest, SingleElementUsesPool) {
    stl::pool_allocator<int> alloc(&arena);
    int* p = alloc.allocate(1);
    ASSERT_NE(p, nullptr);
    EXPECT_NE(alloc.pool(), nullptr); // pool was lazily created
}

TEST_F(PoolAllocatorTest, MultiElementUsesArena) {
    stl::pool_allocator<int> alloc(&arena);
    int* p = alloc.allocate(10);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(alloc.pool(), nullptr); // pool not created for multi-element
}

TEST_F(PoolAllocatorTest, DeallocateSingleReturnsToPool) {
    stl::pool_allocator<int> alloc(&arena);
    int* p1 = alloc.allocate(1);
    alloc.deallocate(p1, 1);
    int* p2 = alloc.allocate(1);
    EXPECT_EQ(p1, p2); // reused from pool
}

TEST_F(PoolAllocatorTest, DeallocateNull) {
    stl::pool_allocator<int> alloc(&arena);
    alloc.deallocate(nullptr, 0); // should not crash
    alloc.deallocate(nullptr, 1); // should not crash
}

TEST_F(PoolAllocatorTest, Rebind) {
    stl::pool_allocator<int> int_alloc(&arena);
    stl::pool_allocator<double> dbl_alloc(int_alloc);
    EXPECT_EQ(dbl_alloc.arena(), &arena);
    // Rebound allocator should have its own pool (nullptr initially)
    EXPECT_EQ(dbl_alloc.pool(), nullptr);
}

TEST_F(PoolAllocatorTest, Equality) {
    stl::pool_allocator<int> a1(&arena);
    stl::pool_allocator<int> a2(&arena);
    // Both null pool
    EXPECT_EQ(a1, a2);

    a1.allocate(1); // creates pool for a1
    EXPECT_NE(a1, a2); // different pools now
}

TEST_F(PoolAllocatorTest, UseWithMap) {
    using pair_type = std::pair<const int, int>;
    stl::pool_allocator<pair_type> alloc(&arena);
    stl::map<int, int, std::less<int>, stl::pool_allocator<pair_type>> m(alloc);
    for (int i = 0; i < 50; ++i)
        m[i] = i * i;
    EXPECT_EQ(m.size(), 50u);
    EXPECT_EQ(m[7], 49);
}

TEST_F(PoolAllocatorTest, ManyAllocDealloc) {
    stl::pool_allocator<int> alloc(&arena);
    std::vector<int*> ptrs;
    for (int i = 0; i < 100; ++i)
        ptrs.push_back(alloc.allocate(1));
    for (auto* p : ptrs)
        alloc.deallocate(p, 1);
    // All should be reusable now
    for (int i = 0; i < 100; ++i) {
        int* p = alloc.allocate(1);
        EXPECT_NE(p, nullptr);
    }
}
