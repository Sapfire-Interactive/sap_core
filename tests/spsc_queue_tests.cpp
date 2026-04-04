#include "sap_core/stl/spsc_queue.h"
#include <gtest/gtest.h>

#include <thread>
#include <vector>

// =============================================================================
// Basic operations
// =============================================================================

TEST(SpscQueue, InitiallyEmpty) {
    stl::spsc_queue<int, 16> q;
    int val;
    EXPECT_FALSE(q.try_pop(val));
}

TEST(SpscQueue, PushAndPop) {
    stl::spsc_queue<int, 16> q;
    EXPECT_TRUE(q.try_push(42));
    int val;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 42);
}

TEST(SpscQueue, FIFO) {
    stl::spsc_queue<int, 16> q;
    q.try_push(1);
    q.try_push(2);
    q.try_push(3);
    int val;
    q.try_pop(val);
    EXPECT_EQ(val, 1);
    q.try_pop(val);
    EXPECT_EQ(val, 2);
    q.try_pop(val);
    EXPECT_EQ(val, 3);
}

TEST(SpscQueue, PopEmptyReturnsFalse) {
    stl::spsc_queue<int, 16> q;
    int val = 999;
    EXPECT_FALSE(q.try_pop(val));
    EXPECT_EQ(val, 999); // unchanged
}

TEST(SpscQueue, FillToCapacity) {
    // Capacity is 16, but ring buffer uses one slot as sentinel,
    // so effective capacity is 15
    stl::spsc_queue<int, 16> q;
    for (int i = 0; i < 15; ++i)
        EXPECT_TRUE(q.try_push(i));
    EXPECT_FALSE(q.try_push(99)); // full
}

TEST(SpscQueue, FullThenDrain) {
    stl::spsc_queue<int, 16> q;
    for (int i = 0; i < 15; ++i)
        q.try_push(i);
    for (int i = 0; i < 15; ++i) {
        int val;
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, i);
    }
    int val;
    EXPECT_FALSE(q.try_pop(val));
}

TEST(SpscQueue, WrapAround) {
    stl::spsc_queue<int, 4> q; // capacity 4, effective 3
    // Push 3, pop 3, push 3 again to force wraparound
    for (int round = 0; round < 10; ++round) {
        for (int i = 0; i < 3; ++i)
            EXPECT_TRUE(q.try_push(round * 3 + i));
        for (int i = 0; i < 3; ++i) {
            int val;
            EXPECT_TRUE(q.try_pop(val));
            EXPECT_EQ(val, round * 3 + i);
        }
    }
}

// =============================================================================
// Move semantics
// =============================================================================

TEST(SpscQueue, TryPushMove) {
    stl::spsc_queue<std::string, 4> q;
    std::string s = "hello";
    EXPECT_TRUE(q.try_push(std::move(s)));
    std::string out;
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, "hello");
}

TEST(SpscQueue, TryPushMoveFull) {
    stl::spsc_queue<int, 4> q;
    for (int i = 0; i < 3; ++i)
        q.try_push(i);
    int val = 99;
    EXPECT_FALSE(q.try_push(std::move(val)));
}

// =============================================================================
// Concurrent producer/consumer
// =============================================================================

TEST(SpscQueue, SingleProducerSingleConsumer) {
    constexpr int count = 100000;
    stl::spsc_queue<int, 4096> q;
    std::vector<int> received;
    received.reserve(count);

    std::thread producer([&] {
        for (int i = 0; i < count; ++i) {
            while (!q.try_push(i)) {
                // spin
            }
        }
    });

    std::thread consumer([&] {
        for (int i = 0; i < count; ++i) {
            int val;
            while (!q.try_pop(val)) {
                // spin
            }
            received.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        EXPECT_EQ(received[i], i);
}

TEST(SpscQueue, HighThroughput) {
    constexpr int count = 1000000;
    stl::spsc_queue<u64, 8192> q;
    std::atomic<u64> sum_produced{0};
    std::atomic<u64> sum_consumed{0};

    std::thread producer([&] {
        for (u64 i = 0; i < count; ++i) {
            while (!q.try_push(i)) {}
            sum_produced.fetch_add(i, std::memory_order_relaxed);
        }
    });

    std::thread consumer([&] {
        for (int i = 0; i < count; ++i) {
            u64 val;
            while (!q.try_pop(val)) {}
            sum_consumed.fetch_add(val, std::memory_order_relaxed);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(sum_produced.load(), sum_consumed.load());
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(SpscQueue, MinimumCapacity) {
    stl::spsc_queue<int, 2> q; // effective capacity = 1
    EXPECT_TRUE(q.try_push(1));
    EXPECT_FALSE(q.try_push(2));
    int val;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 1);
}

TEST(SpscQueue, NonTrivialType) {
    struct Obj {
        int x;
        std::string name;
    };

    stl::spsc_queue<Obj, 8> q;
    q.try_push(Obj{42, "test"});
    Obj out;
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out.x, 42);
    EXPECT_EQ(out.name, "test");
}

TEST(SpscQueue, AlternatingPushPop) {
    stl::spsc_queue<int, 4> q;
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(q.try_push(i));
        int val;
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, i);
    }
}
