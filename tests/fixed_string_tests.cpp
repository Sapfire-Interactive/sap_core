#include "sap_core/stl/fixed_string.h"
#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <string_view>

// =============================================================================
// Construction
// =============================================================================

TEST(FixedString, DefaultConstruct) {
    stl::fixed_string<32> s;
    EXPECT_EQ(s.size(), 0u);
    EXPECT_TRUE(s.empty());
    EXPECT_STREQ(s.c_str(), "");
}

TEST(FixedString, ConstructFromCString) {
    stl::fixed_string<32> s("hello");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_STREQ(s.c_str(), "hello");
}

TEST(FixedString, ConstructFromCStringNull) {
    stl::fixed_string<32> s(static_cast<const char*>(nullptr));
    EXPECT_EQ(s.size(), 0u);
}

TEST(FixedString, ConstructFromCStringWithLength) {
    stl::fixed_string<32> s("hello world", 5);
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.view(), "hello");
}

TEST(FixedString, ConstructFromStringView) {
    std::string_view sv = "test";
    stl::fixed_string<32> s(sv);
    EXPECT_EQ(s.view(), "test");
}

TEST(FixedString, ConstructWithCharFill) {
    stl::fixed_string<32> s(5, 'x');
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.view(), "xxxxx");
}

TEST(FixedString, CopyConstruct) {
    stl::fixed_string<32> s1("hello");
    stl::fixed_string<32> s2(s1);
    EXPECT_EQ(s2.view(), "hello");
}

TEST(FixedString, CopyConstructFromDifferentSize) {
    stl::fixed_string<16> small("hi");
    stl::fixed_string<64> large(small);
    EXPECT_EQ(large.view(), "hi");
}

TEST(FixedString, ConstructExactCapacity) {
    stl::fixed_string<5> s("abcde");
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.view(), "abcde");
}

TEST(FixedString, ConstructEmpty) {
    stl::fixed_string<32> s("");
    EXPECT_EQ(s.size(), 0u);
    EXPECT_TRUE(s.empty());
}

// =============================================================================
// Assignment
// =============================================================================

TEST(FixedString, AssignCopy) {
    stl::fixed_string<32> s1("hello");
    stl::fixed_string<32> s2;
    s2 = s1;
    EXPECT_EQ(s2.view(), "hello");
}

TEST(FixedString, AssignCString) {
    stl::fixed_string<32> s;
    s = "world";
    EXPECT_EQ(s.view(), "world");
}

TEST(FixedString, AssignCStringNull) {
    stl::fixed_string<32> s("hello");
    s = static_cast<const char*>(nullptr);
    EXPECT_EQ(s.size(), 0u);
}

TEST(FixedString, AssignStringView) {
    stl::fixed_string<32> s;
    s = std::string_view("test");
    EXPECT_EQ(s.view(), "test");
}

TEST(FixedString, SelfAssign) {
    stl::fixed_string<32> s("hello");
    s = s;
    EXPECT_EQ(s.view(), "hello");
}

// =============================================================================
// Capacity
// =============================================================================

TEST(FixedString, Capacity) {
    stl::fixed_string<64> s;
    EXPECT_EQ(s.capacity(), 64u);
    EXPECT_EQ(s.max_size(), 64u);
}

TEST(FixedString, SizeAndLength) {
    stl::fixed_string<32> s("abc");
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.length(), 3u);
}

TEST(FixedString, Empty) {
    stl::fixed_string<32> s;
    EXPECT_TRUE(s.empty());
    s.push_back('a');
    EXPECT_FALSE(s.empty());
}

// =============================================================================
// Element access
// =============================================================================

TEST(FixedString, SubscriptOperator) {
    stl::fixed_string<32> s("abc");
    EXPECT_EQ(s[0], 'a');
    EXPECT_EQ(s[1], 'b');
    EXPECT_EQ(s[2], 'c');
    s[0] = 'z';
    EXPECT_EQ(s[0], 'z');
}

TEST(FixedString, At) {
    stl::fixed_string<32> s("abc");
    EXPECT_EQ(s.at(0), 'a');
    EXPECT_EQ(s.at(2), 'c');
}

TEST(FixedString, AtConst) {
    const stl::fixed_string<32> s("abc");
    EXPECT_EQ(s.at(0), 'a');
}

TEST(FixedString, FrontBack) {
    stl::fixed_string<32> s("abc");
    EXPECT_EQ(s.front(), 'a');
    EXPECT_EQ(s.back(), 'c');
}

TEST(FixedString, FrontBackMutable) {
    stl::fixed_string<32> s("abc");
    s.front() = 'z';
    s.back() = 'y';
    EXPECT_EQ(s.view(), "zby");
}

// =============================================================================
// Data access & conversions
// =============================================================================

TEST(FixedString, CStr) {
    stl::fixed_string<32> s("hello");
    EXPECT_STREQ(s.c_str(), "hello");
    // Should be null-terminated
    EXPECT_EQ(s.c_str()[5], '\0');
}

TEST(FixedString, Data) {
    stl::fixed_string<32> s("hello");
    EXPECT_EQ(std::string_view(s.data(), s.size()), "hello");
}

TEST(FixedString, ImplicitStringView) {
    stl::fixed_string<32> s("hello");
    std::string_view sv = s;
    EXPECT_EQ(sv, "hello");
}

TEST(FixedString, ViewMethod) {
    stl::fixed_string<32> s("hello");
    EXPECT_EQ(s.view(), "hello");
}

// =============================================================================
// Iterators
// =============================================================================

TEST(FixedString, BeginEnd) {
    stl::fixed_string<32> s("abc");
    std::string collected(s.begin(), s.end());
    EXPECT_EQ(collected, "abc");
}

TEST(FixedString, ConstBeginEnd) {
    const stl::fixed_string<32> s("abc");
    std::string collected(s.begin(), s.end());
    EXPECT_EQ(collected, "abc");
}

TEST(FixedString, CbeginCend) {
    stl::fixed_string<32> s("abc");
    std::string collected(s.cbegin(), s.cend());
    EXPECT_EQ(collected, "abc");
}

TEST(FixedString, RangeFor) {
    stl::fixed_string<32> s("hello");
    std::string collected;
    for (char c : s)
        collected += c;
    EXPECT_EQ(collected, "hello");
}

TEST(FixedString, EmptyIterators) {
    stl::fixed_string<32> s;
    EXPECT_EQ(s.begin(), s.end());
}

// =============================================================================
// Modifiers
// =============================================================================

TEST(FixedString, Clear) {
    stl::fixed_string<32> s("hello");
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_STREQ(s.c_str(), "");
}

TEST(FixedString, PushBack) {
    stl::fixed_string<32> s;
    s.push_back('a');
    s.push_back('b');
    EXPECT_EQ(s.view(), "ab");
}

TEST(FixedString, PopBack) {
    stl::fixed_string<32> s("abc");
    s.pop_back();
    EXPECT_EQ(s.view(), "ab");
    s.pop_back();
    EXPECT_EQ(s.view(), "a");
}

TEST(FixedString, AppendStringView) {
    stl::fixed_string<32> s("hello");
    s.append(" world");
    EXPECT_EQ(s.view(), "hello world");
}

TEST(FixedString, AppendCString) {
    stl::fixed_string<32> s("hello");
    s.append(" world");
    EXPECT_EQ(s.view(), "hello world");
}

TEST(FixedString, AppendCharFill) {
    stl::fixed_string<32> s("hi");
    s.append(3, '!');
    EXPECT_EQ(s.view(), "hi!!!");
}

TEST(FixedString, PlusEqualsStringView) {
    stl::fixed_string<32> s("hello");
    s += " world";
    EXPECT_EQ(s.view(), "hello world");
}

TEST(FixedString, PlusEqualsChar) {
    stl::fixed_string<32> s("hi");
    s += '!';
    EXPECT_EQ(s.view(), "hi!");
}

TEST(FixedString, ResizeGrow) {
    stl::fixed_string<32> s("ab");
    s.resize(5, 'x');
    EXPECT_EQ(s.size(), 5u);
    EXPECT_EQ(s.view(), "abxxx");
}

TEST(FixedString, ResizeShrink) {
    stl::fixed_string<32> s("hello");
    s.resize(3);
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(s.view(), "hel");
}

TEST(FixedString, ResizeSameSize) {
    stl::fixed_string<32> s("abc");
    s.resize(3);
    EXPECT_EQ(s.view(), "abc");
}

TEST(FixedString, ResizeToZero) {
    stl::fixed_string<32> s("hello");
    s.resize(0);
    EXPECT_TRUE(s.empty());
}

// =============================================================================
// Substr & Find
// =============================================================================

TEST(FixedString, SubstrFromPos) {
    stl::fixed_string<32> s("hello world");
    EXPECT_EQ(s.substr(6), "world");
}

TEST(FixedString, SubstrWithCount) {
    stl::fixed_string<32> s("hello world");
    EXPECT_EQ(s.substr(0, 5), "hello");
}

TEST(FixedString, SubstrEnd) {
    stl::fixed_string<32> s("hello");
    EXPECT_EQ(s.substr(5), "");
}

TEST(FixedString, SubstrCountExceedsSize) {
    stl::fixed_string<32> s("abc");
    EXPECT_EQ(s.substr(1, 100), "bc");
}

TEST(FixedString, FindString) {
    stl::fixed_string<32> s("hello world");
    EXPECT_EQ(s.find("world"), 6u);
    EXPECT_EQ(s.find("missing"), stl::fixed_string<32>::npos);
}

TEST(FixedString, FindChar) {
    stl::fixed_string<32> s("hello");
    EXPECT_EQ(s.find('l'), 2u);
    EXPECT_EQ(s.find('z'), stl::fixed_string<32>::npos);
}

TEST(FixedString, FindFromPos) {
    stl::fixed_string<32> s("hello hello");
    EXPECT_EQ(s.find("hello", 1), 6u);
}

TEST(FixedString, FindEmpty) {
    stl::fixed_string<32> s("hello");
    EXPECT_EQ(s.find(""), 0u);
}

// =============================================================================
// Comparisons
// =============================================================================

TEST(FixedString, EqualityFixedString) {
    stl::fixed_string<32> a("hello");
    stl::fixed_string<32> b("hello");
    stl::fixed_string<32> c("world");
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(FixedString, EqualityStringView) {
    stl::fixed_string<32> s("hello");
    EXPECT_EQ(s, std::string_view("hello"));
    EXPECT_NE(s, std::string_view("world"));
}

TEST(FixedString, EqualityCString) {
    stl::fixed_string<32> s("hello");
    EXPECT_TRUE(s == "hello");
    EXPECT_FALSE(s == "world");
}

TEST(FixedString, Ordering) {
    stl::fixed_string<32> a("abc");
    stl::fixed_string<32> b("abd");
    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
    EXPECT_LE(a, a);
    EXPECT_GE(b, b);
}

TEST(FixedString, OrderingWithStringView) {
    stl::fixed_string<32> s("abc");
    EXPECT_LT(s, std::string_view("abd"));
    EXPECT_GT(s, std::string_view("abb"));
}

// =============================================================================
// Concatenation
// =============================================================================

TEST(FixedString, ConcatTwoFixedStrings) {
    stl::fixed_string<16> a("hello");
    stl::fixed_string<16> b(" world");
    auto result = a + b;
    EXPECT_EQ(result.view(), "hello world");
    EXPECT_EQ(result.capacity(), 32u);
}

TEST(FixedString, ConcatFixedStringAndStringView) {
    stl::fixed_string<32> s("hello");
    auto result = s + std::string_view(" world");
    EXPECT_EQ(result.view(), "hello world");
}

TEST(FixedString, ConcatStringViewAndFixedString) {
    stl::fixed_string<32> s(" world");
    auto result = std::string_view("hello") + s;
    EXPECT_EQ(result.view(), "hello world");
}

// =============================================================================
// Stream output
// =============================================================================

TEST(FixedString, StreamOutput) {
    stl::fixed_string<32> s("hello");
    std::ostringstream oss;
    oss << s;
    EXPECT_EQ(oss.str(), "hello");
}

TEST(FixedString, StreamOutputEmpty) {
    stl::fixed_string<32> s;
    std::ostringstream oss;
    oss << s;
    EXPECT_EQ(oss.str(), "");
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(FixedString, SingleCharCapacity) {
    stl::fixed_string<1> s;
    s.push_back('x');
    EXPECT_EQ(s.size(), 1u);
    EXPECT_EQ(s.view(), "x");
}

TEST(FixedString, NullTerminationAfterModifications) {
    stl::fixed_string<32> s("hello");
    s.resize(3);
    EXPECT_EQ(s.c_str()[3], '\0');
    s.push_back('!');
    EXPECT_EQ(s.c_str()[4], '\0');
    s.clear();
    EXPECT_EQ(s.c_str()[0], '\0');
}

TEST(FixedString, BuildUpCharByChar) {
    stl::fixed_string<26> s;
    for (char c = 'a'; c <= 'z'; ++c)
        s.push_back(c);
    EXPECT_EQ(s.size(), 26u);
    EXPECT_EQ(s.view(), "abcdefghijklmnopqrstuvwxyz");
}
