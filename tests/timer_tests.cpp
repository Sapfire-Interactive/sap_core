#include "sap_core/timer.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

    using sap::core::Timer;
    using Mode = sap::core::Timer::Mode;
    using namespace std::chrono_literals;

    constexpr i64 ms_to_ns(i64 ms) { return ms * 1'000'000; }

    // Short cadence so periodic timers reach a target count quickly; the tests never
    // depend on this being precise, only on events eventually happening.
    constexpr i64 kTickNs = ms_to_ns(5);
    // "Far away" delay: long enough that, within a single test, the timer provably
    // cannot fire — used for non-occurrence and prompt-stop checks.
    constexpr i64 kFarNs = ms_to_ns(60'000);

    // Blocks until pred() is true or the timeout elapses. Returns pred()'s final value.
    // This replaces fixed sleeps: a test passes the instant the awaited event occurs and
    // only fails if it never occurs, which keeps it non-flaky under arbitrary load.
    template <typename Pred>
    bool wait_for_condition(Pred pred, std::chrono::milliseconds timeout = 5000ms) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (pred())
                return true;
            std::this_thread::sleep_for(1ms);
        }
        return pred();
    }

} // namespace

// =============================================================================
// Construction — the ctor stores config but does NOT launch the worker
// =============================================================================

TEST(Timer, ConstructDoesNotAutoStart) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, kTickNs, [&] { count.fetch_add(1); });
    // Never started: no worker exists, so nothing can ever increment the counter.
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(count.load(), 0);
}

TEST(Timer, DestroyWithoutStart) {
    Timer t(Mode::OneShot, kTickNs, [] {});
    // Destructs cleanly without ever starting a thread.
}

TEST(Timer, DestructorStopsRunningTimer) {
    std::atomic<int> count{0};
    {
        Timer t(Mode::Periodic, kTickNs, [&] { count.fetch_add(1); });
        EXPECT_TRUE(t.start().has_value());
        ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    }
    // The dtor joined the worker, so the count is final the moment the scope exits.
    const int after_destroy = count.load();
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(count.load(), after_destroy);
}

// =============================================================================
// OneShot
// =============================================================================

TEST(Timer, OneShotFiresExactlyOnce) {
    std::atomic<int> count{0};
    Timer t(Mode::OneShot, kTickNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());

    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    // OneShot breaks after firing and the worker exits, so a second fire is impossible
    // by construction — this equality is deterministic, not timing-dependent.
    EXPECT_EQ(count.load(), 1);
}

TEST(Timer, OneShotDoesNotFireBeforeDelay) {
    std::atomic<int> count{0};
    Timer t(Mode::OneShot, kFarNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    // Checked with effectively zero elapsed time against a 60 s delay: cannot have fired.
    EXPECT_EQ(count.load(), 0);
}

TEST(Timer, OneShotRestartRequiresStop) {
    std::atomic<int> count{0};
    Timer t(Mode::OneShot, kTickNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    EXPECT_EQ(count.load(), 1);

    // The finished worker is still joinable, so start() refuses until stop().
    EXPECT_TRUE(t.start().has_error());

    EXPECT_TRUE(t.stop().has_value());
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 2; }));
    EXPECT_EQ(count.load(), 2);
}

// =============================================================================
// Periodic
// =============================================================================

TEST(Timer, PeriodicFiresRepeatedly) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, kTickNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    EXPECT_TRUE(wait_for_condition([&] { return count.load() >= 5; }));
    t.stop();
}

TEST(Timer, PeriodicStopsFiringAfterStop) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, kTickNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 2; }));
    EXPECT_TRUE(t.stop().has_value());

    // stop() joined the worker, so the count cannot advance any further.
    const int frozen = count.load();
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(count.load(), frozen);
}

// =============================================================================
// stop()
// =============================================================================

TEST(Timer, StopBeforeStartIsNoop) {
    Timer t(Mode::Periodic, kTickNs, [] {});
    EXPECT_TRUE(t.stop().has_value());
}

TEST(Timer, StopIsIdempotent) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, kTickNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    EXPECT_TRUE(t.stop().has_value());
    EXPECT_TRUE(t.stop().has_value());
    EXPECT_TRUE(t.stop().has_value());
}

TEST(Timer, StopWakesImmediatelyDuringLongWait) {
    // The point of waiting on a condition_variable rather than sleeping: stop() returns
    // promptly even though the delay is a full minute away, and the callback never fires.
    std::atomic<int> count{0};
    Timer t(Mode::OneShot, kFarNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    std::this_thread::sleep_for(5ms); // let the worker enter its wait

    const auto t0 = std::chrono::steady_clock::now();
    EXPECT_TRUE(t.stop().has_value());
    const auto elapsed = std::chrono::steady_clock::now() - t0;

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 500);
    EXPECT_EQ(count.load(), 0);
}

// =============================================================================
// start() guards
// =============================================================================

TEST(Timer, StartReturnsSuccess) {
    Timer t(Mode::Periodic, kFarNs, [] {});
    EXPECT_TRUE(t.start().has_value());
    t.stop();
}

TEST(Timer, StartWhileRunningErrors) {
    Timer t(Mode::Periodic, kFarNs, [] {});
    EXPECT_TRUE(t.start().has_value());
    EXPECT_TRUE(t.start().has_error());
    t.stop();
}

TEST(Timer, StartStopStartRestarts) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, kTickNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    EXPECT_TRUE(t.stop().has_value());
    const int first_run = count.load();

    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() > first_run; }));
    t.stop();
}

// =============================================================================
// reset()
// =============================================================================

TEST(Timer, ResetChangesDelayWhileRunning) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, kFarNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    EXPECT_EQ(count.load(), 0); // 60 s delay: nothing yet

    EXPECT_TRUE(t.reset(kTickNs).has_value());
    EXPECT_TRUE(wait_for_condition([&] { return count.load() >= 5; })); // now firing fast
    t.stop();
}

TEST(Timer, ResetStartsAnIdleTimer) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, kFarNs, [&] { count.fetch_add(1); });
    // Never started; reset() = stop (noop) + new delay + start.
    EXPECT_TRUE(t.reset(kTickNs).has_value());
    EXPECT_TRUE(wait_for_condition([&] { return count.load() >= 5; }));
    t.stop();
}

TEST(Timer, ResetOneShotReArms) {
    std::atomic<int> count{0};
    Timer t(Mode::OneShot, kTickNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    EXPECT_EQ(count.load(), 1);

    EXPECT_TRUE(t.reset(kTickNs).has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 2; }));
    EXPECT_EQ(count.load(), 2);
}

// =============================================================================
// Move semantics — worker captures by value, so moving the Timer is safe and
// the running thread is never disturbed.
// =============================================================================

TEST(Timer, MoveConstructKeepsTimerRunning) {
    std::atomic<int> count{0};
    Timer a(Mode::Periodic, kTickNs, [&] { count.fetch_add(1); });
    EXPECT_TRUE(a.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));

    Timer b(std::move(a));
    const int at_move = count.load();
    EXPECT_TRUE(wait_for_condition([&] { return count.load() > at_move; })); // b keeps firing

    // a is moved-from and inert; tearing it down is safe.
    EXPECT_TRUE(a.stop().has_value());
    EXPECT_TRUE(b.stop().has_value());
}

TEST(Timer, MoveAssignStopsTargetsExistingTimer) {
    std::atomic<int> count_a{0};
    std::atomic<int> count_b{0};
    Timer a(Mode::Periodic, kTickNs, [&] { count_a.fetch_add(1); });
    Timer b(Mode::Periodic, kTickNs, [&] { count_b.fetch_add(1); });
    EXPECT_TRUE(a.start().has_value());
    EXPECT_TRUE(b.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count_a.load() >= 1 && count_b.load() >= 1; }));

    a = std::move(b); // a's original worker is stopped+joined; a adopts b's worker

    // The original a-worker was joined by the move-assign, so count_a is now frozen.
    const int a_frozen = count_a.load();
    EXPECT_TRUE(wait_for_condition([&] { return count_b.load() > a_frozen; })); // adopted worker fires
    EXPECT_EQ(count_a.load(), a_frozen); // original worker no longer fires

    EXPECT_TRUE(b.stop().has_value()); // b is moved-from / inert
    a.stop();
}

// =============================================================================
// Callback / threading
// =============================================================================

TEST(Timer, CallbackRunsOnBackgroundThread) {
    std::atomic<std::thread::id> worker_id{};
    const std::thread::id main_id = std::this_thread::get_id();

    Timer t(Mode::OneShot, kTickNs, [&] { worker_id.store(std::this_thread::get_id()); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return worker_id.load() != std::thread::id{}; }));

    EXPECT_NE(worker_id.load(), main_id); // ran on another thread
}

TEST(Timer, CallbackObservesCapturedState) {
    int captured = 0;
    std::atomic<bool> done{false};
    Timer t(Mode::OneShot, kTickNs, [&] {
        captured = 42;
        done.store(true);
    });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return done.load(); }));
    EXPECT_EQ(captured, 42);
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(Timer, ZeroDelayOneShotFiresOnce) {
    std::atomic<int> count{0};
    Timer t(Mode::OneShot, 0, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    EXPECT_EQ(count.load(), 1);
}

TEST(Timer, ZeroDelayPeriodicFiresAndStopsCleanly) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, 0, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    EXPECT_TRUE(t.stop().has_value());
    const int frozen = count.load();
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(count.load(), frozen);
}

TEST(Timer, NegativeDelayOneShotFiresImmediately) {
    std::atomic<int> count{0};
    Timer t(Mode::OneShot, -1000, [&] { count.fetch_add(1); });
    EXPECT_TRUE(t.start().has_value());
    ASSERT_TRUE(wait_for_condition([&] { return count.load() >= 1; }));
    EXPECT_EQ(count.load(), 1);
}

// =============================================================================
// Stress
// =============================================================================

TEST(Timer, RapidStartStopCycles) {
    std::atomic<int> count{0};
    Timer t(Mode::Periodic, kTickNs, [&] { count.fetch_add(1); });
    for (int i = 0; i < 30; ++i) {
        EXPECT_TRUE(t.start().has_value());
        EXPECT_TRUE(t.stop().has_value());
    }
    SUCCEED();
}

TEST(Timer, ManyConcurrentTimers) {
    constexpr int n = 16;
    std::vector<std::atomic<int>> counts(n);
    std::vector<std::unique_ptr<Timer>> timers;
    timers.reserve(n);

    for (int i = 0; i < n; ++i) {
        timers.push_back(std::make_unique<Timer>(Mode::Periodic, kTickNs, [&counts, i] { counts[i].fetch_add(1); }));
        EXPECT_TRUE(timers[i]->start().has_value());
    }
    EXPECT_TRUE(wait_for_condition([&] {
        for (int i = 0; i < n; ++i)
            if (counts[i].load() == 0)
                return false;
        return true;
    }));
    for (auto& t : timers)
        EXPECT_TRUE(t->stop().has_value());
}
