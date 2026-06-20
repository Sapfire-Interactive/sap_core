#include "sap_core/async/executor.h"
#include "sap_core/async/sleep_for.h"
#include "sap_core/async/spawn.h"
#include "sap_core/async/stop_token.h"
#include "sap_core/async/sync_wait.h"
#include "sap_core/async/task.h"

#include <gtest/gtest.h>

#include <chrono>

using sap::async::CancelledError;
using sap::async::Executor;
using sap::async::sleep_for;
using sap::async::spawn;
using sap::async::StopSource;
using sap::async::StopToken;
using sap::async::sync_wait;
using sap::async::Task;

namespace {

    Task<void> cancel_after(Executor& ex, std::chrono::milliseconds dt, StopSource& src) {
        co_await sleep_for(ex, dt);
        src.request_stop();
    }

    Task<int> inner_cancellable(Executor& ex, StopToken tok) {
        co_await sleep_for(ex, std::chrono::seconds(10), tok);
        co_return 42;
    }

    Task<int> outer_cancellable(Executor& ex, StopToken tok) {
        co_return co_await inner_cancellable(ex, stl::move(tok));
    }

    Task<int> sleep_uncancellable(Executor& ex, std::chrono::milliseconds dt, int v) {
        co_await sleep_for(ex, dt);
        co_return v;
    }

} // namespace

TEST(CancellationTest, StopSource_RequestStopFlipsFlag) {
    StopSource src;
    EXPECT_FALSE(src.stop_requested());
    EXPECT_TRUE(src.request_stop());
    EXPECT_TRUE(src.stop_requested());
}

TEST(CancellationTest, StopSource_RequestStopIdempotent) {
    StopSource src;
    EXPECT_TRUE(src.request_stop());
    EXPECT_FALSE(src.request_stop());
}

TEST(CancellationTest, StopToken_DefaultConstructed_NotStopPossible) {
    StopToken tok;
    EXPECT_FALSE(tok.stop_possible());
    EXPECT_FALSE(tok.stop_requested());
}

TEST(CancellationTest, StopToken_OutlivesSource_NoUseAfterFree) {
    StopToken tok;
    {
        StopSource src;
        tok = src.token();
        EXPECT_TRUE(tok.stop_possible());
    }
    EXPECT_TRUE(tok.stop_possible());
    EXPECT_FALSE(tok.stop_requested());
}

TEST(CancellationTest, StopCallback_FiresOnRequestStop) {
    StopSource src;
    sap::async::detail::stop_callback_node cb;
    bool fired = false;
    auto fn = +[](void* p) noexcept { *static_cast<bool*>(p) = true; };
    src.token()._arm(&cb, fn, &fired);
    EXPECT_FALSE(fired);
    src.request_stop();
    EXPECT_TRUE(fired);
}

TEST(CancellationTest, StopCallback_DestructorUnlinksBeforeFire) {
    StopSource src;
    bool fired = false;
    {
        sap::async::detail::stop_callback_node cb;
        auto fn = +[](void* p) noexcept { *static_cast<bool*>(p) = true; };
        src.token()._arm(&cb, fn, &fired);
    }
    src.request_stop();
    EXPECT_FALSE(fired);
}

TEST(CancellationTest, Cancellable_SleepFor_CancelledMidWait_ThrowsCancelled) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    StopSource src;

    auto h          = spawn(ex, sleep_for(ex, std::chrono::seconds(10), src.token()));
    auto controller = spawn(ex, cancel_after(ex, std::chrono::milliseconds(30), src));

    auto start = std::chrono::steady_clock::now();
    EXPECT_THROW(sync_wait(stl::move(h)), CancelledError);
    auto dt = std::chrono::steady_clock::now() - start;
    EXPECT_LT(dt, std::chrono::milliseconds(500));

    sync_wait(stl::move(controller));
}

TEST(CancellationTest, Cancellable_SleepFor_CancelAfterDone_NoEffect) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    StopSource src;

    auto h = spawn(ex, sleep_for(ex, std::chrono::milliseconds(20), src.token()));
    sync_wait(stl::move(h));
    EXPECT_TRUE(src.request_stop());
    EXPECT_TRUE(src.stop_requested());
}

TEST(CancellationTest, NonCancellable_SleepFor_CancelHasNoEffect) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    StopSource src;

    auto h          = spawn(ex, sleep_uncancellable(ex, std::chrono::milliseconds(80), 5));
    auto controller = spawn(ex, cancel_after(ex, std::chrono::milliseconds(10), src));

    auto start = std::chrono::steady_clock::now();
    int  v     = sync_wait(stl::move(h));
    auto dt    = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(v, 5);
    EXPECT_GE(dt, std::chrono::milliseconds(70));

    sync_wait(stl::move(controller));
}

TEST(CancellationTest, Cancellable_NestedTasks_PropagatesUp) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    StopSource src;

    auto h          = spawn(ex, outer_cancellable(ex, src.token()));
    auto controller = spawn(ex, cancel_after(ex, std::chrono::milliseconds(20), src));

    EXPECT_THROW(sync_wait(stl::move(h)), CancelledError);
    sync_wait(stl::move(controller));
}
