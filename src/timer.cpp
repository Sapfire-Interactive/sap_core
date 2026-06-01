#include "sap_core/timer.h"

#include <chrono>
#include <condition_variable>

namespace {
    using sap::core::Timer;

    void run_loop(Timer::Mode mode, i64 delay_ns, const stl::function<void()>& callback, stl::stop_token token) {
        stl::mutex m;
        stl::condition_variable_any cv;
        stl::unique_lock<stl::mutex> lock(m);
        do {
            if (cv.wait_for(lock, token, std::chrono::nanoseconds(delay_ns), [&] { return token.stop_requested(); }))
                return;
            callback();
        } while (mode == Timer::Mode::Periodic);
    }
} // namespace

namespace sap::core {
    Timer::Timer(Mode mode, i64 delay_ns, stl::function<void()> callback)
        : m_delay_ns(delay_ns), m_callback(std::move(callback)), m_mode(mode) {}

    Timer::~Timer() {
        stop();
    }

    stl::result<> Timer::start() {
        if (m_thread.joinable()) {
            return stl::make_error<>("Timer::start - timer is already running");
        }
        m_thread = stl::jthread([mode = m_mode, delay = m_delay_ns, cb = m_callback](stl::stop_token token) {
            run_loop(mode, delay, cb, token);
        });
        return stl::success;
    }

    stl::result<> Timer::stop() {
        if (m_thread.joinable()) {
            m_thread.request_stop();
            m_thread.join();
        }
        return stl::success;
    }

    stl::result<> Timer::reset(i64 new_delay_ns) {
        stop();
        m_delay_ns = new_delay_ns;
        return start();
    }
} // namespace sap::core