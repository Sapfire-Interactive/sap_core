#include "sap_core/async/executor.h"

#include "sap_core/io/event.h"
#include "sap_core/io/reactor.h"
#include "sap_core/stl/coroutine.h"
#include "sap_core/stl/result.h"
#include "sap_core/stl/span.h"
#include "sap_core/stl/utility.h"
#include "sap_core/types.h"

#include <chrono>

namespace sap::async {

    stl::result<Executor> Executor::create() {
        auto r = sap::io::Reactor::create();
        if (!r)
            return stl::make_error<Executor>("Reactor::create failed: {}", r.error());
        return stl::result<Executor>(stl::success, stl::move(r.value()));
    }

    void Executor::schedule(stl::coroutine_handle<> h) { m_ready.push_back(h); }

    void Executor::stop() { m_running = false; }

    void Executor::run() {
        m_running = true;

        sap::io::Trigger triggers[sap::io::Reactor::MAX_EVENTS_PER_WAIT];

        while (m_running) {
            while (!m_ready.empty() && m_running) {
                auto h = m_ready.front();
                m_ready.pop_front();
                h.resume();
            }

            if (!m_running)
                break;
            if (m_pending_io == 0 && m_ready.empty())
                break;

            auto n = m_reactor.wait(stl::span<sap::io::Trigger>(triggers, sap::io::Reactor::MAX_EVENTS_PER_WAIT),
                                    std::chrono::milliseconds(-1));
            if (!n) {
                m_running = false;
                break;
            }

            for (stl::size_t i = 0; i < n.value(); ++i) {
                auto* awaiter      = reinterpret_cast<IoAwaiter*>(triggers[i].user_token);
                awaiter->m_fired   = triggers[i].events;
                m_ready.push_back(awaiter->m_continuation);
            }
        }
    }

} // namespace sap::async
