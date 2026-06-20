#pragma once

#include "sap_core/async/task.h"
#include "sap_core/io/event.h"
#include "sap_core/io/reactor.h"
#include "sap_core/platform.h"
#include "sap_core/stl/coroutine.h"
#include "sap_core/stl/deque.h"
#include "sap_core/stl/result.h"
#include "sap_core/stl/utility.h"
#include "sap_core/types.h"

namespace sap::async {

    class Executor;

    // Suspends the awaiting coroutine until `handle` is ready for any of
    // `interest`. The IoAwaiter's address is the reactor token, so it must
    // live across suspension — i.e. as a local in the coroutine body, since
    // coroutine locals persist in the heap-allocated frame.
    class IoAwaiter {
    public:
        IoAwaiter(Executor& ex, sap::io::NativeHandle handle, sap::io::Event interest) noexcept
            : m_ex(ex), m_handle(handle), m_interest(interest) {}

        bool await_ready() const noexcept { return false; }

        void await_suspend(stl::coroutine_handle<> h);

        sap::io::Event await_resume();

    private:
        friend class Executor;

        Executor&               m_ex;
        sap::io::NativeHandle   m_handle;
        sap::io::Event          m_interest;
        sap::io::Event          m_fired{};
        stl::coroutine_handle<> m_continuation{};
    };

    // Single-threaded coroutine executor. All methods (schedule, run, stop,
    // spawn_detach) must be called from the same thread — typically from
    // coroutines the executor itself resumed. The only thread-safe surface
    // here is reactor().wake(), inherited from Reactor; use it if another
    // thread needs to nudge the loop.
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

        // Drives coroutines until both the ready queue and the pending-I/O set
        // are empty, or until stop() is called.
        void run();

        // Sets a flag; the run() loop exits on its next iteration.
        void stop();

        // Fire-and-forget. Runs the body up to first suspension synchronously;
        // any later resumption happens via the reactor. Not the same as
        // sap::async::spawn() (which returns an awaitable handle for the result).
        template <typename T>
        void spawn_detach(Task<T> task) {
            task.detach();
        }

        // Public so stl::result<Executor> can construct in-place. Prefer
        // calling create() — it does the Reactor setup for you.
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
        ++m_ex.m_pending_io;
        // If reactor.add fails the continuation never resumes via I/O; the
        // caller's await_resume hands back Event::None to signal "no fire."
        (void)m_ex.reactor().add(m_handle, m_interest, reinterpret_cast<u64>(this));
    }

    inline sap::io::Event IoAwaiter::await_resume() {
        (void)m_ex.reactor().remove(m_handle);
        --m_ex.m_pending_io;
        return m_fired;
    }

} // namespace sap::async
