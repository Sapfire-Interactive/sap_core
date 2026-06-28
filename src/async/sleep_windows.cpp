#include "sap_core/async/sleep_for.h"

#include "sap_core/async/executor.h"
#include "sap_core/async/stop_token.h"
#include "sap_core/async/task.h"
#include "sap_core/io/event.h"
#include "sap_core/stl/utility.h"
#include "sap_core/types.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <chrono>

// The reactor only polls sockets (AFD), so we can't register a waitable-timer
// HANDLE. Instead we wait on a loopback UDP socket that a threadpool timer
// signals when the delay elapses — the Windows analog of the Linux timerfd.
// Winsock must be up via SocketPlatform::init(), like the rest of the io stack.

namespace sap::async {

    namespace {

        struct socket_guard {
            SOCKET s = INVALID_SOCKET;
            socket_guard() = default;
            explicit socket_guard(SOCKET sock) noexcept : s(sock) {}
            socket_guard(const socket_guard&)            = delete;
            socket_guard& operator=(const socket_guard&) = delete;
            ~socket_guard() {
                if (s != INVALID_SOCKET)
                    ::closesocket(s);
            }
        };

        struct timer_guard {
            PTP_TIMER t = nullptr;
            timer_guard() = default;
            explicit timer_guard(PTP_TIMER timer) noexcept : t(timer) {}
            timer_guard(const timer_guard&)            = delete;
            timer_guard& operator=(const timer_guard&) = delete;
            ~timer_guard() {
                if (t) {
                    ::SetThreadpoolTimer(t, nullptr, 0, 0);
                    ::WaitForThreadpoolTimerCallbacks(t, TRUE);
                    ::CloseThreadpoolTimer(t);
                }
            }
        };

        void CALLBACK on_timer(PTP_CALLBACK_INSTANCE, void* ctx, PTP_TIMER) {
            SOCKET s    = *static_cast<SOCKET*>(ctx);
            char   byte = 1;
            ::send(s, &byte, 1, 0); // best-effort wake; the socket is drained on resume
        }

        stl::result<SOCKET> make_self_socket() {
            SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s == INVALID_SOCKET)
                return stl::make_error<SOCKET>("sleep_for: socket() failed: {}", ::WSAGetLastError());
            socket_guard guard{s};

            sockaddr_in addr{};
            addr.sin_family      = AF_INET;
            addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
            addr.sin_port        = 0;
            if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
                return stl::make_error<SOCKET>("sleep_for: bind() failed: {}", ::WSAGetLastError());

            int len = sizeof(addr);
            if (::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
                return stl::make_error<SOCKET>("sleep_for: getsockname() failed: {}", ::WSAGetLastError());
            if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
                return stl::make_error<SOCKET>("sleep_for: connect() failed: {}", ::WSAGetLastError());

            guard.s = INVALID_SOCKET; // hand ownership to the caller
            return s;
        }

    } // namespace

    Task<stl::result<>> sleep_for(Executor& ex, std::chrono::milliseconds dt, StopToken tok) {
        auto sock = make_self_socket();
        if (!sock)
            co_return stl::make_error<>("{}", sock.error());
        socket_guard sg{sock.value()};

        timer_guard tg{::CreateThreadpoolTimer(on_timer, &sg.s, nullptr)};
        if (!tg.t)
            co_return stl::make_error<>("sleep_for: CreateThreadpoolTimer failed: {}", ::GetLastError());

        // Negative due time is relative, measured in 100ns ticks.
        LONGLONG ticks = -(static_cast<LONGLONG>(dt.count()) * 10'000);
        if (ticks == 0)
            ticks = -1;
        FILETIME due{};
        due.dwLowDateTime  = static_cast<DWORD>(static_cast<ULONGLONG>(ticks) & 0xFFFFFFFF);
        due.dwHighDateTime = static_cast<DWORD>(static_cast<ULONGLONG>(ticks) >> 32);
        ::SetThreadpoolTimer(tg.t, &due, 0, 0);

        IoAwaiter awaiter(ex, static_cast<sap::io::NativeHandle>(sg.s), sap::io::Event::Readable, stl::move(tok));
        if (auto r = co_await awaiter; !r)
            co_return r;

        char byte = 0;
        ::recv(sg.s, &byte, 1, 0); // drain the wake byte
        co_return stl::success;
    }

} // namespace sap::async
