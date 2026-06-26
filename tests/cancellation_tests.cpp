#include "sap_core/async/executor.h"
#include "sap_core/async/sleep_for.h"
#include "sap_core/async/spawn.h"
#include "sap_core/async/stop_token.h"
#include "sap_core/async/sync_wait.h"
#include "sap_core/async/task.h"

#include <gtest/gtest.h>

#include <chrono>

using sap::async::Executor;
using sap::async::sleep_for;
using sap::async::spawn;
using sap::async::StopSource;
using sap::async::StopToken;
using sap::async::sync_wait;
using sap::async::Task;

namespace {

    Task<stl::result<>> cancel_after(Executor& ex, std::chrono::milliseconds dt, StopSource& src) {
        if (auto r = co_await sleep_for(ex, dt); !r)
            co_return r;
        src.request_stop();
        co_return stl::success;
    }

    Task<stl::result<int>> inner_cancellable(Executor& ex, StopToken tok) {
        if (auto r = co_await sleep_for(ex, std::chrono::seconds(10), tok); !r)
            co_return stl::make_error<int>("{}", r.error());
        co_return stl::result<int>{stl::success, 42};
    }

    Task<stl::result<int>> outer_cancellable(Executor& ex, StopToken tok) {
        co_return co_await inner_cancellable(ex, stl::move(tok));
    }

    Task<stl::result<int>> sleep_uncancellable(Executor& ex, std::chrono::milliseconds dt, int v) {
        if (auto r = co_await sleep_for(ex, dt); !r)
            co_return stl::make_error<int>("{}", r.error());
        co_return stl::result<int>{stl::success, v};
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

TEST(CancellationTest, Cancellable_SleepFor_CancelledMidWait_ReturnsError) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    StopSource src;

    auto h          = spawn(ex, sleep_for(ex, std::chrono::seconds(10), src.token()));
    auto controller = spawn(ex, cancel_after(ex, std::chrono::milliseconds(30), src));

    auto start = std::chrono::steady_clock::now();
    auto r = sync_wait(stl::move(h));
    auto dt = std::chrono::steady_clock::now() - start;
    EXPECT_FALSE(r);
    EXPECT_LT(dt, std::chrono::milliseconds(500));

    auto cr = sync_wait(stl::move(controller));
    EXPECT_TRUE(cr) << (cr ? "" : cr.error().c_str());
}

TEST(CancellationTest, Cancellable_SleepFor_CancelAfterDone_NoEffect) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    StopSource src;

    auto h = spawn(ex, sleep_for(ex, std::chrono::milliseconds(20), src.token()));
    auto r = sync_wait(stl::move(h));
    EXPECT_TRUE(r) << (r ? "" : r.error().c_str());
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
    auto r = sync_wait(stl::move(h));
    auto dt = std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(r) << (r ? "" : r.error().c_str());
    EXPECT_EQ(*r, 5);
    EXPECT_GE(dt, std::chrono::milliseconds(70));

    auto cr = sync_wait(stl::move(controller));
    EXPECT_TRUE(cr) << (cr ? "" : cr.error().c_str());
}

TEST(CancellationTest, Cancellable_NestedTasks_PropagatesUp) {
    auto exr = Executor::create();
    auto& ex = exr.value();
    StopSource src;

    auto h          = spawn(ex, outer_cancellable(ex, src.token()));
    auto controller = spawn(ex, cancel_after(ex, std::chrono::milliseconds(20), src));

    auto r = sync_wait(stl::move(h));
    EXPECT_FALSE(r);
    auto cr = sync_wait(stl::move(controller));
    EXPECT_TRUE(cr) << (cr ? "" : cr.error().c_str());
}
