#include "sap_core/async/sync_wait.h"
#include "sap_core/async/task.h"
#include "sap_core/async/when_all.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

using sap::async::sync_wait;
using sap::async::Task;
using sap::async::when_all;

TEST(WhenAllTest, TwoIntTasks) {
    auto t1 = []() -> Task<int> { co_return 1; };
    auto t2 = []() -> Task<int> { co_return 2; };
    auto r = sync_wait(when_all(t1(), t2()));
    EXPECT_EQ(std::get<0>(r), 1);
    EXPECT_EQ(std::get<1>(r), 2);
}

TEST(WhenAllTest, ThreeMixedTypeTasks) {
    auto ti = []() -> Task<int> { co_return 42; };
    auto ts = []() -> Task<std::string> { co_return std::string("hello"); };
    auto td = []() -> Task<double> { co_return 3.14; };
    auto r = sync_wait(when_all(ti(), ts(), td()));
    EXPECT_EQ(std::get<0>(r), 42);
    EXPECT_EQ(std::get<1>(r), "hello");
    EXPECT_DOUBLE_EQ(std::get<2>(r), 3.14);
}

TEST(WhenAllTest, SingleTask) {
    auto t = []() -> Task<int> { co_return 99; };
    auto r = sync_wait(when_all(t()));
    EXPECT_EQ(std::get<0>(r), 99);
}

TEST(WhenAllTest, RunsTasksInArgumentOrder) {
    auto counter = std::make_shared<std::atomic<int>>(0);
    auto mark = [counter](int& slot) -> Task<int> {
        slot = counter->fetch_add(1) + 1;
        co_return slot;
    };
    int a = 0, b = 0, c = 0;
    sync_wait(when_all(mark(a), mark(b), mark(c)));
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

TEST(WhenAllTest, ExceptionInFirstTaskPropagates) {
    auto bad = []() -> Task<int> {
        throw std::runtime_error("first-boom");
        co_return 0;
    };
    auto fine = []() -> Task<int> { co_return 42; };
    EXPECT_THROW(sync_wait(when_all(bad(), fine())), std::runtime_error);
}

TEST(WhenAllTest, ExceptionInLaterTaskPropagates) {
    auto fine = []() -> Task<int> { co_return 1; };
    auto bad = []() -> Task<int> {
        throw std::runtime_error("second-boom");
        co_return 0;
    };
    EXPECT_THROW(sync_wait(when_all(fine(), bad())), std::runtime_error);
}

TEST(WhenAllTest, ExceptionStopsRemainingTasksFromRunning) {
    auto bad = []() -> Task<int> {
        throw std::runtime_error("boom");
        co_return 0;
    };
    auto side = std::make_shared<std::atomic<int>>(0);
    auto with_side = [side]() -> Task<int> {
        side->fetch_add(1);
        co_return 0;
    };
    EXPECT_THROW(sync_wait(when_all(bad(), with_side())), std::runtime_error);
    EXPECT_EQ(side->load(), 0);
}

TEST(WhenAllTest, MoveOnlyReturnType) {
    auto t1 = []() -> Task<std::unique_ptr<int>> { co_return std::make_unique<int>(7); };
    auto t2 = []() -> Task<std::unique_ptr<int>> { co_return std::make_unique<int>(8); };
    auto r = sync_wait(when_all(t1(), t2()));
    EXPECT_EQ(*std::get<0>(r), 7);
    EXPECT_EQ(*std::get<1>(r), 8);
}

TEST(WhenAllTest, Nested) {
    auto leaf = [](int v) -> Task<int> { co_return v; };
    auto r = sync_wait(when_all(when_all(leaf(1), leaf(2)), leaf(3)));
    auto inner = std::get<0>(r);
    EXPECT_EQ(std::get<0>(inner), 1);
    EXPECT_EQ(std::get<1>(inner), 2);
    EXPECT_EQ(std::get<1>(r), 3);
}

TEST(WhenAllTest, EightTasks) {
    auto t = [](int v) -> Task<int> { co_return v; };
    auto r = sync_wait(when_all(t(1), t(2), t(3), t(4), t(5), t(6), t(7), t(8)));
    EXPECT_EQ(std::get<0>(r), 1);
    EXPECT_EQ(std::get<7>(r), 8);
}
