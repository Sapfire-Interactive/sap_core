#include "sap_core/job_system.h"
#include "sap_core/platform.h"
#include "sap_core/stl/utility.h"

namespace sap {

    job_system::job_system(const job_system_config& config) {
        m_queues.reserve(config.thread_count);
        m_threads.reserve(config.thread_count);

        for (u32 i = 0; i < config.thread_count; ++i)
            m_queues.push_back(stl::make_unique<job_queue>());

        for (u32 i = 0; i < config.thread_count; ++i) {
            m_threads.emplace_back([this, i](std::stop_token stop) {
                worker_main(i, stop);
            });
        }
    }

    job_system::~job_system() {
        for (auto& t : m_threads)
            t.request_stop();
        // Workers are spinning — they will see the stop request
        // and exit. jthread destructor joins automatically.
    }

    bool job_system::submit(stl::function<void()> job) {
        const u32 count = static_cast<u32>(m_queues.size());
        const u32 start = m_next_queue.fetch_add(1, std::memory_order_relaxed) % count;

        // Try round-robin across all queues to find one that isn't full.
        for (u32 i = 0; i < count; ++i) {
            if (m_queues[(start + i) % count]->try_push(stl::move(job))) {
                m_pending_jobs.fetch_add(1, std::memory_order_release);
                return true;
            }
        }

        return false;
    }

    void job_system::wait_idle() {
        while (m_pending_jobs.load(std::memory_order_acquire) != 0)
            cpu_pause();
    }

    u32 job_system::thread_count() const {
        return static_cast<u32>(m_threads.size());
    }

    void job_system::worker_main(u32 index, std::stop_token stop) {
        auto& queue = *m_queues[index];

        while (!stop.stop_requested()) {
            stl::function<void()> job;

            if (queue.try_pop(job)) {
                job();
                m_pending_jobs.fetch_sub(1, std::memory_order_release);
            } else {
                cpu_pause();
            }
        }

        // Drain remaining jobs before exiting.
        stl::function<void()> job;
        while (queue.try_pop(job)) {
            job();
            m_pending_jobs.fetch_sub(1, std::memory_order_release);
        }
    }

} // namespace sap
