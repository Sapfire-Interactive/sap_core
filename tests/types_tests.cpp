#include "sap_core/types.h"
#include "sap_core/timestamp.h"
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>

// =============================================================================
// Type sizes
// =============================================================================

TEST(Types, UnsignedSizes) {
    EXPECT_EQ(sizeof(u8), 1u);
    EXPECT_EQ(sizeof(u16), 2u);
    EXPECT_EQ(sizeof(u32), 4u);
    EXPECT_EQ(sizeof(u64), 8u);
}

TEST(Types, SignedSizes) {
    EXPECT_EQ(sizeof(i8), 1u);
    EXPECT_EQ(sizeof(i16), 2u);
    EXPECT_EQ(sizeof(i32), 4u);
    EXPECT_EQ(sizeof(i64), 8u);
}

TEST(Types, FloatSizes) {
    EXPECT_EQ(sizeof(f32), 4u);
    EXPECT_EQ(sizeof(f64), 8u);
}

// =============================================================================
// Type ranges
// =============================================================================

TEST(Types, U8Range) {
    EXPECT_EQ(std::numeric_limits<u8>::min(), 0u);
    EXPECT_EQ(std::numeric_limits<u8>::max(), 255u);
}

TEST(Types, I8Range) {
    EXPECT_EQ(std::numeric_limits<i8>::min(), -128);
    EXPECT_EQ(std::numeric_limits<i8>::max(), 127);
}

TEST(Types, U32Range) {
    EXPECT_EQ(std::numeric_limits<u32>::max(), 4294967295u);
}

// =============================================================================
// Memory size helpers
// =============================================================================

TEST(Types, Kibibytes) {
    EXPECT_EQ(stl::kibibytes(1), 1024u);
    EXPECT_EQ(stl::kibibytes(4), 4096u);
}

TEST(Types, Mebibytes) {
    EXPECT_EQ(stl::mebibytes(1), 1024u * 1024u);
    EXPECT_EQ(stl::mebibytes(2), 2u * 1024u * 1024u);
}

TEST(Types, Gibibytes) {
    EXPECT_EQ(stl::gibibytes(1), 1024ULL * 1024ULL * 1024ULL);
}

TEST(Types, Kilobytes) {
    EXPECT_EQ(stl::kilobytes(1), 1000u);
}

TEST(Types, Megabytes) {
    EXPECT_EQ(stl::megabytes(1), 1000000u);
}

TEST(Types, Gigabytes) {
    EXPECT_EQ(stl::gigabytes(1), 1000000000ULL);
}

TEST(Types, MemoryHelpersZero) {
    EXPECT_EQ(stl::kibibytes(0), 0u);
    EXPECT_EQ(stl::mebibytes(0), 0u);
    EXPECT_EQ(stl::gibibytes(0), 0u);
    EXPECT_EQ(stl::kilobytes(0), 0u);
    EXPECT_EQ(stl::megabytes(0), 0u);
    EXPECT_EQ(stl::gigabytes(0), 0u);
}

// =============================================================================
// STL aliases compile checks
// =============================================================================

TEST(Types, MutexCompiles) {
    stl::mutex m;
    stl::lock_guard<stl::mutex> lock(m);
    (void)lock;
}

TEST(Types, AtomicCompiles) {
    stl::atomic<int> a{42};
    EXPECT_EQ(a.load(), 42);
    a.store(99);
    EXPECT_EQ(a.load(), 99);
}

TEST(Types, OptionalCompiles) {
    stl::optional<int> opt;
    EXPECT_FALSE(opt.has_value());
    opt = 42;
    EXPECT_EQ(*opt, 42);
}

TEST(Types, FunctionCompiles) {
    stl::function<int(int)> fn = [](int x) { return x * 2; };
    EXPECT_EQ(fn(21), 42);
}

TEST(Types, QueueCompiles) {
    stl::queue<int> q;
    q.push(1);
    q.push(2);
    EXPECT_EQ(q.front(), 1);
    q.pop();
    EXPECT_EQ(q.front(), 2);
}

// =============================================================================
// Timestamp
// =============================================================================

TEST(Timestamp, NowMsReturnsPositive) {
    Timestamp t = now_ms();
    EXPECT_GT(t, 0);
}

TEST(Timestamp, NowMsIncreases) {
    Timestamp t1 = now_ms();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Timestamp t2 = now_ms();
    EXPECT_GE(t2, t1);
}

TEST(Timestamp, ReasonableEpochRange) {
    Timestamp t = now_ms();
    // Should be after year 2020 (roughly 1577836800000 ms)
    EXPECT_GT(t, 1577836800000LL);
    // And before year 2100
    EXPECT_LT(t, 4102444800000LL);
}
