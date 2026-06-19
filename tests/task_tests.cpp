#include "sap_core/async/sync_wait.h"
#include "sap_core/async/task.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using sap::async::sync_wait;
using sap::async::Task;

// -----------------------------------------------------------------------------
// Construction / lifetime
// -----------------------------------------------------------------------------

TEST(TaskTest, DefaultConstructedIsEmpty) {
    Task<int> t;
    EXPECT_FALSE(static_cast<bool>(t));
    EXPECT_TRUE(t.done());
    EXPECT_TRUE(t.await_ready());
}

TEST(TaskTest, ConstructedFromHandleIsBool) {
    auto factory = []() -> Task<int> { co_return 1; };
    auto t = factory();
    EXPECT_TRUE(static_cast<bool>(t));
}

TEST(TaskTest, MoveConstructorTransfersOwnership) {
    auto factory = []() -> Task<int> { co_return 7; };
    auto t1 = factory();
    auto handle = t1.handle();
    Task<int> t2(std::move(t1));
    EXPECT_FALSE(static_cast<bool>(t1));
    EXPECT_TRUE(static_cast<bool>(t2));
    EXPECT_EQ(t2.handle(), handle);
}

TEST(TaskTest, MoveAssignmentDestroysExistingFrame) {
    auto factory_a = []() -> Task<int> { co_return 1; };
    auto factory_b = []() -> Task<int> { co_return 2; };
    auto a = factory_a();
    auto b = factory_b();
    auto b_handle = b.handle();
    a = std::move(b);
    EXPECT_EQ(a.handle(), b_handle);
    EXPECT_FALSE(static_cast<bool>(b));
}

TEST(TaskTest, SelfMoveAssignmentIsNoop) {
    auto factory = []() -> Task<int> { co_return 1; };
    auto t = factory();
    auto h = t.handle();
    // self move avoids destroying the handle
    auto& alias = t;
    t = std::move(alias);
    EXPECT_EQ(t.handle(), h);
}

TEST(TaskTest, DestructorOfUnstartedTaskDoesNotRunBody) {
    int side = 0;
    {
        auto factory = [&]() -> Task<int> {
            side = 42;
            co_return 0;
        };
        auto t = factory();
        // never awaited, never resumed
    }
    EXPECT_EQ(side, 0); // initial_suspend kept the body parked
}

// -----------------------------------------------------------------------------
// sync_wait: basic value semantics
// -----------------------------------------------------------------------------

TEST(SyncWaitTest, ReturnsLiteralInt) {
    auto factory = []() -> Task<int> { co_return 42; };
    EXPECT_EQ(sync_wait(factory()), 42);
}

TEST(SyncWaitTest, ReturnsStringValue) {
    auto factory = []() -> Task<std::string> { co_return std::string("hello"); };
    EXPECT_EQ(sync_wait(factory()), "hello");
}

TEST(SyncWaitTest, VoidTaskCompletes) {
    int side = 0;
    auto factory = [&]() -> Task<void> {
        side = 1;
        co_return;
    };
    sync_wait(factory());
    EXPECT_EQ(side, 1);
}

TEST(SyncWaitTest, MoveOnlyReturnType) {
    auto factory = []() -> Task<std::unique_ptr<int>> { co_return std::make_unique<int>(99); };
    auto r = sync_wait(factory());
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(*r, 99);
}

// -----------------------------------------------------------------------------
// Chained co_await
// -----------------------------------------------------------------------------

TEST(SyncWaitTest, AwaitsInnerTask) {
    auto inner = []() -> Task<int> { co_return 10; };
    auto outer = [&]() -> Task<int> {
        int v = co_await inner();
        co_return v + 5;
    };
    EXPECT_EQ(sync_wait(outer()), 15);
}

TEST(SyncWaitTest, DeepChainDoesNotOverflowStack) {
    // Symmetric transfer should make a deep chain run in O(1) stack.
    constexpr int depth = 5000;

    struct chain {
        static Task<int> run(int n) {
            if (n == 0)
                co_return 0;
            int sub = co_await run(n - 1);
            co_return sub + 1;
        }
    };

    EXPECT_EQ(sync_wait(chain::run(depth)), depth);
}

TEST(SyncWaitTest, AwaitMultipleSerial) {
    auto one = []() -> Task<int> { co_return 1; };
    auto two = []() -> Task<int> { co_return 2; };
    auto three = []() -> Task<int> { co_return 3; };
    auto outer = [&]() -> Task<int> {
        int a = co_await one();
        int b = co_await two();
        int c = co_await three();
        co_return a + b + c;
    };
    EXPECT_EQ(sync_wait(outer()), 6);
}

TEST(SyncWaitTest, VoidChainsIntoValueReturn) {
    int side = 0;
    auto step = [&]() -> Task<void> {
        ++side;
        co_return;
    };
    auto outer = [&]() -> Task<int> {
        co_await step();
        co_await step();
        co_await step();
        co_return side * 10;
    };
    EXPECT_EQ(sync_wait(outer()), 30);
}

// -----------------------------------------------------------------------------
// Exception propagation
// -----------------------------------------------------------------------------

TEST(SyncWaitTest, RethrowsExceptionFromTask) {
    auto failing = []() -> Task<int> {
        throw std::runtime_error("boom");
        co_return 0;
    };
    EXPECT_THROW(sync_wait(failing()), std::runtime_error);
}

TEST(SyncWaitTest, RethrowsExceptionFromVoidTask) {
    auto failing = []() -> Task<void> {
        throw std::logic_error("nope");
        co_return;
    };
    EXPECT_THROW(sync_wait(failing()), std::logic_error);
}

TEST(SyncWaitTest, ExceptionPropagatesThroughCoAwait) {
    auto inner = []() -> Task<int> {
        throw std::runtime_error("inner-fail");
        co_return 0;
    };
    auto outer = [&]() -> Task<int> {
        int v = co_await inner();
        co_return v;
    };
    EXPECT_THROW(sync_wait(outer()), std::runtime_error);
}

TEST(SyncWaitTest, ExceptionCanBeCaughtAcrossCoAwait) {
    auto inner = []() -> Task<int> {
        throw std::runtime_error("inner-fail");
        co_return 0;
    };
    auto outer = [&]() -> Task<int> {
        try {
            co_await inner();
        } catch (const std::runtime_error&) {
            co_return -1;
        }
        co_return 0;
    };
    EXPECT_EQ(sync_wait(outer()), -1);
}

TEST(SyncWaitTest, ExceptionInDeepChainPropagatesUp) {
    struct chain {
        static Task<int> run(int n) {
            if (n == 0) {
                throw std::runtime_error("depth-zero");
                co_return 0;
            }
            int sub = co_await run(n - 1);
            co_return sub + 1;
        }
    };
    EXPECT_THROW(sync_wait(chain::run(50)), std::runtime_error);
}

// -----------------------------------------------------------------------------
// detach()
// -----------------------------------------------------------------------------

TEST(DetachTest, RunsBodyToCompletionSynchronously) {
    int side = 0;
    auto factory = [&]() -> Task<int> {
        side = 123;
        co_return 0;
    };
    auto t = factory();
    EXPECT_EQ(side, 0);
    t.detach();
    EXPECT_EQ(side, 123);
    EXPECT_FALSE(static_cast<bool>(t));
}

TEST(DetachTest, VoidTaskRuns) {
    int side = 0;
    auto factory = [&]() -> Task<void> {
        side = 7;
        co_return;
    };
    factory().detach();
    EXPECT_EQ(side, 7);
}

TEST(DetachTest, NoUseAfterDetach) {
    auto factory = []() -> Task<int> { co_return 1; };
    auto t = factory();
    t.detach();
    EXPECT_TRUE(t.done());
    EXPECT_TRUE(t.await_ready());
    EXPECT_FALSE(static_cast<bool>(t));
}

TEST(DetachTest, DetachOnEmptyIsNoop) {
    Task<int> t;
    t.detach();
    SUCCEED();
}

TEST(DetachTest, ExceptionInDetachedBodyIsSwallowedSilently) {
    // A detached task has nowhere to surface the exception. The frame stores it
    // and self-destroys; we just need to verify the program doesn't crash.
    auto factory = []() -> Task<int> {
        throw std::runtime_error("detached-fail");
        co_return 0;
    };
    factory().detach();
    SUCCEED();
}

// -----------------------------------------------------------------------------
// done() / await_ready()
// -----------------------------------------------------------------------------

TEST(TaskTest, DoneFalseBeforeStart) {
    auto factory = []() -> Task<int> { co_return 1; };
    auto t = factory();
    EXPECT_FALSE(t.done());
    EXPECT_FALSE(t.await_ready());
}

TEST(TaskTest, AwaitReadyTrueAfterDoneTask) {
    auto factory = []() -> Task<int> { co_return 1; };
    auto t = factory();
    sync_wait(std::move(t));
    // t now has an empty handle (moved into sync_wait runner) — verify our
    // moved-from invariant: empty handle reports as done/ready.
    EXPECT_TRUE(t.done());
    EXPECT_TRUE(t.await_ready());
}

// -----------------------------------------------------------------------------
// Composition / multiple tasks in flight
// -----------------------------------------------------------------------------

TEST(SyncWaitTest, NestedComposition) {
    auto leaf = [](int x) -> Task<int> { co_return x * x; };
    auto mid = [&](int x) -> Task<int> {
        int a = co_await leaf(x);
        int b = co_await leaf(x + 1);
        co_return a + b;
    };
    auto top = [&]() -> Task<int> {
        int v = co_await mid(3);
        co_return v;
    };
    EXPECT_EQ(sync_wait(top()), 9 + 16);
}

TEST(SyncWaitTest, ReturnsLargeStructByMove) {
    struct big {
        std::vector<int> data;
    };
    auto factory = []() -> Task<big> {
        big b;
        b.data.resize(1000);
        for (int i = 0; i < 1000; ++i)
            b.data[i] = i;
        co_return b;
    };
    auto r = sync_wait(factory());
    ASSERT_EQ(r.data.size(), 1000u);
    EXPECT_EQ(r.data[999], 999);
}
