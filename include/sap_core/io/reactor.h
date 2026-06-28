#pragma once

#include "sap_core/io/event.h"
#include "sap_core/platform.h"
#include "sap_core/stl/result.h"
#include "sap_core/stl/span.h"
#include "sap_core/stl/unordered_map.h"
#include "sap_core/types.h"

#include <chrono>
#include <cstdint>

namespace sap::io {

#ifdef _WIN32
    using NativeHandle = std::uintptr_t;
#else
    using NativeHandle = int;
#endif

    inline constexpr NativeHandle INVALID_NATIVE_HANDLE = static_cast<NativeHandle>(-1);

    struct Trigger {
        Event events;
        u64 user_token;
    };

    // Cross-platform readiness reactor. Sockets, pipes, eventfd, timerfd, etc.
    // Not for regular files — those don't poll cleanly; route through a
    // thread-pool offload behind a different abstraction.
    class SAP_CORE_API Reactor {
    public:
        // wait() returns at most this many triggers per call; loop if you want more.
        static constexpr stl::size_t MAX_EVENTS_PER_WAIT = 64;

        static stl::result<Reactor> create();

        ~Reactor();
        Reactor(Reactor&&) noexcept;
        Reactor& operator=(Reactor&&) noexcept;
        Reactor(const Reactor&) = delete;
        Reactor& operator=(const Reactor&) = delete;

        stl::result<> add(NativeHandle handle, Event interest, u64 token);
        stl::result<> modify(NativeHandle handle, Event interest, u64 token);
        stl::result<> remove(NativeHandle handle);

        // Negative timeout waits forever; zero polls.
        stl::result<stl::size_t> wait(stl::span<Trigger> out, std::chrono::milliseconds timeout);

        // Thread-safe; makes wait() return promptly.
        void wake();

    private:
        Reactor() noexcept = default;

#if defined(__linux__)
        int m_epoll_fd = -1;
        int m_wake_fd  = -1;
#elif defined(_WIN32)
        // Raw pointers, not unique_ptr: an incomplete value type would force the
        // map's destructor on every includer. Owned by the out-of-line members.
        struct AfdPollContext;
        void* m_iocp = nullptr; // HANDLE
        stl::unordered_map<NativeHandle, AfdPollContext*> m_contexts;

        static stl::result<> submit_poll(AfdPollContext& ctx);
        static void          cancel_poll(AfdPollContext& ctx) noexcept;
#else
#error "sap::io::Reactor only supports Linux and Windows"
#endif
    };

} // namespace sap::io
