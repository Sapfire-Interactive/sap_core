# `sap::async`

C++20 coroutine primitives, a single-threaded executor, cancellation
tokens, and time/IO awaitables for sap_core.

```cpp
#include <sap_core/async/task.h>        // Task<T>
#include <sap_core/async/sync_wait.h>   // sync_wait(Task<T>)
#include <sap_core/async/when_all.h>    // when_all(Task<Ts>...)
#include <sap_core/async/executor.h>    // Executor, IoAwaiter
#include <sap_core/async/sleep_for.h>   // sleep_for
#include <sap_core/async/spawn.h>       // spawn, SpawnHandle, sync_wait(SpawnHandle)
#include <sap_core/async/stop_token.h>  // StopSource, StopToken, CancelledError
```

## Quick reference

| Type / function                       | Purpose                                                            |
|---------------------------------------|--------------------------------------------------------------------|
| `Task<T>`                             | Coroutine return type. Lazy; awaitable; can be detached.           |
| `sync_wait(Task<T>)`                  | Blocking driver for tasks that don't suspend on I/O.               |
| `when_all(Task<Ts>...)`               | Compose multiple tasks; returns `Task<tuple<Ts...>>`.              |
| `Executor`                            | Single-threaded run loop tied to a `sap::io::Reactor`.             |
| `Executor::run_until(handle)`         | Drive the loop until a specific coroutine handle is done.          |
| `IoAwaiter`                           | Bridge a coroutine to a reactor: park until a fd is ready.         |
| `sleep_for(ex, dt[, tok])`            | Cancellable delay.                                                 |
| `spawn(ex, Task<T>) → SpawnHandle<T>` | Start a task now; collect later.                                   |
| `sync_wait(SpawnHandle<T>&&)`         | Sync collection that drives the executor's reactor.                |
| `StopSource` / `StopToken`            | Cooperative cancellation primitives.                               |
| `CancelledError`                      | Exception thrown by a cancelled `IoAwaiter`.                       |

## End-to-end example

```cpp
#include <sap_core/async/executor.h>
#include <sap_core/async/sleep_for.h>
#include <sap_core/async/spawn.h>
#include <sap_core/async/stop_token.h>
#include <sap_core/async/task.h>

using namespace sap::async;
using namespace std::chrono_literals;

Task<int> fetch(Executor& ex, StopToken tok) {
    co_await sleep_for(ex, 100ms, tok);
    co_return 42;
}

int main() {
    auto exr = Executor::create();
    auto& ex = exr.value();

    StopSource src;
    auto handle = spawn(ex, fetch(ex, src.token()));

    // ... synchronous work runs in parallel with fetch's sleep ...

    int v = sync_wait(stl::move(handle));   // drives the executor until fetch is done
    return v;
}
```

---

## `Task<T>`

Coroutine return type. **Lazy**: the body doesn't execute until somebody
consumes the Task. The three ways to consume one:

1. `co_await task` — from inside another Task.
2. `sync_wait(task)` — from synchronous code (no reactor).
3. `task.detach()` — fire-and-forget; result discarded.

```cpp
Task<int> compute() {
    co_return 42;
}

Task<int> use_it() {
    int n = co_await compute();   // body runs here
    co_return n * 2;
}
```

### `.detach()`

Releases ownership of the coroutine frame and starts the body running.
The frame self-destroys on completion. Exceptions thrown inside a
detached Task are stored in the promise and discarded on frame
destruction.

```cpp
auto bg = log_to_disk_async();
bg.detach();
```

### Move-only

Coroutine handles can't be copied without double-freeing the frame.

```cpp
Task<int> t = compute();
auto t2 = stl::move(t);   // OK
auto t3 = t;              // compile error
```

### `value_type`

`Task<T>::value_type` is `T`. Useful for combinators that need to
extract the value type from a Task at compile time.

---

## `sync_wait`

Two overloads:

```cpp
T sync_wait(Task<T> task);
T sync_wait(SpawnHandle<T>&& handle);
```

`sync_wait(Task<T>)` blocks the calling thread and drives the Task to
completion **synchronously**. It does not run an event loop, so a Task
that suspends on real I/O (e.g. via an `IoAwaiter`) won't be resumed.
Use it for pure computation, composed tasks that all reduce to
synchronous work, or tasks that complete immediately.

```cpp
Task<int> sum_async() { co_return 7; }

int main() {
    int n = sync_wait(sum_async());
    return n;
}
```

`sync_wait(SpawnHandle<T>&&)` drives the **executor** (via
`run_until`) until the handle is done — so it works for tasks that
park on I/O. See `spawn` below.

If the Task throws, the exception is rethrown out of `sync_wait`.

---

## `when_all`

Runs multiple Tasks and returns a `Task` that completes once every input
has completed. The result is a `tuple` in argument order.

```cpp
Task<int>    fetch_user();
Task<bool>   fetch_flag();
Task<double> fetch_score();

auto [user, flag, score] = sync_wait(
    when_all(fetch_user(), fetch_flag(), fetch_score()));
```

Today's implementation is **sequential**: tasks are awaited in argument
order. The contract (returned tuple, exception propagation, ordering)
matches the eventual concurrent implementation, so user code is
forward-compatible. The wall-clock improvement arrives later.

### Exceptions

If any Task throws, the exception is propagated out of `when_all` and
remaining results are discarded.

### Restrictions

- Void Tasks aren't supported (can't put `void` in a `tuple`).
- Tasks are consumed by value — pass `make_task()` directly or
  `stl::move(task)` for named locals.

---

## `Executor`

Single-threaded coroutine driver. Owns a `sap::io::Reactor` (epoll on
Linux, IOCP+AFD on Windows). The run loop alternates between draining a
ready queue of coroutine handles and parking in `reactor.wait()`.

```cpp
auto exr = Executor::create();
if (!exr) {
    // Reactor allocation failed (fd limits, OOM, etc.)
    return 1;
}
auto& ex = exr.value();

ex.spawn_detach(my_task());   // fire-and-forget
ex.run();                     // block until everything's done
```

### Lifecycle methods

| Method                       | What it does                                                            |
|------------------------------|-------------------------------------------------------------------------|
| `run()`                      | Drive until ready queue and pending I/O are both empty (or `stop()`).   |
| `run_until(handle)`          | Drive until `handle.done()` (or stop, or nothing left to do).           |
| `stop()`                     | Set a flag; loop exits on its next iteration.                           |
| `schedule(handle)`           | Push a coroutine handle onto the ready queue.                           |
| `spawn_detach(Task<T>)`      | Detach a task and run its body up to first suspension.                  |
| `reactor()`                  | Access the underlying `sap::io::Reactor` (thread-safe `wake()`).        |

### Threading

Strictly single-threaded. All methods (including `stop()`,
`run_until()`, and `schedule()`) must run on the thread that called
`run()`. The only thread-safe surface is `ex.reactor().wake()` —
useful to nudge a parked `wait()` from a signal handler or a
file-I/O worker thread.

If you want cross-thread `stop()` or `schedule()`, the smallest
upgrade is an atomic running-flag + a lock-guarded post queue.
That's a future change; today, don't reach across threads.

### IoAwaiter

The bridge between a coroutine and the reactor:

```cpp
Task<void> wait_readable(Executor& ex, int fd) {
    IoAwaiter awaiter(ex, fd, sap::io::Event::Readable);
    sap::io::Event fired = co_await awaiter;
    // fd is ready to read
}
```

With cancellation:

```cpp
Task<void> wait_or_cancel(Executor& ex, int fd, StopToken tok) {
    IoAwaiter awaiter(ex, fd, sap::io::Event::Readable, stl::move(tok));
    co_await awaiter;   // throws CancelledError on stop_requested
}
```

**The awaiter's address is the reactor token**, so it must live across
the suspend point. Always declare it as a local in the coroutine body;
coroutine locals are pinned in the heap-allocated frame.

---

## `sleep_for`

Cancellable delay backed by `timerfd` on Linux. On Windows it waits on a
loopback socket that a threadpool timer signals when the delay elapses.

```cpp
co_await sleep_for(ex, 100ms);            // uncancellable

co_await sleep_for(ex, 100ms, my_token);  // cancellable
```

Example heartbeat loop that exits when the source flips:

```cpp
Task<void> heartbeat(Executor& ex, StopToken tok) {
    while (!tok.stop_requested()) {
        co_await sleep_for(ex, 1s, tok);
        emit_heartbeat();
    }
}
```

---

## `spawn` / `SpawnHandle`

"Start now, await later." Lets you eagerly kick off a Task and either
collect its result via `co_await` (from another Task) or `sync_wait`
(synchronously).

```cpp
Task<int> compute(Executor& ex) {
    co_await sleep_for(ex, 100ms);
    co_return 42;
}

Task<int> use(Executor& ex) {
    auto h = spawn(ex, compute(ex));   // body starts running NOW
    do_other_work();                   // overlaps with compute's sleep
    co_return co_await stl::move(h);   // collect the result
}
```

### `spawn` vs `Task::detach()`

| Use                                  | Picks                |
|--------------------------------------|----------------------|
| Don't care about the result          | `task.detach()`      |
| Will collect the result or exception | `spawn(ex, task)`    |

### Single-shot

A `SpawnHandle<T>` may be awaited at most once. The value is moved out
of the promise's `optional<T>` at `co_await`; awaiting twice silently
returns a moved-from value. Move the handle into the `co_await`:

```cpp
auto h = spawn(ex, compute(ex));
int  v = co_await stl::move(h);    // consume
```

### Dropping a handle

Dropping a `SpawnHandle` while its runner is pending does **not** cancel
the task — the body keeps running and self-destroys at completion. The
result and exception are discarded.

If you want cancellation, use a `StopSource`.

### Synchronous collection

```cpp
auto h = spawn(ex, fetch_thing(ex));
int  v = sync_wait(stl::move(h));   // drives ex.run_until(h.handle())
```

`sync_wait(SpawnHandle<T>&&)` is the right tool when you're not inside
a coroutine but the task suspends on real I/O. It's **not** re-entrant
— don't call it from inside a coroutine the executor is already
resuming. From inside a coroutine, use `co_await stl::move(h)`.

### Concurrent overlap

`when_all` collects all results in argument order; `spawn` lets you
explicitly start work early and decide when to wait for it:

```cpp
Task<int> overlap(Executor& ex) {
    auto a = spawn(ex, fetch_from_a(ex));
    auto b = spawn(ex, fetch_from_b(ex));
    // both running concurrently
    co_return (co_await stl::move(a)) + (co_await stl::move(b));
}
```

---

## `StopSource` / `StopToken` / `CancelledError`

Cooperative cancellation, opt-in per awaiter. Loosely modeled on
`std::stop_token`.

```cpp
StopSource src;
auto handle = spawn(ex, fetch(ex, src.token()));

// ... later, from another task or before sync_wait ...
src.request_stop();

try {
    int v = sync_wait(stl::move(handle));
} catch (const CancelledError&) {
    // task was cancelled mid-flight
}
```

### Wiring the token

The token is only useful where an awaiter accepts one. Today that's
`IoAwaiter` and `sleep_for`. Plumb the token through every layer that
should be cancellable:

```cpp
Task<int> inner(Executor& ex, StopToken tok) {
    co_await sleep_for(ex, 10s, tok);
    co_return 7;
}

Task<int> outer(Executor& ex, StopToken tok) {
    co_return co_await inner(ex, stl::move(tok));
}
```

Without a token, awaiters run to completion regardless of any
`request_stop()` calls. Cancellation is opt-in.

### Propagation

A cancelled `IoAwaiter` throws `CancelledError`. The exception walks
the `co_await` chain like any other exception — through inner Tasks,
through `spawn`'s runner, out of `co_await handle`. You can catch it
at any layer:

```cpp
Task<int> resilient(Executor& ex, StopToken tok) {
    try {
        co_return co_await fetch(ex, tok);
    } catch (const CancelledError&) {
        co_return fallback_value();
    }
}
```

### Lifetime

`StopToken` holds a `stl::shared_ptr` to shared state — it's safe to
copy, store, and outlive its `StopSource`. Once the source is gone,
the token reflects whatever the last state was (probably "not
requested").

### Idempotence

`StopSource::request_stop()` returns `true` if this call flipped the
flag, `false` if it was already set. Each callback fires exactly once.

### What doesn't get cancellation

- Plain `sync_wait(Task<T>)` — no executor, no event loop, no
  cancellation path.
- Detached tasks — there's no handle to cancel.
- Awaiters that don't take a token (`sleep_for(ex, dt)` overload, any
  user-written `IoAwaiter` that omits the token argument).

---

## Awaiter cleanup contract

Custom awaiters (anything that ends up as a local in a coroutine
body and gets `co_await`ed) must clean up any external state they
register during `await_suspend`, and they must do so from their
destructor — not just from `await_resume`.

This matters because a coroutine frame can be destroyed in several
ways:

1. **Natural completion** — the body runs to `co_return`, the
   awaiter's `await_resume` runs, then the awaiter's destructor
   runs as the frame unwinds.
2. **Cancellation via `StopSource`** — the awaiter's `await_resume`
   throws `CancelledError`. The frame unwinds, the awaiter
   destructor runs.
3. **Dropped `SpawnHandle` while pending** — the runner stays
   alive until it completes (either by I/O firing naturally or by
   you signalling a `StopSource`). The frame is *not* destroyed
   prematurely.
4. **`Task<T>` dropped without ever being awaited** — the body
   never runs, no awaiters are constructed. Clean.

Path (3) is the one that surprises people: dropping a `SpawnHandle`
is **not** cancellation. If the task is parked on I/O that never
fires (a connection that goes idle, a timer with no expiry, etc.),
the frame leaks. Use `StopSource` to actually interrupt it.

```cpp
StopSource src;
auto       h = spawn(ex, fetch(ex, src.token()));

// Drop the handle — task keeps running.
// Eventually you want it gone:
src.request_stop();   // <-- this is what actually cancels
```

If you're writing a custom awaiter, the pattern is:

```cpp
class my_awaiter {
public:
    bool await_ready() const noexcept { /* ... */ }

    void await_suspend(stl::coroutine_handle<> h) {
        // Register something external.
        m_token = external_thing::register(this);
        m_registered = true;
    }

    auto await_resume() {
        // Normal-path cleanup.
        if (m_registered) {
            external_thing::unregister(m_token);
            m_registered = false;
        }
        // Return the value.
    }

    ~my_awaiter() {
        // Frame-destruction cleanup. Same as await_resume's, but
        // also handles the case where the frame was destroyed
        // without await_resume running (e.g. a cancellation that
        // throws before await_resume gets to clean up).
        if (m_registered)
            external_thing::unregister(m_token);
    }

private:
    external_token m_token{};
    bool           m_registered = false;
};
```

`IoAwaiter` follows this pattern for the stop_callback list (via
`stop_callback_node`'s destructor) but currently *not* for the
reactor registration itself — the reactor is only deregistered from
`await_resume`. This is safe as long as the awaiter is only ever
destroyed via paths (1) or (2). The `SpawnHandle`-managed runner
patterns never destroy an awaiter mid-park (the runner stays alive
until it completes), so today's code is sound. If you write an
awaiter that registers with the reactor directly, add the
destructor cleanup yourself.

## Common pitfalls

### Lazy by default

Constructing a Task does not start its body:

```cpp
auto t = compute_heavy();   // nothing happens
do_other_work();
auto n = co_await t;        // body runs HERE — total = work + heavy
```

For overlap, use `spawn`:

```cpp
auto h = spawn(ex, compute_heavy());   // starts now
do_other_work();
auto n = co_await stl::move(h);        // total = max(work, heavy)
```

### By-value parameters across `co_await`

Function parameters are stored in the coroutine frame, but **references
are stored as references** — they don't get a copy of the referent.
If a coroutine takes `const Foo&` and suspends, and the caller's `Foo`
goes away during the suspension, the resumed coroutine has a dangling
reference.

```cpp
// DANGEROUS: req may not outlive the suspension.
Task<int> bad(const stl::string& req) {
    co_await sleep_for(ex, 100ms);
    co_return req.size();
}

// SAFE: req is moved into the coroutine frame.
Task<int> good(stl::string req) {
    co_await sleep_for(ex, 100ms);
    co_return req.size();
}
```

Rule of thumb: take parameters by value (or move) in coroutines that
suspend, unless the lifetime is guaranteed by the owner chain.

### Frame allocation cost

Each Task creates a heap-allocated coroutine frame (typically
100–300 bytes). For hot paths producing millions of Tasks, this can
be measurable. Optimizer-driven Heap Allocation Elision (HALO) only
fires when the Task's lifetime is fully visible to the compiler —
not when the Task crosses a type-erased boundary like
`stl::function` or a virtual call.

If you need to bring the cost down, override `operator new` on the
promise to allocate from an arena.

### Exception propagation

| Where it's thrown                | What happens                                                              |
|----------------------------------|---------------------------------------------------------------------------|
| Task body                        | Stored in promise. Rethrown at `co_await` / `sync_wait`.                  |
| `co_await some_task()`           | Rethrown at the `co_await` expression; `try/catch` catches normally.      |
| Cancelled `IoAwaiter`            | Throws `CancelledError`; same propagation as any other exception.         |
| Detached Task                    | Stored, never observed — discarded on frame destruction.                  |
| `when_all`                       | First exception wins; remaining tasks' results are discarded.             |
| Dropped `SpawnHandle` (pending)  | Stored in runner; discarded when runner self-destroys at `final_suspend`. |

### Coroutine identity across suspension

When a coroutine resumes, it's on the thread that called `resume()`.
Today everything is single-threaded so this is the same thread, but
when a multi-threaded executor lands, watch out for:

- `thread_local` variables — not the same instance.
- `std::this_thread::get_id()` — different return value.
- RAII guards on the caller's stack — gone.

### Single-shot SpawnHandle

`await_resume` moves out of the promise's `optional<T>`. Awaiting the
same handle twice returns a moved-from `T`. Treat each handle as
single-shot.

```cpp
auto h = spawn(ex, fetch(ex));
auto a = co_await stl::move(h);   // OK
auto b = co_await stl::move(h);   // h is empty; UB
```

### `sync_wait(SpawnHandle&&)` re-entrance

Don't call this from inside a coroutine the executor is currently
resuming. The executor is not re-entrant. From inside a coroutine,
`co_await stl::move(h)` is the right tool.

---

## Implementation status

| Item                                    | Status                                                      |
|-----------------------------------------|-------------------------------------------------------------|
| `Task<T>`, `sync_wait(Task<T>)`         | Shipped.                                                    |
| `when_all`                              | Shipped (sequential; concurrent rewrite later).             |
| `Executor`, `IoAwaiter`                 | Linux: shipped. Windows: depends on the IOCP reactor, which is implemented but unverified. |
| `Executor::run_until`                   | Shipped.                                                    |
| `sleep_for`                             | Linux: shipped. Windows: loopback socket + threadpool timer. |
| `spawn`, `SpawnHandle`                  | Shipped.                                                    |
| `sync_wait(SpawnHandle<T>&&)`           | Shipped.                                                    |
| `StopSource`, `StopToken`, `CancelledError` | Shipped.                                                |
| `when_any`                              | Deferred — needs concurrent execution to be meaningful.     |
