#include "sap_core/job_system.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <numeric>
#include <set>
#include <thread>
#include <vector>

// =============================================================================
// Construction / Destruction
// =============================================================================

TEST(JobSystem, DefaultConstruct) {
    sap::job_system js;
    EXPECT_GT(js.thread_count(), 0u);
}

TEST(JobSystem, ExplicitThreadCount) {
    sap::job_system js({.thread_count = 2});
    EXPECT_EQ(js.thread_count(), 2u);
}

TEST(JobSystem, SingleThread) {
    sap::job_system js({.thread_count = 1});
    EXPECT_EQ(js.thread_count(), 1u);
}

TEST(JobSystem, DestroyWithoutSubmit) {
    sap::job_system js({.thread_count = 4});
    // Should destruct cleanly without any work submitted
}

// =============================================================================
// Basic submit / execution
// =============================================================================

TEST(JobSystem, SubmitSingleJob) {
    sap::job_system js({.thread_count = 2});
    std::atomic<int> counter{0};

    EXPECT_TRUE(js.submit([&] { counter.fetch_add(1); }));
    js.wait_idle();

    EXPECT_EQ(counter.load(), 1);
}

TEST(JobSystem, SubmitMultipleJobs) {
    sap::job_system js({.thread_count = 4});
    constexpr int n = 1000;
    std::atomic<int> counter{0};

    for (int i = 0; i < n; ++i)
        EXPECT_TRUE(js.submit([&] { counter.fetch_add(1); }));
    js.wait_idle();

    EXPECT_EQ(counter.load(), n);
}

TEST(JobSystem, SubmitWithCapture) {
    sap::job_system js({.thread_count = 2});
    std::atomic<int> sum{0};

    for (int i = 1; i <= 100; ++i) {
        js.submit([&sum, i] { sum.fetch_add(i); });
    }
    js.wait_idle();

    EXPECT_EQ(sum.load(), 5050);
}

// =============================================================================
// wait_idle
// =============================================================================

TEST(JobSystem, WaitIdleNoJobs) {
    sap::job_system js({.thread_count = 2});
    js.wait_idle(); // should return immediately
}

TEST(JobSystem, WaitIdleMultipleTimes) {
    sap::job_system js({.thread_count = 2});
    std::atomic<int> counter{0};

    for (int i = 0; i < 50; ++i)
        js.submit([&] { counter.fetch_add(1); });
    js.wait_idle();
    EXPECT_EQ(counter.load(), 50);

    for (int i = 0; i < 50; ++i)
        js.submit([&] { counter.fetch_add(1); });
    js.wait_idle();
    EXPECT_EQ(counter.load(), 100);
}

TEST(JobSystem, WaitIdleEnsuresCompletion) {
    sap::job_system js({.thread_count = 2});
    std::atomic<bool> done{false};

    js.submit([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        done.store(true);
    });
    js.wait_idle();

    EXPECT_TRUE(done.load());
}

// =============================================================================
// Concurrency correctness
// =============================================================================

TEST(JobSystem, NoDataRace) {
    sap::job_system js({.thread_count = 8});
    constexpr int n = 10000;
    std::atomic<int> counter{0};

    for (int i = 0; i < n; ++i)
        js.submit([&] { counter.fetch_add(1, std::memory_order_relaxed); });
    js.wait_idle();

    EXPECT_EQ(counter.load(), n);
}

TEST(JobSystem, MultipleThreadsExecute) {
    sap::job_system js({.thread_count = 4});
    std::set<std::thread::id> thread_ids;
    stl::mutex mtx;

    for (int i = 0; i < 100; ++i) {
        js.submit([&] {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            stl::lock_guard<stl::mutex> lock(mtx);
            thread_ids.insert(std::this_thread::get_id());
        });
    }
    js.wait_idle();

    // With 4 threads and 100 jobs with sleeps, we expect multiple threads to participate
    EXPECT_GT(thread_ids.size(), 1u);
}

TEST(JobSystem, JobsExecuteInParallel) {
    sap::job_system js({.thread_count = 4});
    std::atomic<int> concurrent{0};
    std::atomic<int> max_concurrent{0};

    for (int i = 0; i < 20; ++i) {
        js.submit([&] {
            int c = concurrent.fetch_add(1, std::memory_order_relaxed) + 1;
            int prev = max_concurrent.load(std::memory_order_relaxed);
            while (c > prev && !max_concurrent.compare_exchange_weak(prev, c, std::memory_order_relaxed)) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            concurrent.fetch_sub(1, std::memory_order_relaxed);
        });
    }
    js.wait_idle();

    EXPECT_GT(max_concurrent.load(), 1);
}

// =============================================================================
// Stress tests
// =============================================================================

TEST(JobSystem, HighVolume) {
    sap::job_system js({.thread_count = 4});
    constexpr int n = 100000;
    std::atomic<i64> sum{0};

    for (int i = 0; i < n; ++i)
        js.submit([&sum, i] { sum.fetch_add(i, std::memory_order_relaxed); });
    js.wait_idle();

    i64 expected = static_cast<i64>(n) * (n - 1) / 2;
    EXPECT_EQ(sum.load(), expected);
}

TEST(JobSystem, RapidSubmitWait) {
    sap::job_system js({.thread_count = 2});
    std::atomic<int> counter{0};

    for (int round = 0; round < 100; ++round) {
        for (int i = 0; i < 10; ++i)
            js.submit([&] { counter.fetch_add(1); });
        js.wait_idle();
    }

    EXPECT_EQ(counter.load(), 1000);
}

// =============================================================================
// Edge cases
// =============================================================================

TEST(JobSystem, EmptyJob) {
    sap::job_system js({.thread_count = 2});
    js.submit([] {}); // noop
    js.wait_idle();
}

TEST(JobSystem, JobSubmitsAreNonBlocking) {
    sap::job_system js({.thread_count = 2});
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        js.submit([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    auto submit_time = std::chrono::steady_clock::now() - start;
    // Submitting should be near-instant, not blocked by execution
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(submit_time).count(), 100);
    js.wait_idle();
}

TEST(JobSystem, DestroyDrainsRemainingJobs) {
    std::atomic<int> counter{0};
    {
        sap::job_system js({.thread_count = 2});
        for (int i = 0; i < 100; ++i)
            js.submit([&] { counter.fetch_add(1); });
        // Destructor should drain remaining jobs
    }
    EXPECT_EQ(counter.load(), 100);
}

// =============================================================================
// submit return value
// =============================================================================

TEST(JobSystem, SubmitReturnsTrueNormally) {
    sap::job_system js({.thread_count = 2});
    bool ok = js.submit([] {});
    EXPECT_TRUE(ok);
    js.wait_idle();
}
