#pragma once

#include "sap_core/async/stop_token.h"
#include "sap_core/async/task.h"
#include "sap_core/io/event.h"
#include "sap_core/io/reactor.h"
#include "sap_core/platform.h"
#include "sap_core/stl/coroutine.h"
#include "sap_core/stl/deque.h"
#include "sap_core/stl/fixed_string.h"
#include "sap_core/stl/result.h"
#include "sap_core/stl/utility.h"
#include "sap_core/types.h"

#include <exception>

namespace sap::async {

    class Executor;

    using ReactorErrorMessage = stl::fixed_string<127>;

    class ReactorError : public std::exception {
    public:
        explicit ReactorError(ReactorErrorMessage msg) noexcept : m_msg(stl::move(msg)) {}
        const char* what() const noexcept override { return m_msg.c_str(); }

    private:
        ReactorErrorMessage m_msg;
    };

    // The awaiter's address is the reactor token, so the awaiter must live across
    // the suspend point — i.e. as a local in the coroutine body (coroutine frame
    // is heap-allocated, locals are pinned there).
    class IoAwaiter {
    public:
        IoAwaiter(Executor& ex, sap::io::NativeHandle handle, sap::io::Event interest) noexcept
            : m_ex(ex), m_handle(handle), m_interest(interest) {}

        IoAwaiter(Executor& ex, sap::io::NativeHandle handle, sap::io::Event interest, StopToken tok) noexcept
            : m_ex(ex), m_handle(handle), m_interest(interest), m_token(stl::move(tok)) {}

        IoAwaiter(const IoAwaiter&)            = delete;
        IoAwaiter& operator=(const IoAwaiter&) = delete;
        IoAwaiter(IoAwaiter&&)                 = delete;
        IoAwaiter& operator=(IoAwaiter&&)      = delete;

        bool await_ready() const noexcept { return m_token.stop_possible() && m_token.stop_requested(); }

        void await_suspend(stl::coroutine_handle<> h);

        sap::io::Event await_resume();

    private:
        friend class Executor;

        static void on_cancel(void* arg) noexcept;

        Executor&                  m_ex;
        sap::io::NativeHandle      m_handle;
        sap::io::Event             m_interest;
        sap::io::Event             m_fired{};
        stl::coroutine_handle<>    m_continuation{};
        StopToken                  m_token;
        detail::stop_callback_node m_cb_node{};
        ReactorErrorMessage        m_add_error;
        bool                       m_cancelled  = false;
        bool                       m_queued     = false;
        bool                       m_registered = false;
    };

    // Single-threaded by contract. All methods (schedule, run, run_until, stop,
    // spawn_detach) must be called from the same thread. reactor().wake() is the
    // only thread-safe surface.
    class SAP_CORE_API Executor {
    public:
        static stl::result<Executor> create();

        ~Executor() = default;
        Executor(const Executor&)            = delete;
        Executor& operator=(const Executor&) = delete;
        Executor(Executor&&)                 = delete;
        Executor& operator=(Executor&&)      = delete;

        sap::io::Reactor& reactor() noexcept { return m_reactor; }

        void schedule(stl::coroutine_handle<> h);

        void run();

        // Drives the loop until target.done() (empty target = no target check).
        // Also exits on stop() or when ready queue and pending I/O are both empty.
        // Non-reentrant: do not call from a coroutine the executor is resuming.
        void run_until(stl::coroutine_handle<> target);

        void stop();

        template <typename T>
        void spawn_detach(Task<T> task) {
            task.detach();
        }

        explicit Executor(sap::io::Reactor r) noexcept : m_reactor(stl::move(r)) {}

    private:
        friend class IoAwaiter;

        sap::io::Reactor                    m_reactor;
        stl::deque<stl::coroutine_handle<>> m_ready;
        stl::size_t                         m_pending_io = 0;
        bool                                m_running    = false;
    };

    inline void IoAwaiter::await_suspend(stl::coroutine_handle<> h) {
        m_continuation = h;
        if (auto r = m_ex.reactor().add(m_handle, m_interest, reinterpret_cast<u64>(this)); !r) {
            const auto& err = r.error();
            stl::size_t len = err.size() > 127 ? 127 : err.size();
            m_add_error     = ReactorErrorMessage(err.data(), len);
            m_queued        = true;
            m_ex.m_ready.push_back(h);
            return;
        }
        m_registered = true;
        ++m_ex.m_pending_io;
        if (m_token.stop_possible())
            m_token._arm(&m_cb_node, &IoAwaiter::on_cancel, this);
    }

    inline sap::io::Event IoAwaiter::await_resume() {
        bool cancelled = m_cancelled || (m_token.stop_possible() && m_token.stop_requested());
        if (m_registered) {
            (void)m_ex.reactor().remove(m_handle);
            m_registered = false;
            --m_ex.m_pending_io;
        }
        if (!m_add_error.empty())
            throw ReactorError(stl::move(m_add_error));
        if (cancelled)
            throw CancelledError{};
        return m_fired;
    }

    inline void IoAwaiter::on_cancel(void* arg) noexcept {
        auto* self = static_cast<IoAwaiter*>(arg);
        if (self->m_cancelled)
            return;
        self->m_cancelled = true;
        if (self->m_registered) {
            (void)self->m_ex.reactor().remove(self->m_handle);
            self->m_registered = false;
            --self->m_ex.m_pending_io;
        }
        // Trigger path may have already queued the continuation; don't double-push.
        if (!self->m_queued && self->m_continuation) {
            self->m_queued = true;
            self->m_ex.m_ready.push_back(self->m_continuation);
        }
    }

} // namespace sap::async
