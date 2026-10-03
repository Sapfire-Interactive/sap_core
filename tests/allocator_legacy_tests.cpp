#include <gtest/gtest.h>
#include <sap_core/serialization.h>
#include <sap_core/stl/allocator.h>
#include <sap_core/stl/vector.h>

#include <type_traits>

// Compile the ordinary sap_core configuration without a consumer allocator hook.
// Existing projects retain their allocator, constructors and serialization API.
static_assert(std::is_same_v<stl::vector<u32>::allocator_type, std::allocator<u32>>);
static_assert(std::is_default_constructible_v<stl::vector<u32>>);
static_assert(std::is_same_v<decltype(std::declval<stl::vector<u32>&>().reserve(1024)), void>);
static_assert(std::is_same_v<decltype(sap::ByteWriter{}.buffer()), const stl::vector<stl::byte>&>);

TEST(AllocatorLegacy, DefaultConstructorsKeepOrdinaryAllocator) {
    stl::vector<u32> empty;
    stl::vector<u32> count(3);
    stl::vector<u32> copies(3, 7);
    stl::vector<u32> list{1, 2, 3};

    EXPECT_TRUE(empty.empty());
    ASSERT_EQ(count.size(), 3u);
    EXPECT_EQ(count[0], 0u);
    EXPECT_EQ(count[2], 0u);
    ASSERT_EQ(copies.size(), 3u);
    EXPECT_EQ(copies[0], 7u);
    EXPECT_EQ(copies[2], 7u);
    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0], 1u);
    EXPECT_EQ(list[2], 3u);
}

TEST(AllocatorLegacy, RangeCopyAndMoveConstructorsRemainCompatible) {
    stl::vector<u32> list{1, 2, 3};
    stl::vector<u32> range(list.begin(), list.end());
    stl::vector<u32> copy(range);
    stl::vector<u32> moved(static_cast<stl::vector<u32>&&>(copy));

    ASSERT_EQ(range.size(), 3u);
    ASSERT_EQ(moved.size(), 3u);
    for (u32 index = 0; index < 3; ++index) {
        EXPECT_EQ(range[index], list[index]);
        EXPECT_EQ(moved[index], list[index]);
    }
}

TEST(AllocatorLegacy, ExplicitAllocatorFactoryRemainsCompatible) {
    alignas(u32) u8 storage[256]{};
    stl::linear_arena arena(storage, sizeof(storage));
    auto values = stl::make_vector<u32>(stl::linear_allocator<u32>(&arena));
    values.push_back(9);

    EXPECT_EQ(values.get_allocator().arena(), &arena);
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0], 9u);
    EXPECT_GT(arena.used(), 0u);
}
