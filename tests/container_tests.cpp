#include "sap_core/stl/map.h"
#include "sap_core/stl/string.h"
#include "sap_core/stl/unordered_map.h"
#include "sap_core/stl/vector.h"
#include "sap_core/stl/allocator.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>

// =============================================================================
// stl::vector
// =============================================================================

TEST(Vector, DefaultConstruct) {
    stl::vector<int> v;
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(Vector, PushBack) {
    stl::vector<int> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[2], 3);
}

TEST(Vector, CopyConstruct) {
    stl::vector<int> v1;
    v1.push_back(1);
    v1.push_back(2);
    v1.push_back(3);
    stl::vector<int> v2(v1);
    EXPECT_EQ(v2.size(), 3u);
    EXPECT_EQ(v2[0], 1);
}

TEST(Vector, MoveConstruct) {
    stl::vector<int> v1;
    v1.push_back(1);
    v1.push_back(2);
    v1.push_back(3);
    stl::vector<int> v2(std::move(v1));
    EXPECT_EQ(v2.size(), 3u);
    EXPECT_TRUE(v1.empty());
}

TEST(Vector, CopyAssign) {
    stl::vector<int> v1;
    v1.push_back(1);
    v1.push_back(2);
    v1.push_back(3);
    stl::vector<int> v2;
    v2 = v1;
    EXPECT_EQ(v2.size(), 3u);
}

TEST(Vector, MoveAssign) {
    stl::vector<int> v1;
    v1.push_back(1);
    v1.push_back(2);
    v1.push_back(3);
    stl::vector<int> v2;
    v2 = std::move(v1);
    EXPECT_EQ(v2.size(), 3u);
}

TEST(Vector, WithLinearAllocator) {
    alignas(64) uint8_t buf[65536];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<int> alloc(&arena);

    stl::vector<int, stl::linear_allocator<int>> v(alloc);
    for (int i = 0; i < 200; ++i)
        v.push_back(i);
    EXPECT_EQ(v.size(), 200u);
    for (int i = 0; i < 200; ++i)
        EXPECT_EQ(v[i], i);
}

TEST(Vector, CountConstruct) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<int> alloc(&arena);
    stl::vector<int, stl::linear_allocator<int>> v(10, alloc);
    EXPECT_EQ(v.size(), 10u);
}

TEST(Vector, CountValueConstruct) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<int> alloc(&arena);
    stl::vector<int, stl::linear_allocator<int>> v(5, 42, alloc);
    EXPECT_EQ(v.size(), 5u);
    for (auto& val : v)
        EXPECT_EQ(val, 42);
}

TEST(Vector, IteratorConstruct) {
    std::vector<int> source = {10, 20, 30};
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<int> alloc(&arena);
    stl::vector<int, stl::linear_allocator<int>> v(source.begin(), source.end(), alloc);
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[0], 10);
}

TEST(Vector, MakeVector) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<int> alloc(&arena);
    auto v = stl::make_vector<int>(alloc);
    v.push_back(42);
    EXPECT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0], 42);
}

// =============================================================================
// stl::string
// =============================================================================

TEST(String, DefaultConstruct) {
    stl::string s;
    EXPECT_TRUE(s.empty());
}

TEST(String, ConstructFromCString) {
    stl::string s("hello");
    EXPECT_EQ(s, "hello");
    EXPECT_EQ(s.size(), 5u);
}

TEST(String, ConstructFromStringView) {
    stl::string s(std::string_view("world"));
    EXPECT_EQ(s, "world");
}

TEST(String, ConstructFromStdString) {
    std::string std_s = "test";
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<char> alloc(&arena);
    stl::basic_string<stl::linear_allocator<char>> s(std_s, alloc);
    EXPECT_EQ(std::string_view(s), "test");
}

TEST(String, CopyConstruct) {
    stl::string s1("hello");
    stl::string s2(s1);
    EXPECT_EQ(s2, "hello");
}

TEST(String, MoveConstruct) {
    stl::string s1("hello");
    stl::string s2(std::move(s1));
    EXPECT_EQ(s2, "hello");
}

TEST(String, CopyAssign) {
    stl::string s1("hello");
    stl::string s2;
    s2 = s1;
    EXPECT_EQ(s2, "hello");
}

TEST(String, MoveAssign) {
    stl::string s1("hello");
    stl::string s2;
    s2 = std::move(s1);
    EXPECT_EQ(s2, "hello");
}

TEST(String, AssignCString) {
    stl::string s;
    s = "world";
    EXPECT_EQ(s, "world");
}

TEST(String, AssignStringView) {
    stl::string s;
    s = std::string_view("test");
    EXPECT_EQ(s, "test");
}

TEST(String, AssignStdString) {
    stl::string s;
    s = std::string("hello");
    EXPECT_EQ(s, "hello");
}

TEST(String, MakeString) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<char> alloc(&arena);

    auto s = stl::make_string("hello", alloc);
    EXPECT_EQ(std::string_view(s), "hello");
}

TEST(String, MakeStringFromStringView) {
    alignas(64) uint8_t buf[4096];
    stl::linear_arena arena(buf, sizeof(buf));
    stl::linear_allocator<char> alloc(&arena);

    auto s = stl::make_string(std::string_view("world"), alloc);
    EXPECT_EQ(std::string_view(s), "world");
}

TEST(String, FormatSupport) {
    stl::string s("hello");
    auto formatted = std::format("greeting: {}", s);
    EXPECT_EQ(formatted, "greeting: hello");
}

// =============================================================================
// stl::map
// =============================================================================

TEST(Map, DefaultConstruct) {
    stl::map<int, int> m;
    EXPECT_TRUE(m.empty());
}

TEST(Map, InsertAndLookup) {
    stl::map<int, std::string> m;
    m[1] = "one";
    m[2] = "two";
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m[1], "one");
    EXPECT_EQ(m[2], "two");
}

TEST(Map, InitializerList) {
    alignas(64) uint8_t buf[65536];
    stl::linear_arena arena(buf, sizeof(buf));
    using pair_t = std::pair<const int, int>;
    stl::linear_allocator<pair_t> alloc(&arena);
    stl::map<int, int, std::less<int>, stl::linear_allocator<pair_t>> m({{1, 10}, {2, 20}}, alloc);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m[1], 10);
}

TEST(Map, CopyConstruct) {
    stl::map<int, int> m1;
    m1[1] = 10;
    stl::map<int, int> m2(m1);
    EXPECT_EQ(m2[1], 10);
}

TEST(Map, MoveConstruct) {
    stl::map<int, int> m1;
    m1[1] = 10;
    stl::map<int, int> m2(std::move(m1));
    EXPECT_EQ(m2[1], 10);
    EXPECT_TRUE(m1.empty());
}

TEST(Map, MakeMap) {
    alignas(64) uint8_t buf[65536];
    stl::linear_arena arena(buf, sizeof(buf));
    using pair_t = std::pair<const int, int>;
    stl::linear_allocator<pair_t> alloc(&arena);
    auto m = stl::make_map<int, int>(alloc);
    m[1] = 42;
    EXPECT_EQ(m[1], 42);
}

TEST(Map, Ordering) {
    stl::map<int, int> m;
    m[3] = 30;
    m[1] = 10;
    m[2] = 20;
    std::vector<int> keys;
    for (auto& [k, v] : m)
        keys.push_back(k);
    EXPECT_EQ(keys, (std::vector<int>{1, 2, 3}));
}

// =============================================================================
// stl::unordered_map
// =============================================================================

TEST(UnorderedMap, DefaultConstruct) {
    stl::unordered_map<int, int> m;
    EXPECT_TRUE(m.empty());
}

TEST(UnorderedMap, InsertAndLookup) {
    stl::unordered_map<std::string, int> m;
    m["one"] = 1;
    m["two"] = 2;
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m["one"], 1);
}

TEST(UnorderedMap, CopyConstruct) {
    stl::unordered_map<int, int> m1;
    m1[1] = 10;
    stl::unordered_map<int, int> m2(m1);
    EXPECT_EQ(m2[1], 10);
}

TEST(UnorderedMap, MoveConstruct) {
    stl::unordered_map<int, int> m1;
    m1[1] = 10;
    stl::unordered_map<int, int> m2(std::move(m1));
    EXPECT_EQ(m2[1], 10);
}

// =============================================================================
// stl::string_unordered_map (transparent lookup)
// =============================================================================

TEST(StringUnorderedMap, TransparentLookup) {
    stl::string_unordered_map<int> m;
    m["hello"] = 42;
    // Lookup with string_view — avoids constructing std::string
    auto it = m.find(std::string_view("hello"));
    ASSERT_NE(it, m.end());
    EXPECT_EQ(it->second, 42);
}

TEST(StringUnorderedMap, TransparentLookupMiss) {
    stl::string_unordered_map<int> m;
    m["hello"] = 42;
    auto it = m.find(std::string_view("world"));
    EXPECT_EQ(it, m.end());
}

TEST(StringUnorderedMap, MultipleEntries) {
    stl::string_unordered_map<int> m;
    for (int i = 0; i < 100; ++i)
        m[std::to_string(i)] = i;
    EXPECT_EQ(m.size(), 100u);
    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(m[std::to_string(i)], i);
}

TEST(UnorderedMap, MakeUnorderedMap) {
    alignas(64) uint8_t buf[65536];
    stl::linear_arena arena(buf, sizeof(buf));
    using pair_t = std::pair<const int, int>;
    stl::linear_allocator<pair_t> alloc(&arena);
    auto m = stl::make_unordered_map<int, int>(alloc);
    m[1] = 42;
    EXPECT_EQ(m[1], 42);
}
