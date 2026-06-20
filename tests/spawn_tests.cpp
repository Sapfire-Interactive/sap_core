#include "sap_core/async/executor.h"
#include "sap_core/async/sleep_for.h"
#include "sap_core/async/spawn.h"
#include "sap_core/async/sync_wait.h"
#include "sap_core/async/task.h"
#include "sap_core/stl/unique_ptr.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>

using sap::async::Executor;
using sap::async::sleep_for;
using sap::async::spawn;
using sap::async::SpawnHandle;
using sap::async::sync_wait;
using sap::async::Task;

namespace {

    Task<int> just_return(int v) { co_return v; }

    Task<void> set_flag(std::atomic<bool>& flag) {
        flag.store(true);
        co_return;
    }

    Task<stl::unique_ptr<int>> make_ptr(int v) {
        co_return stl::unique_ptr<int>{new int(v)};
    }

    Task<int> throw_runtime() {
        throw std::runtime_error("boom");
        co_return 0;
    }

    Task<int> sleep_then_return(Executor& ex, std::chrono::milliseconds dt, int v) {
        co_await sleep_for(ex, dt);
        co_return v;
    }

    Task<void> sleep_then_set(Executor& ex, std::chrono::milliseconds dt, std::atomic<int>& counter) {
        co_await sleep_for(ex, dt);
        counter.fetch_add(1);
    }

    Task<int> drive_two_spawns(Executor& ex) {
        auto h1 = spawn(ex, sleep_then_return(ex, std::chrono::milliseconds(100), 1));
        auto h2 = spawn(ex, sleep_then_return(ex, std::chrono::milliseconds(100), 2));
        int a = co_await stl::move(h1);
        int b = co_await stl::move(h2);
        co_return a + b;
    }

    Task<int> drive_concurrent_with_sleep(Executor& ex) {
        auto h = spawn(ex, sleep_then_return(ex, std::chrono::milliseconds(100), 7));
        co_await sleep_for(ex, std::chrono::milliseconds(100));
        co_return co_await stl::move(h);
    }

} // namespace

TEST(SpawnTest, RunsSynchronouslyForNonSuspendingTask) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto h   = spawn(ex, just_return(42));
    EXPECT_TRUE(h.done());
}

TEST(SpawnTest, ReturnsValue) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto h   = spawn(ex, just_return(99));
    EXPECT_EQ(sync_wait(stl::move(h)), 99);
}

TEST(SpawnTest, ReturnsVoid) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    std::atomic<bool> flag{false};
    auto h = spawn(ex, set_flag(flag));
    sync_wait(stl::move(h));
    EXPECT_TRUE(flag.load());
}

TEST(SpawnTest, ReturnsMoveOnly) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto h   = spawn(ex, make_ptr(13));
    auto p   = sync_wait(stl::move(h));
    ASSERT_TRUE(p);
    EXPECT_EQ(*p, 13);
}

TEST(SpawnTest, StartsTaskImmediately) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    std::atomic<bool> flag{false};
    {
        auto h = spawn(ex, set_flag(flag));
        EXPECT_TRUE(flag.load());
        (void)h;
    }
}

TEST(SpawnTest, TwoOverlap) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto driver = drive_two_spawns(ex);
    auto h      = spawn(ex, stl::move(driver));
    auto start  = std::chrono::steady_clock::now();
    int  total  = sync_wait(stl::move(h));
    auto dt     = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(total, 3);
    EXPECT_GE(dt, std::chrono::milliseconds(90));
    EXPECT_LT(dt, std::chrono::milliseconds(250));
}

TEST(SpawnTest, ConcurrentWithSyncBefore) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto driver = drive_concurrent_with_sleep(ex);
    auto h      = spawn(ex, stl::move(driver));
    auto start  = std::chrono::steady_clock::now();
    int  v      = sync_wait(stl::move(h));
    auto dt     = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(v, 7);
    EXPECT_GE(dt, std::chrono::milliseconds(90));
    EXPECT_LT(dt, std::chrono::milliseconds(250));
}

TEST(SpawnTest, ExceptionRethrownAtAwait) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto h   = spawn(ex, throw_runtime());
    EXPECT_THROW(sync_wait(stl::move(h)), std::runtime_error);
}

TEST(SpawnTest, DroppedHandle_PendingTask_RunsToCompletion) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    std::atomic<int> counter{0};
    {
        auto h = spawn(ex, sleep_then_set(ex, std::chrono::milliseconds(50), counter));
        EXPECT_EQ(counter.load(), 0);
        (void)h;
    }
    ex.run();
    EXPECT_EQ(counter.load(), 1);
}

TEST(SpawnTest, DroppedHandle_DoneTask_NoLeak) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    {
        auto h = spawn(ex, just_return(1));
        EXPECT_TRUE(h.done());
        (void)h;
    }
}

TEST(SpawnTest, MoveOnly) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto h1  = spawn(ex, just_return(5));
    auto h2  = stl::move(h1);
    EXPECT_FALSE(static_cast<bool>(h1));
    EXPECT_TRUE(static_cast<bool>(h2));
    EXPECT_EQ(sync_wait(stl::move(h2)), 5);
}

TEST(SpawnTest, InsideTaskChainedAwait) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto driver = [&ex]() -> Task<int> {
        auto h = spawn(ex, sleep_then_return(ex, std::chrono::milliseconds(100), 11));
        co_await sleep_for(ex, std::chrono::milliseconds(100));
        co_return co_await stl::move(h);
    }();
    auto outer = spawn(ex, stl::move(driver));
    auto start = std::chrono::steady_clock::now();
    int  v     = sync_wait(stl::move(outer));
    auto dt    = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(v, 11);
    EXPECT_GE(dt, std::chrono::milliseconds(90));
    EXPECT_LT(dt, std::chrono::milliseconds(250));
}

TEST(SpawnTest, SyncWaitSpawnHandle_DoneTask_ReturnsValue) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto h   = spawn(ex, just_return(123));
    EXPECT_TRUE(h.done());
    EXPECT_EQ(sync_wait(stl::move(h)), 123);
}

TEST(SpawnTest, SyncWaitSpawnHandle_PendingTask_DrivesExecutor) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto h   = spawn(ex, sleep_then_return(ex, std::chrono::milliseconds(50), 8));
    EXPECT_FALSE(h.done());
    auto start = std::chrono::steady_clock::now();
    int  v     = sync_wait(stl::move(h));
    auto dt    = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(v, 8);
    EXPECT_GE(dt, std::chrono::milliseconds(40));
    EXPECT_LT(dt, std::chrono::milliseconds(500));
}

TEST(SpawnTest, SyncWaitSpawnHandle_TaskThrows_Rethrows) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    auto h   = spawn(ex, throw_runtime());
    EXPECT_THROW(sync_wait(stl::move(h)), std::runtime_error);
}
