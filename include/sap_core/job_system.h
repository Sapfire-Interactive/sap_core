#pragma once

#include "sap_core/stl/spsc_queue.h"
#include "sap_core/stl/unique_ptr.h"
#include "sap_core/stl/vector.h"
#include "sap_core/types.h"

namespace sap {

    struct job_system_config {
        u32 thread_count = stl::thread::hardware_concurrency();
    };

    class job_system {
    public:
        job_system(const job_system_config& config = {});
        ~job_system();

        job_system(const job_system&) = delete;
        job_system& operator=(const job_system&) = delete;

        // Submit a job to be executed by a worker thread.
        // Returns false if the target queue is full.
        bool submit(stl::function<void()> job);

        // Block until all submitted jobs have completed.
        void wait_idle();

        // Number of worker threads.
        u32 thread_count() const;

    private:
        static constexpr stl::size_t queue_capacity = 4096;
        using job_queue = stl::spsc_queue<stl::function<void()>, queue_capacity>;

        void worker_main(u32 index, std::stop_token stop);

        stl::vector<stl::unique_ptr<job_queue>> m_queues;
        stl::atomic<u32> m_next_queue{0};
        stl::atomic<u32> m_pending_jobs{0};
        // Must be last: destroyed first so workers join before queues/atomics are torn down.
        stl::vector<stl::jthread> m_threads;
    };

} // namespace sap
