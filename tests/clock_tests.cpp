#include <gtest/gtest.h>
#include <sap_core/clock.h>

using namespace sap::core;

// ── SystemMonotonicClock ─────────────────────────────────────────────────────

TEST(SystemMonotonicClock, ReturnsSuccess) {
    system_monotonic_clock clk;
    auto result = clk.now_ns();
    EXPECT_TRUE(result.has_value()) << result.error();
}

TEST(SystemMonotonicClock, ReturnsPositiveValue) {
    system_monotonic_clock clk;
    auto result = clk.now_ns();
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(result.value(), 0);
}

TEST(SystemMonotonicClock, IsMonotonic) {
    system_monotonic_clock clk;
    auto t1 = clk.now_ns();
    auto t2 = clk.now_ns();
    ASSERT_TRUE(t1.has_value());
    ASSERT_TRUE(t2.has_value());
    EXPECT_GE(t2.value(), t1.value());
}

TEST(SystemMonotonicClock, ValueWithinPlausibleUptimeRange) {
    system_monotonic_clock clk;
    auto result = clk.now_ns();
    ASSERT_TRUE(result.has_value());
    // CLOCK_MONOTONIC counts from boot; no real machine has been up 100 years.
    constexpr i64 k100YearsNs = i64(100) * 365LL * 24 * 3600 * 1'000'000'000LL;
    EXPECT_LT(result.value(), k100YearsNs);
}

// ── FakeClock ────────────────────────────────────────────────────────────────

TEST(FakeClock, StartsAtZero) {
    fake_clock clk;
    auto result = clk.now_ns();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0);
}

TEST(FakeClock, AdvanceNsUpdatesTime) {
    fake_clock clk;
    clk.advance_ns(1'000'000);
    EXPECT_EQ(clk.now_ns().value(), 1'000'000);
}

TEST(FakeClock, MultipleAdvancesAccumulate) {
    fake_clock clk;
    clk.advance_ns(500);
    clk.advance_ns(300);
    clk.advance_ns(200);
    EXPECT_EQ(clk.now_ns().value(), 1000);
}
