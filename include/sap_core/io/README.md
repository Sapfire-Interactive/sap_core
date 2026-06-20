# `sap::io`

Cross-platform readiness reactor. Sockets, pipes, eventfd, timerfd —
anything that can be polled. **Not** for regular files.

```cpp
#include <sap_core/io/event.h>     // Event flag enum
#include <sap_core/io/reactor.h>   // Reactor, Trigger, NativeHandle
```

## Quick reference

| Type / function                                            | Purpose                                                          |
|------------------------------------------------------------|------------------------------------------------------------------|
| `Event`                                                    | Bit-flag enum: `Readable`, `Writable`, `Error`, `HangUp`.        |
| `NativeHandle`                                             | `int` on Linux, `std::uintptr_t` on Windows.                     |
| `Reactor`                                                  | One-per-thread readiness reactor.                                |
| `Reactor::create()`                                        | Allocate the underlying kernel resources.                        |
| `add(handle, interest, token)`                             | Register interest in events on a handle.                         |
| `modify(handle, interest, token)`                          | Change interest / token without re-registration.                 |
| `remove(handle)`                                           | Deregister.                                                      |
| `wait(span<Trigger>, timeout)`                             | Block until events fire; up to `MAX_EVENTS_PER_WAIT` per call.   |
| `wake()`                                                   | Thread-safe; nudges a parked `wait()` to return.                 |
| `Trigger`                                                  | `{ Event events; u64 user_token; }` returned by `wait()`.        |

## When to use the Reactor directly

Most user code shouldn't. The Reactor is a low-level primitive; the
coroutine-friendly path is `sap::async::IoAwaiter` (defined in
`sap_core/async/executor.h`), which wraps the Reactor and gives you
`co_await` semantics.

Reach for the Reactor directly when:
- You're implementing a new awaitable (e.g. a different timer, a
  custom socket type).
- You're integrating with an existing event loop that isn't built on
  coroutines.
- You're writing a test that exercises the readiness layer in
  isolation.

## End-to-end example

A self-pipe waker, exercised by `wait()` returning the `Readable`
trigger:

```cpp
#include <sap_core/io/reactor.h>

#include <unistd.h>
#include <vector>

int main() {
    int fds[2];
    ::pipe(fds);

    auto r = sap::io::Reactor::create();
    if (!r) return 1;
    auto& reactor = r.value();

    constexpr sap::u64 my_token = 7;
    (void)reactor.add(fds[0], sap::io::Event::Readable, my_token);

    // From this thread or another:
    (void)::write(fds[1], "x", 1);

    sap::io::Trigger triggers[16];
    auto n = reactor.wait(stl::span<sap::io::Trigger>(triggers, 16),
                          std::chrono::milliseconds(1000));
    if (n && n.value() > 0) {
        // triggers[0].user_token == 7
        // triggers[0].events & Event::Readable
    }

    (void)reactor.remove(fds[0]);
    ::close(fds[0]);
    ::close(fds[1]);
}
```

The same code on Windows uses `NativeHandle` for the socket / pipe
handle; everything else is identical.

## `Event`

Bit flags. Combine with `|`, test with `has()`:

```cpp
auto interest = sap::io::Event::Readable | sap::io::Event::Writable;

if (sap::io::has(fired, sap::io::Event::HangUp)) {
    // peer closed
}
```

| Flag      | Meaning                                                 |
|-----------|---------------------------------------------------------|
| `None`    | Default-constructed sentinel.                           |
| `Readable`| `recv()`/`read()` won't block.                          |
| `Writable`| `send()`/`write()` won't block.                         |
| `Error`   | Error condition; always reported when present.          |
| `HangUp`  | Peer closed the connection; always reported.            |

`Error` and `HangUp` are reported by the kernel regardless of whether
they were in the interest set — they show up alongside the requested
events.

## `Reactor`

### Construction

```cpp
auto r = sap::io::Reactor::create();
if (!r) { /* allocation failed */ }
```

`create()` allocates the underlying kernel object (an epoll fd + an
eventfd waker on Linux; an IOCP handle on Windows). Returns
`stl::result<Reactor>` so the failure case is explicit.

The Reactor is **move-only**. A Reactor instance is tied to its
creating process and shouldn't be shared across threads as a value;
share access through `wake()` instead.

### Registration

```cpp
(void)reactor.add(fd, Event::Readable | Event::Writable, my_token);
(void)reactor.modify(fd, Event::Readable, my_token);
(void)reactor.remove(fd);
```

The `token` is an opaque `u64` you choose. It's returned verbatim
in `Trigger::user_token`. Common idiom: cast a pointer to your awaiter
into a `u64` so the trigger handler can recover the awaiter cheaply.
That's exactly what `sap::async::IoAwaiter` does.

**`u64(-1)` is reserved** — both backends use it as the wake-sentinel
token. The `wait()` implementation filters this token out before
returning. If your code uses it, you'll never see those triggers and
weird things will happen.

### Waiting

```cpp
sap::io::Trigger triggers[16];
auto n = reactor.wait(stl::span<sap::io::Trigger>(triggers, 16),
                      std::chrono::milliseconds(-1));   // wait forever
```

| Timeout value | Behavior                              |
|---------------|---------------------------------------|
| `-1`          | Block until at least one event fires. |
| `0`           | Poll: return immediately.             |
| `> 0`         | Block up to that many milliseconds.   |

`wait()` fills your span and returns the count. If you pass a buffer
smaller than the number of ready events, the rest stay in the
kernel queue and surface on the next call. The internal cap per call
is `Reactor::MAX_EVENTS_PER_WAIT` (64); buffers larger than that are
clipped to 64.

The hot path uses a stack-allocated `epoll_event` (or
`OVERLAPPED_ENTRY`) buffer internally — no heap allocation per
`wait()` call.

### Waking

```cpp
reactor.wake();   // thread-safe; may be called from a different thread
```

The only thread-safe surface. A parked `wait()` will return shortly
after `wake()` is called. The wake mechanism is:

- **Linux**: an `eventfd` registered in the epoll set with the reserved
  sentinel token. `wake()` writes to the eventfd; `wait()` filters
  the sentinel out.
- **Windows**: `PostQueuedCompletionStatus` with the reserved
  completion key. `wait()` filters it out.

Use `wake()` to nudge the reactor from outside its owning thread —
typically a file-I/O worker that just finished an offload and needs
the reactor thread to pick up the completion.

## Threading model

A `Reactor` is owned by a single thread for `add` / `modify` /
`remove` / `wait`. The only cross-thread call is `wake()`.

The coroutine executor (`sap::async::Executor`) inherits this
contract — single-threaded by design. If you want multi-threaded
work, run N independent executors and shard work across them.

## Backend specifics

### Linux (`src/io/epoll_reactor.cpp`)

- `epoll_create1(EPOLL_CLOEXEC)` for the kernel object.
- `eventfd` for the waker.
- **Level-triggered** (no `EPOLLET`). The fd stays "ready" until
  you actually consume the bytes. Matches the `sap::async::IoAwaiter`
  contract of "one fire per registration; remove on resume."
- No `EPOLLONESHOT`. Registrations stay until `remove()`.

`Event::Error` and `Event::HangUp` map to `EPOLLERR` / `EPOLLHUP`,
which the kernel always reports.

### Windows (`src/io/iocp_reactor.cpp`)

- One IOCP per Reactor.
- For each registered socket, the *base* handle (resolved via
  `WSAIoctl(SIO_BASE_HANDLE)`) is associated with the IOCP.
- Polling is requested per-socket via `NtDeviceIoControlFile` +
  `IOCTL_AFD_POLL` and an `AFD_POLL_INFO` struct. Completions arrive
  on the IOCP carrying the OVERLAPPED pointer in the per-fd context.
- AFD is one-shot. The backend re-submits the poll request on each
  completion so callers see level-triggered semantics.

**This backend has not been compiled or run on Windows yet.** The AFD
interface is reverse-engineered (stable in libuv / asio / NSPR for
years but not officially documented by Microsoft). Likely failure
modes:

- `AFD_POLL_INFO` layout drift across Windows versions.
- `SIO_BASE_HANDLE` resolution on layered service providers.
- Cancellation race in `remove()` (a completion may already be in
  flight when we issue `NtCancelIoFileEx`).

The Linux backend is the verified path until someone runs the
Windows test suite end-to-end.

## What the Reactor is **not** for

### Regular files

Regular files are always "ready" but reads can still block on disk
I/O. epoll refuses to register them; AFD_POLL is socket-only. There's
no useful polling answer for files at this layer.

The plan is `sap::async::AsyncFile` — a thread-pool offload backed
by `job_system`, with the worker calling `reactor.wake()` to nudge
the reactor when a read/write completes. Not in the tree yet.

### `io_uring`

Not part of the Reactor. `io_uring` is completion-based (a proactor)
and fights a readiness abstraction. If we adopt it, it'll be a
separate `sap::io::Proactor` track with its own API, not a Reactor
backend.

### Scheduling / runqueues

The Reactor knows about kernel-level readiness, not about coroutines
or runqueues. The runqueue lives in `sap::async::Executor`, which
calls `wait()` to get triggers and then resumes the right
coroutines. Keep the layering: Reactor → Executor → user code.

## Implementation status

| Item                                | Status                                                      |
|-------------------------------------|-------------------------------------------------------------|
| `Event` flag enum                   | Shipped.                                                    |
| `Reactor` interface                 | Shipped.                                                    |
| Linux backend (epoll + eventfd)     | Shipped; 12 tests pass.                                     |
| Windows backend (IOCP + AFD_POLL)   | Shipped, **unverified** — needs Windows compile + tests.    |
| macOS / BSD backend (kqueue)        | Out of scope. Unsupported platforms get `#error` at compile.|
| `io_uring` proactor                 | Future work; separate API track.                            |
| File I/O via thread-pool offload    | Future work; lives behind `sap::async::AsyncFile`.          |
