#include "sap_core/stl/arena.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

// =============================================================================
// align_up
// =============================================================================

TEST(AlignUp, PowersOfTwo) {
    EXPECT_EQ(stl::align_up(0, 4), 0u);
    EXPECT_EQ(stl::align_up(1, 4), 4u);
    EXPECT_EQ(stl::align_up(3, 4), 4u);
    EXPECT_EQ(stl::align_up(4, 4), 4u);
    EXPECT_EQ(stl::align_up(5, 4), 8u);
}

TEST(AlignUp, Alignment1) {
    EXPECT_EQ(stl::align_up(0, 1), 0u);
    EXPECT_EQ(stl::align_up(7, 1), 7u);
    EXPECT_EQ(stl::align_up(1024, 1), 1024u);
}

TEST(AlignUp, LargeAlignments) {
    EXPECT_EQ(stl::align_up(1, 64), 64u);
    EXPECT_EQ(stl::align_up(63, 64), 64u);
    EXPECT_EQ(stl::align_up(64, 64), 64u);
    EXPECT_EQ(stl::align_up(65, 64), 128u);
}

TEST(AlignUp, AlreadyAligned) {
    EXPECT_EQ(stl::align_up(16, 16), 16u);
    EXPECT_EQ(stl::align_up(256, 256), 256u);
}

// =============================================================================
// linear_arena
// =============================================================================

class LinearArenaTest : public ::testing::Test {
protected:
    static constexpr size_t kSize = 4096;
    alignas(64) uint8_t buffer[kSize];
    stl::linear_arena arena{buffer, kSize};
};

TEST_F(LinearArenaTest, InitialState) {
    EXPECT_EQ(arena.capacity(), kSize);
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.peak(), 0u);
    EXPECT_EQ(arena.alloc_calls(), 0u);
}

TEST_F(LinearArenaTest, SingleAllocation) {
    void* p = arena.allocate(64, 1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(arena.used(), 64u);
    EXPECT_EQ(arena.alloc_calls(), 1u);
}

TEST_F(LinearArenaTest, MultipleAllocations) {
    void* p1 = arena.allocate(100, 1);
    void* p2 = arena.allocate(200, 1);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);
    EXPECT_EQ(arena.used(), 300u);
    EXPECT_EQ(arena.alloc_calls(), 2u);
}

TEST_F(LinearArenaTest, AllocationAlignment) {
    arena.allocate(1, 1); // offset = 1
    void* p = arena.allocate(16, 16);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16, 0u);
}

TEST_F(LinearArenaTest, AlignmentWastesSpace) {
    arena.allocate(1, 1);
    arena.allocate(8, 8); // should skip bytes for alignment
    EXPECT_GT(arena.used(), 9u);
}

TEST_F(LinearArenaTest, ExhaustCapacity) {
    void* p = arena.allocate(kSize, 1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(arena.used(), kSize);
}

TEST_F(LinearArenaTest, OverflowReturnsNull) {
    void* p = arena.allocate(kSize + 1, 1);
    EXPECT_EQ(p, nullptr);
}

TEST_F(LinearArenaTest, GradualOverflow) {
    arena.allocate(kSize - 1, 1);
    void* p = arena.allocate(2, 1);
    EXPECT_EQ(p, nullptr);
}

TEST_F(LinearArenaTest, AlignmentCausedOverflow) {
    // Fill to near end, then request aligned allocation that won't fit
    arena.allocate(kSize - 8, 1);
    void* p = arena.allocate(1, 16); // align to 16 would push past end
    EXPECT_EQ(p, nullptr);
}

TEST_F(LinearArenaTest, Reset) {
    arena.allocate(100, 1);
    arena.allocate(200, 1);
    arena.reset();
    EXPECT_EQ(arena.used(), 0u);
    // Peak should remain
    EXPECT_EQ(arena.peak(), 300u);
    // Alloc calls should remain
    EXPECT_EQ(arena.alloc_calls(), 2u);
}

TEST_F(LinearArenaTest, AllocateAfterReset) {
    arena.allocate(kSize, 1);
    arena.reset();
    void* p = arena.allocate(kSize, 1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(arena.used(), kSize);
}

TEST_F(LinearArenaTest, PeakTracking) {
    arena.allocate(1000, 1);
    EXPECT_EQ(arena.peak(), 1000u);
    arena.reset();
    arena.allocate(500, 1);
    EXPECT_EQ(arena.peak(), 1000u); // peak stays at 1000
    arena.allocate(600, 1);
    EXPECT_EQ(arena.peak(), 1100u); // new peak
}

TEST_F(LinearArenaTest, DeallocateIsNoop) {
    void* p = arena.allocate(100, 1);
    arena.deallocate(p, 100, 1);
    EXPECT_EQ(arena.used(), 100u); // unchanged
}

TEST_F(LinearArenaTest, DefaultConstructor) {
    stl::linear_arena empty;
    EXPECT_EQ(empty.capacity(), 0u);
    EXPECT_EQ(empty.used(), 0u);
    void* p = empty.allocate(1, 1);
    EXPECT_EQ(p, nullptr);
}

TEST_F(LinearArenaTest, ResetTo) {
    arena.allocate(100, 1);
    alignas(64) uint8_t other_buf[2048];
    arena.reset_to(other_buf, 2048);
    EXPECT_EQ(arena.capacity(), 2048u);
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.peak(), 0u);
    EXPECT_EQ(arena.alloc_calls(), 0u);
}

TEST_F(LinearArenaTest, ZeroSizeAllocation) {
    void* p = arena.allocate(0, 1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(arena.alloc_calls(), 1u);
}

TEST_F(LinearArenaTest, ManySmallAllocations) {
    for (size_t i = 0; i < kSize; ++i) {
        void* p = arena.allocate(1, 1);
        ASSERT_NE(p, nullptr);
    }
    EXPECT_EQ(arena.used(), kSize);
    void* p = arena.allocate(1, 1);
    EXPECT_EQ(p, nullptr);
}

// =============================================================================
// fixed_block_pool
// =============================================================================

class FixedBlockPoolTest : public ::testing::Test {
protected:
    static constexpr size_t kArenaSize = 65536;
    alignas(64) uint8_t buffer[kArenaSize];
    stl::linear_arena arena{buffer, kArenaSize};
    stl::fixed_block_pool pool{&arena, 64, 16}; // 64-byte blocks, 16 per chunk
};

TEST_F(FixedBlockPoolTest, InitialState) {
    EXPECT_EQ(pool.block_size(), 64u);
    EXPECT_EQ(pool.chunks(), 0u);
}

TEST_F(FixedBlockPoolTest, SingleAllocate) {
    void* p = pool.allocate();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(pool.chunks(), 1u);
}

TEST_F(FixedBlockPoolTest, AllocateWithinChunk) {
    std::vector<void*> ptrs;
    for (int i = 0; i < 16; ++i) {
        ptrs.push_back(pool.allocate());
    }
    EXPECT_EQ(pool.chunks(), 1u);
    for (auto* p : ptrs)
        EXPECT_NE(p, nullptr);
}

TEST_F(FixedBlockPoolTest, AllocateAcrossChunks) {
    for (int i = 0; i < 17; ++i) {
        pool.allocate();
    }
    EXPECT_EQ(pool.chunks(), 2u);
}

TEST_F(FixedBlockPoolTest, DeallocateAndReuse) {
    void* p1 = pool.allocate();
    pool.deallocate(p1);
    void* p2 = pool.allocate();
    EXPECT_EQ(p1, p2); // reused from free list
}

TEST_F(FixedBlockPoolTest, DeallocateNull) {
    pool.deallocate(nullptr); // should not crash
}

TEST_F(FixedBlockPoolTest, AllocDeallocPattern) {
    // Allocate 16, deallocate all, reallocate 16 - should still be 1 chunk
    std::vector<void*> ptrs;
    for (int i = 0; i < 16; ++i)
        ptrs.push_back(pool.allocate());
    for (auto* p : ptrs)
        pool.deallocate(p);
    for (int i = 0; i < 16; ++i)
        pool.allocate();
    EXPECT_EQ(pool.chunks(), 1u); // no new chunks needed
}

TEST_F(FixedBlockPoolTest, MinimumBlockSize) {
    // Block size smaller than sizeof(node*) should be clamped
    stl::fixed_block_pool small_pool(&arena, 1, 8);
    EXPECT_GE(small_pool.block_size(), sizeof(void*));
    void* p = small_pool.allocate();
    ASSERT_NE(p, nullptr);
}

TEST_F(FixedBlockPoolTest, UniquePointers) {
    std::vector<void*> ptrs;
    for (int i = 0; i < 32; ++i)
        ptrs.push_back(pool.allocate());
    // All pointers should be unique
    for (size_t i = 0; i < ptrs.size(); ++i)
        for (size_t j = i + 1; j < ptrs.size(); ++j)
            EXPECT_NE(ptrs[i], ptrs[j]);
}

TEST_F(FixedBlockPoolTest, DefaultConstructed) {
    stl::fixed_block_pool empty;
    EXPECT_EQ(empty.block_size(), 0u);
    EXPECT_EQ(empty.chunks(), 0u);
}
