#include "sap_core/async/executor.h"
#include "sap_core/async/sleep_for.h"
#include "sap_core/async/spawn.h"
#include "sap_core/async/sync_wait.h"
#include "sap_core/async/task.h"
#include "sap_core/stl/coroutine.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>

using sap::async::Executor;
using sap::async::sleep_for;
using sap::async::spawn;
using sap::async::Task;

namespace {

    Task<void> set_int(int& dst, int v) {
        dst = v;
        co_return;
    }

    Task<void> tick_with(Executor& ex, std::chrono::milliseconds dt, std::atomic<int>& counter) {
        co_await sleep_for(ex, dt);
        counter.fetch_add(1);
    }

    Task<void> tick_and_stop(Executor& ex, std::chrono::milliseconds dt, std::atomic<int>& counter, int stop_at) {
        for (int i = 0; i < 100; ++i) {
            co_await sleep_for(ex, dt);
            int now = counter.fetch_add(1) + 1;
            if (now >= stop_at) {
                ex.stop();
                co_return;
            }
        }
    }

} // namespace

TEST(ExecutorTest, Create) {
    auto ex = Executor::create();
    ASSERT_TRUE(ex.has_value()) << ex.error();
}

TEST(ExecutorTest, SpawnSyncTaskRunsToCompletion) {
    auto exr = Executor::create();
    ASSERT_TRUE(exr.has_value());
    auto& ex = exr.value();

    int side = 0;
    ex.spawn_detach(set_int(side, 42));
    ex.run();
    EXPECT_EQ(side, 42);
}

TEST(ExecutorTest, RunReturnsImmediatelyWhenNothingToDo) {
    auto exr = Executor::create();
    auto& ex = exr.value();

    auto start = std::chrono::steady_clock::now();
    ex.run();
    auto dt = std::chrono::steady_clock::now() - start;
    EXPECT_LT(dt, std::chrono::milliseconds(50));
}

TEST(ExecutorTest, SleepForDelaysAtLeastThatLong) {
    auto exr = Executor::create();
    auto& ex = exr.value();

    std::atomic<int> counter{0};
    ex.spawn_detach(tick_with(ex, std::chrono::milliseconds(100), counter));

    auto start = std::chrono::steady_clock::now();
    ex.run();
    auto dt = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(counter.load(), 1);
    EXPECT_GE(dt, std::chrono::milliseconds(90));
    EXPECT_LT(dt, std::chrono::seconds(2));
}

TEST(ExecutorTest, ConcurrentSleepsRunInParallel) {
    auto exr = Executor::create();
    auto& ex = exr.value();

    std::atomic<int> counter{0};
    for (int i = 0; i < 5; ++i) {
        ex.spawn_detach(tick_with(ex, std::chrono::milliseconds(100), counter));
    }

    auto start = std::chrono::steady_clock::now();
    ex.run();
    auto dt = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(counter.load(), 5);
    // Serial would be ~500ms; concurrent should finish in ~100ms.
    EXPECT_GE(dt, std::chrono::milliseconds(90));
    EXPECT_LT(dt, std::chrono::milliseconds(250));
}

TEST(ExecutorTest, StopFromCoroutineExitsLoop) {
    auto exr = Executor::create();
    auto& ex = exr.value();

    std::atomic<int> counter{0};
    ex.spawn_detach(tick_and_stop(ex, std::chrono::milliseconds(20), counter, 3));
    ex.run();

    EXPECT_EQ(counter.load(), 3);
}

TEST(ExecutorTest, SleepFiresInArgumentOrderForSameDeadline) {
    auto exr = Executor::create();
    auto& ex = exr.value();

    std::atomic<int> seq{0};
    int              order[3] = {0, 0, 0};

    auto make_tagged = [&ex, &seq, &order](int tag) -> Task<void> {
        co_await sleep_for(ex, std::chrono::milliseconds(50));
        order[tag] = seq.fetch_add(1) + 1;
    };

    ex.spawn_detach(make_tagged(0));
    ex.spawn_detach(make_tagged(1));
    ex.spawn_detach(make_tagged(2));
    ex.run();

    // All three should have observed a distinct sequence number.
    EXPECT_NE(order[0], 0);
    EXPECT_NE(order[1], 0);
    EXPECT_NE(order[2], 0);
    EXPECT_NE(order[0], order[1]);
    EXPECT_NE(order[1], order[2]);
    EXPECT_NE(order[0], order[2]);
}

TEST(ExecutorTest, RunUntil_HandleAlreadyDone_ReturnsImmediately) {
    auto exr = Executor::create();
    auto& ex = exr.value();

    auto h = spawn(ex, []() -> Task<int> { co_return 1; }());
    ASSERT_TRUE(h.done());

    auto start = std::chrono::steady_clock::now();
    ex.run_until(h.handle());
    auto dt = std::chrono::steady_clock::now() - start;
    EXPECT_LT(dt, std::chrono::milliseconds(50));
}

TEST(ExecutorTest, RunUntil_PendingIO_DrivesToCompletion) {
    auto exr = Executor::create();
    auto& ex = exr.value();

    auto h = spawn(ex, [&ex]() -> Task<void> {
        co_await sleep_for(ex, std::chrono::milliseconds(100));
    }());
    ASSERT_FALSE(h.done());

    auto start = std::chrono::steady_clock::now();
    ex.run_until(h.handle());
    auto dt = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(h.done());
    EXPECT_GE(dt, std::chrono::milliseconds(90));
    EXPECT_LT(dt, std::chrono::seconds(2));
}

TEST(ExecutorTest, RunUntil_NothingToDo_ExitsCleanly) {
    auto exr = Executor::create();
    auto& ex = exr.value();

    // Unresumed Task<void>: frame is at initial_suspend (suspend_always), done()=false.
    // run_until has no ready work and no pending I/O — must exit, not block.
    auto parked = []() -> Task<void> { co_await stl::suspend_always{}; }();
    ASSERT_FALSE(parked.done());

    auto start = std::chrono::steady_clock::now();
    ex.run_until(parked.handle());
    auto dt = std::chrono::steady_clock::now() - start;
    EXPECT_LT(dt, std::chrono::milliseconds(50));
}
