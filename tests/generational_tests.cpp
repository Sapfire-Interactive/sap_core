#include "sap_core/stl/generational.h"
#include <gtest/gtest.h>

#include <set>
#include <string>

// =============================================================================
// generational_index_allocator
// =============================================================================

TEST(GenIndexAllocator, AllocateFirstIndex) {
    stl::generational_index_allocator<> alloc;
    auto idx = alloc.allocate();
    EXPECT_EQ(idx.index, 0u);
    EXPECT_EQ(idx.generation, 0u);
    EXPECT_TRUE(alloc.is_alive(idx));
}

TEST(GenIndexAllocator, AllocateMultiple) {
    stl::generational_index_allocator<> alloc;
    auto a = alloc.allocate();
    auto b = alloc.allocate();
    auto c = alloc.allocate();
    EXPECT_EQ(a.index, 0u);
    EXPECT_EQ(b.index, 1u);
    EXPECT_EQ(c.index, 2u);
    EXPECT_TRUE(alloc.is_alive(a));
    EXPECT_TRUE(alloc.is_alive(b));
    EXPECT_TRUE(alloc.is_alive(c));
}

TEST(GenIndexAllocator, DeallocateMarksDead) {
    stl::generational_index_allocator<> alloc;
    auto idx = alloc.allocate();
    alloc.deallocate(idx);
    EXPECT_FALSE(alloc.is_alive(idx));
}

TEST(GenIndexAllocator, ReuseWithIncrementedGeneration) {
    stl::generational_index_allocator<> alloc;
    auto first = alloc.allocate();
    alloc.deallocate(first);
    auto second = alloc.allocate();
    EXPECT_EQ(second.index, first.index); // same slot
    EXPECT_EQ(second.generation, first.generation + 1); // new generation
    EXPECT_TRUE(alloc.is_alive(second));
    EXPECT_FALSE(alloc.is_alive(first)); // old handle is stale
}

TEST(GenIndexAllocator, MultipleReuseIncrementsGeneration) {
    stl::generational_index_allocator<> alloc;
    auto idx = alloc.allocate();
    for (int i = 0; i < 10; ++i) {
        alloc.deallocate(idx);
        idx = alloc.allocate();
    }
    EXPECT_EQ(idx.index, 0u);
    EXPECT_EQ(idx.generation, 10u);
}

TEST(GenIndexAllocator, DoubleDeallocateIgnored) {
    stl::generational_index_allocator<> alloc;
    auto idx = alloc.allocate();
    alloc.deallocate(idx);
    alloc.deallocate(idx); // second dealloc with stale generation — is_alive returns false
    // Should not corrupt state
    auto next = alloc.allocate();
    EXPECT_TRUE(alloc.is_alive(next));
}

TEST(GenIndexAllocator, IsAliveOutOfRange) {
    stl::generational_index_allocator<> alloc;
    stl::generational_index bogus{999, 0};
    EXPECT_FALSE(alloc.is_alive(bogus));
}

TEST(GenIndexAllocator, IsAliveWrongGeneration) {
    stl::generational_index_allocator<> alloc;
    auto idx = alloc.allocate();
    stl::generational_index stale{idx.index, idx.generation + 1};
    EXPECT_FALSE(alloc.is_alive(stale));
}

TEST(GenIndexAllocator, AllocDeallocPattern) {
    stl::generational_index_allocator<> alloc;
    // Allocate 5, deallocate middle 3, allocate 3 again
    std::vector<stl::generational_index> indices;
    for (int i = 0; i < 5; ++i)
        indices.push_back(alloc.allocate());

    alloc.deallocate(indices[1]);
    alloc.deallocate(indices[2]);
    alloc.deallocate(indices[3]);

    EXPECT_TRUE(alloc.is_alive(indices[0]));
    EXPECT_FALSE(alloc.is_alive(indices[1]));
    EXPECT_TRUE(alloc.is_alive(indices[4]));

    // Reallocate - should reuse indices 3, 2, 1 (LIFO free list)
    auto r1 = alloc.allocate();
    auto r2 = alloc.allocate();
    auto r3 = alloc.allocate();
    std::set<u32> reused = {r1.index, r2.index, r3.index};
    EXPECT_TRUE(reused.count(1));
    EXPECT_TRUE(reused.count(2));
    EXPECT_TRUE(reused.count(3));
}

// =============================================================================
// generational_vector
// =============================================================================

TEST(GenVector, SetAndGet) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<std::string> vec;

    auto idx = alloc.allocate();
    vec.set(idx, "hello");
    auto* val = vec.get(idx);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "hello");
}

TEST(GenVector, GetInvalidIndex) {
    stl::generational_vector<int> vec;
    stl::generational_index bogus{100, 0};
    EXPECT_EQ(vec.get(bogus), nullptr);
}

TEST(GenVector, StaleIndexReturnsNull) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;

    auto idx = alloc.allocate();
    vec.set(idx, 42);
    alloc.deallocate(idx);

    auto new_idx = alloc.allocate();
    vec.set(new_idx, 99);

    EXPECT_EQ(vec.get(idx), nullptr); // stale
    EXPECT_NE(vec.get(new_idx), nullptr);
    EXPECT_EQ(*vec.get(new_idx), 99);
}

TEST(GenVector, Remove) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;

    auto idx = alloc.allocate();
    vec.set(idx, 42);
    vec.remove(idx);
    EXPECT_EQ(vec.get(idx), nullptr);
}

TEST(GenVector, RemoveOutOfRange) {
    stl::generational_vector<int> vec;
    stl::generational_index bogus{100, 0};
    vec.remove(bogus); // should not crash
}

TEST(GenVector, ConstGet) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;
    auto idx = alloc.allocate();
    vec.set(idx, 42);

    const auto& cvec = vec;
    const int* val = cvec.get(idx);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, 42);
}

TEST(GenVector, AutoExpand) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;

    // Allocate indices 0..99
    std::vector<stl::generational_index> indices;
    for (int i = 0; i < 100; ++i) {
        auto idx = alloc.allocate();
        vec.set(idx, i);
        indices.push_back(idx);
    }

    for (int i = 0; i < 100; ++i) {
        auto* val = vec.get(indices[i]);
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(*val, i);
    }
}

TEST(GenVector, GetAllValidIndices) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;

    auto a = alloc.allocate();
    auto b = alloc.allocate();
    auto c = alloc.allocate();
    vec.set(a, 1);
    vec.set(b, 2);
    vec.set(c, 3);
    alloc.deallocate(b);

    auto valid = vec.get_all_valid_indices(alloc);
    EXPECT_EQ(valid.size(), 2u);
    std::set<u32> indices;
    for (auto& idx : valid)
        indices.insert(idx.index);
    EXPECT_TRUE(indices.count(a.index));
    EXPECT_TRUE(indices.count(c.index));
}

TEST(GenVector, GetFirstValidEntry) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<std::string> vec;

    auto a = alloc.allocate();
    auto b = alloc.allocate();
    vec.set(a, "first");
    vec.set(b, "second");
    alloc.deallocate(a);

    auto entry = vec.get_first_valid_entry(alloc);
    ASSERT_TRUE(entry.has_value());
    auto [idx, val_ref] = *entry;
    EXPECT_EQ(idx.index, b.index);
    EXPECT_EQ(val_ref.get(), "second");
}

TEST(GenVector, GetFirstValidEntryEmpty) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;
    auto entry = vec.get_first_valid_entry(alloc);
    EXPECT_FALSE(entry.has_value());
}

TEST(GenVector, Size) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;
    EXPECT_EQ(vec.size(), 0u);
    auto a = alloc.allocate();
    vec.set(a, 1);
    EXPECT_EQ(vec.size(), 1u);
    auto b = alloc.allocate();
    vec.set(b, 2);
    EXPECT_EQ(vec.size(), 2u);
}

TEST(GenVector, Iterators) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;
    auto a = alloc.allocate();
    auto b = alloc.allocate();
    vec.set(a, 10);
    vec.set(b, 20);

    int count = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        if (*it)
            ++count;
    }
    EXPECT_EQ(count, 2);
}

TEST(GenVector, OverwriteValue) {
    stl::generational_index_allocator<> alloc;
    stl::generational_vector<int> vec;
    auto idx = alloc.allocate();
    vec.set(idx, 1);
    vec.set(idx, 2);
    EXPECT_EQ(*vec.get(idx), 2);
}
