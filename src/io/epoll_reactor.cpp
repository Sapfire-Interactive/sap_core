#include "sap_core/io/reactor.h"

#include "sap_core/stl/result.h"
#include "sap_core/types.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <utility>

namespace sap::io {

    namespace {
        // Sentinel token for the wake eventfd. Filtered out of wait() output.
        // Users must not pass this value as their own token.
        constexpr u64 WAKE_TOKEN = static_cast<u64>(-1);

        u32 to_epoll(Event e) noexcept {
            u32 ev = 0;
            if (has(e, Event::Readable))
                ev |= EPOLLIN;
            if (has(e, Event::Writable))
                ev |= EPOLLOUT;
            // EPOLLERR and EPOLLHUP are reported automatically; setting them
            // as interest is a no-op.
            return ev;
        }

        Event from_epoll(u32 ev) noexcept {
            Event e = Event::None;
            if (ev & EPOLLIN)
                e |= Event::Readable;
            if (ev & EPOLLOUT)
                e |= Event::Writable;
            if (ev & EPOLLERR)
                e |= Event::Error;
            if (ev & EPOLLHUP)
                e |= Event::HangUp;
            return e;
        }
    } // namespace

    stl::result<Reactor> Reactor::create() {
        int ep = ::epoll_create1(EPOLL_CLOEXEC);
        if (ep < 0)
            return stl::make_error<Reactor>("epoll_create1 failed: {}", std::strerror(errno));

        int wfd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (wfd < 0) {
            int saved = errno;
            ::close(ep);
            return stl::make_error<Reactor>("eventfd failed: {}", std::strerror(saved));
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.u64 = WAKE_TOKEN;
        if (::epoll_ctl(ep, EPOLL_CTL_ADD, wfd, &ev) < 0) {
            int saved = errno;
            ::close(wfd);
            ::close(ep);
            return stl::make_error<Reactor>("epoll_ctl(wake_fd) failed: {}", std::strerror(saved));
        }

        Reactor r;
        r.m_epoll_fd = ep;
        r.m_wake_fd  = wfd;
        return stl::result<Reactor>(stl::success, std::move(r));
    }

    Reactor::~Reactor() {
        if (m_wake_fd >= 0)
            ::close(m_wake_fd);
        if (m_epoll_fd >= 0)
            ::close(m_epoll_fd);
    }

    Reactor::Reactor(Reactor&& o) noexcept : m_epoll_fd(o.m_epoll_fd), m_wake_fd(o.m_wake_fd) {
        o.m_epoll_fd = -1;
        o.m_wake_fd  = -1;
    }

    Reactor& Reactor::operator=(Reactor&& o) noexcept {
        if (this != &o) {
            if (m_wake_fd >= 0)
                ::close(m_wake_fd);
            if (m_epoll_fd >= 0)
                ::close(m_epoll_fd);
            m_epoll_fd   = o.m_epoll_fd;
            m_wake_fd    = o.m_wake_fd;
            o.m_epoll_fd = -1;
            o.m_wake_fd  = -1;
        }
        return *this;
    }

    stl::result<> Reactor::add(NativeHandle handle, Event interest, u64 token) {
        epoll_event ev{};
        ev.events   = to_epoll(interest);
        ev.data.u64 = token;
        if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, handle, &ev) < 0)
            return stl::make_error<>("epoll_ctl(ADD) failed: {}", std::strerror(errno));
        return stl::result_success();
    }

    stl::result<> Reactor::modify(NativeHandle handle, Event interest, u64 token) {
        epoll_event ev{};
        ev.events   = to_epoll(interest);
        ev.data.u64 = token;
        if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, handle, &ev) < 0)
            return stl::make_error<>("epoll_ctl(MOD) failed: {}", std::strerror(errno));
        return stl::result_success();
    }

    stl::result<> Reactor::remove(NativeHandle handle) {
        if (::epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, handle, nullptr) < 0)
            return stl::make_error<>("epoll_ctl(DEL) failed: {}", std::strerror(errno));
        return stl::result_success();
    }

    stl::result<stl::size_t> Reactor::wait(stl::span<Trigger> out, std::chrono::milliseconds timeout) {
        if (out.empty())
            return stl::result<stl::size_t>(stl::success, stl::size_t{0});

        epoll_event events[MAX_EVENTS_PER_WAIT];
        const int max_n = static_cast<int>(out.size() < MAX_EVENTS_PER_WAIT ? out.size() : MAX_EVENTS_PER_WAIT);

        const int t_ms = (timeout < std::chrono::milliseconds::zero()) ? -1 : static_cast<int>(timeout.count());

        const int n = ::epoll_wait(m_epoll_fd, events, max_n, t_ms);
        if (n < 0) {
            // EINTR isn't an error; the caller can just call wait() again.
            if (errno == EINTR)
                return stl::result<stl::size_t>(stl::success, stl::size_t{0});
            return stl::make_error<stl::size_t>("epoll_wait failed: {}", std::strerror(errno));
        }

        stl::size_t written = 0;
        for (int i = 0; i < n; ++i) {
            if (events[i].data.u64 == WAKE_TOKEN) {
                // Drain the eventfd counter; otherwise the next wait() would
                // see it ready immediately and spin.
                u64 drain = 0;
                (void)::read(m_wake_fd, &drain, sizeof(drain));
                continue;
            }
            out[written].events     = from_epoll(events[i].events);
            out[written].user_token = events[i].data.u64;
            ++written;
        }
        return stl::result<stl::size_t>(stl::success, written);
    }

    void Reactor::wake() {
        if (m_wake_fd < 0)
            return;
        u64 one = 1;
        (void)::write(m_wake_fd, &one, sizeof(one));
    }

} // namespace sap::io
