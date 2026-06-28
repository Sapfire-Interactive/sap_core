// Windows reactor backend: IOCP + AFD_POLL (libuv pattern).
//
// AFD ("Ancillary Function Driver") is the Windows kernel driver that backs
// Winsock. The IOCTL_AFD_POLL interface lets us submit a poll request for a
// socket via NtDeviceIoControlFile, with the completion delivered through an
// I/O completion port. This is the pattern libuv, asio, and Mozilla NSPR
// have used for ~15 years.
//
// AFD itself is not in any official Microsoft SDK header. The struct layouts
// and ioctl number below are reverse-engineered and stable across Windows
// versions but unofficial. **This file was written from a Linux dev box and
// has not been compiled or run on Windows.** Verify before relying.

#include "sap_core/io/reactor.h"

#include "sap_core/stl/result.h"
#include "sap_core/stl/unique_ptr.h"
#include "sap_core/stl/utility.h"
#include "sap_core/types.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <mswsock.h> // SIO_BASE_HANDLE
#include <windows.h>

#include <cstring>

namespace sap::io {

    namespace {

        // ---- AFD definitions (not in public headers) -----------------------

        constexpr ULONG kIoctlAfdPoll = 0x00012024;

        constexpr ULONG AFD_POLL_RECEIVE           = 0x0001;
        constexpr ULONG AFD_POLL_RECEIVE_EXPEDITED = 0x0002;
        constexpr ULONG AFD_POLL_SEND              = 0x0004;
        constexpr ULONG AFD_POLL_DISCONNECT        = 0x0008; // graceful peer close
        constexpr ULONG AFD_POLL_ABORT             = 0x0010; // abortive close
        constexpr ULONG AFD_POLL_LOCAL_CLOSE       = 0x0020;
        constexpr ULONG AFD_POLL_CONNECT           = 0x0040;
        constexpr ULONG AFD_POLL_ACCEPT            = 0x0080;
        constexpr ULONG AFD_POLL_CONNECT_FAIL      = 0x0100;

        struct AfdPollHandleInfo {
            HANDLE Handle;
            ULONG  Events;
            LONG   Status; // NTSTATUS
        };

        struct AfdPollInfo {
            LARGE_INTEGER     Timeout;
            ULONG             NumberOfHandles;
            ULONG             Exclusive;
            AfdPollHandleInfo Handles[1];
        };

        // ---- NTDLL function pointers (loaded once) -------------------------

        struct IoStatusBlock {
            union {
                LONG    Status; // NTSTATUS
                void*   Pointer;
            };
            ULONG_PTR Information;
        };

        using NtDeviceIoControlFile_t = LONG(NTAPI*)(HANDLE, HANDLE, void*, void*, IoStatusBlock*, ULONG, void*, ULONG, void*,
                                                     ULONG);
        using NtCancelIoFileEx_t      = LONG(NTAPI*)(HANDLE, IoStatusBlock*, IoStatusBlock*);

        // ntdll exports are process-wide and identical for every reactor, so they
        // are resolved once into this immutable cache. The function-local static
        // makes init thread-safe under concurrent Reactor::create() calls.
        struct NtAfd {
            NtDeviceIoControlFile_t device_io_control = nullptr;
            NtCancelIoFileEx_t      cancel_io_file_ex = nullptr;

            NtAfd() {
                if (HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll")) {
                    device_io_control = reinterpret_cast<NtDeviceIoControlFile_t>(::GetProcAddress(ntdll, "NtDeviceIoControlFile"));
                    cancel_io_file_ex = reinterpret_cast<NtCancelIoFileEx_t>(::GetProcAddress(ntdll, "NtCancelIoFileEx"));
                }
            }
        };

        const NtAfd& nt_afd() {
            static const NtAfd fns;
            return fns;
        }

        // ---- Sentinel completion key used by wake() ------------------------

        constexpr ULONG_PTR WAKE_KEY = static_cast<ULONG_PTR>(-1);

        // NTSTATUS reported by a completion produced by cancelling its request.
        constexpr LONG kStatusCancelled = static_cast<LONG>(0xC0000120L);

        // ---- Event flag conversion -----------------------------------------

        ULONG to_afd(Event e) noexcept {
            ULONG flags = 0;
            if (has(e, Event::Readable))
                flags |= AFD_POLL_RECEIVE | AFD_POLL_ACCEPT | AFD_POLL_DISCONNECT | AFD_POLL_ABORT;
            if (has(e, Event::Writable))
                flags |= AFD_POLL_SEND | AFD_POLL_CONNECT_FAIL;
            return flags;
        }

        Event from_afd(ULONG flags) noexcept {
            Event e = Event::None;
            if (flags & (AFD_POLL_RECEIVE | AFD_POLL_ACCEPT))
                e |= Event::Readable;
            if (flags & AFD_POLL_SEND)
                e |= Event::Writable;
            if (flags & (AFD_POLL_DISCONNECT | AFD_POLL_LOCAL_CLOSE))
                e |= Event::HangUp;
            if (flags & (AFD_POLL_ABORT | AFD_POLL_CONNECT_FAIL))
                e |= Event::Error;
            return e;
        }

        // Walk SIO_BASE_HANDLE until the socket's true base handle is found.
        // Layered service providers (LSPs) may wrap a socket; the AFD ioctl
        // requires the actual kernel handle.
        HANDLE resolve_base_handle(SOCKET s) {
            SOCKET base = s;
            for (;;) {
                SOCKET next = INVALID_SOCKET;
                DWORD  bytes;
                if (::WSAIoctl(base, SIO_BASE_HANDLE, nullptr, 0, &next, sizeof(next), &bytes, nullptr, nullptr) != 0)
                    break;
                if (next == base || next == INVALID_SOCKET)
                    break;
                base = next;
            }
            return reinterpret_cast<HANDLE>(base);
        }

    } // namespace

    // Freed only in ~Reactor: the IOCP association is permanent and completions
    // reference this by pointer. active = registered; pending = poll in flight.
    struct Reactor::AfdPollContext {
        OVERLAPPED   overlapped{};
        AfdPollInfo  in_buf{};
        AfdPollInfo  out_buf{};
        u64          user_token    = 0;
        NativeHandle socket_handle = INVALID_NATIVE_HANDLE;
        HANDLE       base_handle   = nullptr;
        Event        interest      = Event::None;
        bool         active        = false;
        bool         pending       = false;
    };

    stl::result<> Reactor::submit_poll(AfdPollContext& ctx) {
        std::memset(&ctx.overlapped, 0, sizeof(ctx.overlapped));
        ctx.in_buf.Timeout.QuadPart    = INT64_MIN; // infinite (until event)
        ctx.in_buf.NumberOfHandles     = 1;
        ctx.in_buf.Exclusive           = 0;
        ctx.in_buf.Handles[0].Handle   = ctx.base_handle;
        ctx.in_buf.Handles[0].Events   = to_afd(ctx.interest);
        ctx.in_buf.Handles[0].Status   = 0;

        // overlapped.Internal/InternalHigh alias the IO_STATUS_BLOCK fields.
        auto* iosb = reinterpret_cast<IoStatusBlock*>(&ctx.overlapped.Internal);

        LONG status = nt_afd().device_io_control(ctx.base_handle, nullptr, nullptr, &ctx.overlapped, iosb, kIoctlAfdPoll,
                                                 &ctx.in_buf, sizeof(ctx.in_buf), &ctx.out_buf, sizeof(ctx.out_buf));

        // STATUS_PENDING (0x103) is success-but-async; anything else nonzero fails.
        if (status != 0 && status != 0x00000103L)
            return stl::make_error<>("NtDeviceIoControlFile(IOCTL_AFD_POLL) failed: 0x{:x}", static_cast<u32>(status));

        ctx.pending = true;
        return stl::result_success();
    }

    // Request cancellation; pending stays set until the completion is drained.
    void Reactor::cancel_poll(AfdPollContext& ctx) noexcept {
        if (!ctx.pending)
            return;
        if (nt_afd().cancel_io_file_ex) {
            IoStatusBlock iosb_in{};
            IoStatusBlock iosb_out{};
            iosb_in.Pointer = &ctx.overlapped;
            (void)nt_afd().cancel_io_file_ex(ctx.base_handle, &iosb_in, &iosb_out);
        } else {
            (void)::CancelIoEx(ctx.base_handle, &ctx.overlapped);
        }
    }

    stl::result<Reactor> Reactor::create() {
        if (!nt_afd().device_io_control)
            return stl::make_error<Reactor>("NtDeviceIoControlFile not found in ntdll");

        HANDLE iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (!iocp)
            return stl::make_error<Reactor>("CreateIoCompletionPort failed: {}", ::GetLastError());

        Reactor r;
        r.m_iocp = iocp;
        return stl::result<Reactor>(stl::success, stl::move(r));
    }

    Reactor::~Reactor() {
        if (!m_iocp) {
            m_contexts.clear();
            return;
        }
        // Stop watching everything, then drain in-flight completions so the kernel
        // is finished with every OVERLAPPED before the contexts are freed.
        for (auto& kv : m_contexts) {
            kv.second->active = false;
            cancel_poll(*kv.second);
        }
        for (;;) {
            bool any_pending = false;
            for (auto& kv : m_contexts)
                if (kv.second->pending) {
                    any_pending = true;
                    break;
                }
            if (!any_pending)
                break;

            OVERLAPPED_ENTRY entries[MAX_EVENTS_PER_WAIT];
            ULONG            removed = 0;
            if (!::GetQueuedCompletionStatusEx(m_iocp, entries, MAX_EVENTS_PER_WAIT, &removed, 1000, FALSE))
                break; // timeout/error: stop rather than hang teardown
            for (ULONG i = 0; i < removed; ++i)
                if (entries[i].lpCompletionKey != WAKE_KEY)
                    reinterpret_cast<AfdPollContext*>(entries[i].lpCompletionKey)->pending = false;
        }

        ::CloseHandle(m_iocp);
        m_iocp = nullptr;
        for (auto& kv : m_contexts)
            delete kv.second;
        m_contexts.clear();
    }

    Reactor::Reactor(Reactor&& o) noexcept : m_iocp(o.m_iocp), m_contexts(stl::move(o.m_contexts)) { o.m_iocp = nullptr; }

    Reactor& Reactor::operator=(Reactor&& o) noexcept {
        if (this != &o) {
            // tmp adopts o's resources, then takes ours and tears them down on exit.
            Reactor tmp(stl::move(o));
            void* iocp_tmp = m_iocp;
            m_iocp         = tmp.m_iocp;
            tmp.m_iocp     = iocp_tmp;
            m_contexts.swap(tmp.m_contexts);
        }
        return *this;
    }

    stl::result<> Reactor::add(NativeHandle handle, Event interest, u64 token) {
        if (auto it = m_contexts.find(handle); it != m_contexts.end()) {
            AfdPollContext& ctx = *it->second;
            if (ctx.active)
                return stl::make_error<>("Reactor::add: handle already registered");

            // A prior remove() may have left a cancellation in flight; drain it
            // (requeuing other sockets' events) before reusing the OVERLAPPED.
            while (ctx.pending) {
                OVERLAPPED_ENTRY entries[MAX_EVENTS_PER_WAIT];
                ULONG            removed = 0;
                if (!::GetQueuedCompletionStatusEx(m_iocp, entries, MAX_EVENTS_PER_WAIT, &removed, 1000, FALSE))
                    break;
                for (ULONG i = 0; i < removed; ++i) {
                    if (entries[i].lpCompletionKey == WAKE_KEY)
                        continue;
                    auto* c = reinterpret_cast<AfdPollContext*>(entries[i].lpCompletionKey);
                    if (c == &ctx || !c->active)
                        c->pending = false;
                    else
                        ::PostQueuedCompletionStatus(m_iocp, entries[i].dwNumberOfBytesTransferred, entries[i].lpCompletionKey,
                                                     entries[i].lpOverlapped);
                }
            }

            HANDLE base = resolve_base_handle(static_cast<SOCKET>(handle));
            if (!base)
                return stl::make_error<>("Reactor::add: WSAIoctl(SIO_BASE_HANDLE) failed: {}", ::WSAGetLastError());
            ctx.base_handle = base;
            // Re-associate in case the handle value was recycled for a new socket;
            // ERROR_INVALID_PARAMETER means it's the same, still-associated socket.
            if (!::CreateIoCompletionPort(base, m_iocp, reinterpret_cast<ULONG_PTR>(&ctx), 0)) {
                if (DWORD err = ::GetLastError(); err != ERROR_INVALID_PARAMETER)
                    return stl::make_error<>("CreateIoCompletionPort(associate) failed: {}", err);
            }
            ctx.interest   = interest;
            ctx.user_token = token;
            ctx.active     = true;
            return submit_poll(ctx);
        }

        auto ctx           = stl::make_unique<AfdPollContext>();
        ctx->socket_handle = handle;
        ctx->user_token    = token;
        ctx->interest      = interest;
        ctx->base_handle   = resolve_base_handle(static_cast<SOCKET>(handle));
        if (!ctx->base_handle)
            return stl::make_error<>("Reactor::add: WSAIoctl(SIO_BASE_HANDLE) failed: {}", ::WSAGetLastError());

        // Associate once; the completion key is the context pointer. Permanent.
        if (!::CreateIoCompletionPort(ctx->base_handle, m_iocp, reinterpret_cast<ULONG_PTR>(ctx.get()), 0))
            return stl::make_error<>("CreateIoCompletionPort(associate) failed: {}", ::GetLastError());

        AfdPollContext* raw = ctx.get();
        raw->active         = true;
        m_contexts.emplace(handle, ctx.release());
        if (auto r = submit_poll(*raw); !r) {
            raw->active = false; // keep the associated context for reuse
            return r;
        }
        return stl::result_success();
    }

    stl::result<> Reactor::modify(NativeHandle handle, Event interest, u64 token) {
        if (auto r = remove(handle); !r)
            return r;
        return add(handle, interest, token);
    }

    stl::result<> Reactor::remove(NativeHandle handle) {
        auto it = m_contexts.find(handle);
        if (it == m_contexts.end())
            return stl::make_error<>("Reactor::remove: handle not registered");
        AfdPollContext& ctx = *it->second;
        ctx.active          = false;
        cancel_poll(ctx); // context kept; freed only in ~Reactor
        return stl::result_success();
    }

    stl::result<stl::size_t> Reactor::wait(stl::span<Trigger> out, std::chrono::milliseconds timeout) {
        if (out.empty())
            return stl::result<stl::size_t>(stl::success, stl::size_t{0});

        OVERLAPPED_ENTRY entries[MAX_EVENTS_PER_WAIT];
        const ULONG      max_n = static_cast<ULONG>(out.size() < MAX_EVENTS_PER_WAIT ? out.size() : MAX_EVENTS_PER_WAIT);

        DWORD timeout_ms = (timeout < std::chrono::milliseconds::zero()) ? INFINITE : static_cast<DWORD>(timeout.count());

        ULONG removed = 0;
        BOOL  ok      = ::GetQueuedCompletionStatusEx(m_iocp, entries, max_n, &removed, timeout_ms, FALSE);
        if (!ok) {
            DWORD err = ::GetLastError();
            if (err == WAIT_TIMEOUT)
                return stl::result<stl::size_t>(stl::success, stl::size_t{0});
            return stl::make_error<stl::size_t>("GetQueuedCompletionStatusEx failed: {}", err);
        }

        stl::size_t written = 0;
        for (ULONG i = 0; i < removed; ++i) {
            if (entries[i].lpCompletionKey == WAKE_KEY)
                continue;

            auto* ctx = reinterpret_cast<AfdPollContext*>(entries[i].lpCompletionKey);
            if (!ctx || !ctx->pending)
                continue; // no poll was outstanding
            ctx->pending = false;
            if (!ctx->active)
                continue; // removed between submit and completion
            const LONG status = static_cast<LONG>(ctx->overlapped.Internal);
            if (status == kStatusCancelled)
                continue; // a cancellation we requested

            ULONG fired             = (ctx->out_buf.NumberOfHandles >= 1) ? ctx->out_buf.Handles[0].Events : 0;
            out[written].events     = (status == 0) ? from_afd(fired) : Event::Error;
            out[written].user_token = ctx->user_token;
            ++written;
        }
        return stl::result<stl::size_t>(stl::success, written);
    }

    void Reactor::wake() {
        if (!m_iocp)
            return;
        ::PostQueuedCompletionStatus(m_iocp, 0, WAKE_KEY, nullptr);
    }

} // namespace sap::io
