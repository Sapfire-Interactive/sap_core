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
#include "sap_core/types.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>

#include <cstring>
#include <utility>

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

        NtDeviceIoControlFile_t g_nt_device_io_control_file = nullptr;
        NtCancelIoFileEx_t      g_nt_cancel_io_file_ex      = nullptr;

        void load_ntdll() {
            if (g_nt_device_io_control_file)
                return;
            HMODULE ntdll = ::GetModuleHandleA("ntdll.dll");
            if (!ntdll)
                return;
            g_nt_device_io_control_file = reinterpret_cast<NtDeviceIoControlFile_t>(::GetProcAddress(ntdll, "NtDeviceIoControlFile"));
            g_nt_cancel_io_file_ex      = reinterpret_cast<NtCancelIoFileEx_t>(::GetProcAddress(ntdll, "NtCancelIoFileEx"));
        }

        // ---- Sentinel completion key used by wake() ------------------------

        constexpr ULONG_PTR WAKE_KEY = static_cast<ULONG_PTR>(-1);

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

    // Per-fd state. Lives until remove() or Reactor destruction. The
    // OVERLAPPED + AfdPollInfo must outlive each in-flight kernel request.
    struct Reactor::AfdPollContext {
        OVERLAPPED   overlapped{};
        AfdPollInfo  in_buf{};
        AfdPollInfo  out_buf{};
        u64          user_token    = 0;
        NativeHandle socket_handle = INVALID_NATIVE_HANDLE;
        HANDLE       base_handle   = nullptr;
        Event        interest      = Event::None;
        bool         pending       = false;
    };

    namespace {

        // Submit an AFD poll for this context's current interest. If a previous
        // request is in flight, the caller must cancel it first.
        stl::result<> submit_poll(HANDLE iocp, Reactor::AfdPollContext& ctx) {
            (void)iocp; // ctx.base_handle is already associated with the IOCP

            std::memset(&ctx.overlapped, 0, sizeof(ctx.overlapped));
            ctx.in_buf.Timeout.QuadPart    = INT64_MIN; // infinite (until event)
            ctx.in_buf.NumberOfHandles     = 1;
            ctx.in_buf.Exclusive           = 0;
            ctx.in_buf.Handles[0].Handle   = ctx.base_handle;
            ctx.in_buf.Handles[0].Events   = to_afd(ctx.interest);
            ctx.in_buf.Handles[0].Status   = 0;

            auto* iosb = reinterpret_cast<IoStatusBlock*>(&ctx.overlapped.Internal);
            // overlapped.Internal/InternalHigh are aliased with NTSTATUS/Information.
            // We pass the OVERLAPPED's first two fields as the IO_STATUS_BLOCK.

            LONG status = g_nt_device_io_control_file(ctx.base_handle, nullptr, nullptr, &ctx.overlapped, iosb, kIoctlAfdPoll,
                                                      &ctx.in_buf, sizeof(ctx.in_buf), &ctx.out_buf, sizeof(ctx.out_buf));

            // STATUS_PENDING (0x103) is success-but-async; anything below 0 is failure.
            if (status != 0 && status != 0x00000103L)
                return stl::make_error<>("NtDeviceIoControlFile(IOCTL_AFD_POLL) failed: 0x{:x}", static_cast<u32>(status));

            ctx.pending = true;
            return stl::result_success();
        }

        void cancel_poll(Reactor::AfdPollContext& ctx) {
            if (!ctx.pending)
                return;
            if (g_nt_cancel_io_file_ex) {
                IoStatusBlock iosb_in{};
                IoStatusBlock iosb_out{};
                iosb_in.Pointer = &ctx.overlapped;
                (void)g_nt_cancel_io_file_ex(ctx.base_handle, &iosb_in, &iosb_out);
            } else {
                (void)::CancelIoEx(ctx.base_handle, &ctx.overlapped);
            }
            ctx.pending = false;
            // Note: the cancellation completion may still arrive on the IOCP;
            // wait() filters by ctx.pending to ignore stale completions.
        }

    } // namespace

    stl::result<Reactor> Reactor::create() {
        load_ntdll();
        if (!g_nt_device_io_control_file)
            return stl::make_error<Reactor>("NtDeviceIoControlFile not found in ntdll");

        HANDLE iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (!iocp)
            return stl::make_error<Reactor>("CreateIoCompletionPort failed: {}", ::GetLastError());

        Reactor r;
        r.m_iocp = iocp;
        return stl::result<Reactor>(stl::success, std::move(r));
    }

    Reactor::~Reactor() {
        // Cancel any in-flight polls before tearing down the contexts.
        for (auto& kv : m_contexts) {
            if (kv.second)
                cancel_poll(*kv.second);
        }
        m_contexts.clear();
        if (m_iocp)
            ::CloseHandle(m_iocp);
    }

    Reactor::Reactor(Reactor&& o) noexcept : m_iocp(o.m_iocp), m_contexts(std::move(o.m_contexts)) { o.m_iocp = nullptr; }

    Reactor& Reactor::operator=(Reactor&& o) noexcept {
        if (this != &o) {
            for (auto& kv : m_contexts) {
                if (kv.second)
                    cancel_poll(*kv.second);
            }
            m_contexts.clear();
            if (m_iocp)
                ::CloseHandle(m_iocp);
            m_iocp       = o.m_iocp;
            m_contexts   = std::move(o.m_contexts);
            o.m_iocp     = nullptr;
        }
        return *this;
    }

    stl::result<> Reactor::add(NativeHandle handle, Event interest, u64 token) {
        if (m_contexts.find(handle) != m_contexts.end())
            return stl::make_error<>("Reactor::add: handle already registered");

        auto ctx              = stl::make_unique<AfdPollContext>();
        ctx->socket_handle    = handle;
        ctx->user_token       = token;
        ctx->interest         = interest;
        ctx->base_handle      = resolve_base_handle(static_cast<SOCKET>(handle));
        if (!ctx->base_handle)
            return stl::make_error<>("Reactor::add: WSAIoctl(SIO_BASE_HANDLE) failed: {}", ::WSAGetLastError());

        // Associate the base handle with the IOCP. The completion key carries
        // the per-fd context pointer so wait() can recover it.
        if (!::CreateIoCompletionPort(ctx->base_handle, m_iocp, reinterpret_cast<ULONG_PTR>(ctx.get()), 0))
            return stl::make_error<>("CreateIoCompletionPort(associate) failed: {}", ::GetLastError());

        auto submit = submit_poll(m_iocp, *ctx);
        if (!submit)
            return submit;

        m_contexts.emplace(handle, std::move(ctx));
        return stl::result_success();
    }

    stl::result<> Reactor::modify(NativeHandle handle, Event interest, u64 token) {
        auto it = m_contexts.find(handle);
        if (it == m_contexts.end())
            return stl::make_error<>("Reactor::modify: handle not registered");
        auto& ctx       = *it->second;
        cancel_poll(ctx);
        ctx.interest    = interest;
        ctx.user_token  = token;
        return submit_poll(m_iocp, ctx);
    }

    stl::result<> Reactor::remove(NativeHandle handle) {
        auto it = m_contexts.find(handle);
        if (it == m_contexts.end())
            return stl::make_error<>("Reactor::remove: handle not registered");
        cancel_poll(*it->second);
        m_contexts.erase(it);
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
                continue; // stale completion from a cancelled request

            ctx->pending = false;
            ULONG fired  = (ctx->out_buf.NumberOfHandles >= 1) ? ctx->out_buf.Handles[0].Events : 0;

            out[written].events     = from_afd(fired);
            out[written].user_token = ctx->user_token;
            ++written;

            // AFD_POLL is one-shot; resubmit so we stay registered.
            (void)submit_poll(m_iocp, *ctx);
        }
        return stl::result<stl::size_t>(stl::success, written);
    }

    void Reactor::wake() {
        if (!m_iocp)
            return;
        ::PostQueuedCompletionStatus(m_iocp, 0, WAKE_KEY, nullptr);
    }

} // namespace sap::io
