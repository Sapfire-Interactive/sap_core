# `sap::async`

C++20 coroutine primitives for sap_core. Header-only.

```cpp
#include <sap_core/async/task.h>
#include <sap_core/async/sync_wait.h>
#include <sap_core/async/when_all.h>
#include <sap_core/async/when_any.h>
```

## Quick reference

| Type / function | What it is |
|---|---|
| `sap::async::Task<T>` | The coroutine return type. Awaitable; can be detached. |
| `sap::async::sync_wait(Task<T>)` | Run a Task to completion on the calling thread. Returns `T`. |
| `sap::async::when_all(Task<T>...)` | Run multiple Tasks concurrently; returns `Task<tuple<T...>>`. |
| `sap::async::when_any(Task<T>...)` | Race multiple Tasks; returns `Task<variant<T...>>` with whichever finished first. |

---

## `Task<T>`

Coroutine return type. **Lazy**: the body doesn't execute until somebody
consumes the Task. The two ways to consume one are `co_await` (from another
coroutine) and `sync_wait` (from synchronous code). The third option is
`.detach()` — fire-and-forget, can't collect the result.

```cpp
sap::async::Task<int> compute() {
    co_return 42;
}

sap::async::Task<int> use_it() {
    int n = co_await compute();   // body runs here
    co_return n * 2;
}
```

### `.detach()`

Releases ownership of the coroutine frame and starts the body running. The
frame self-destroys on completion. Exceptions thrown inside a detached Task
have no awaiter to surface to — they're stored in the promise and discarded
on frame destruction. Use this for "I don't care about the result and
nothing else will await it" cases.

```cpp
auto bg = log_to_disk_async();  // returns Task<void>
bg.detach();                     // starts running; the Task object can be dropped
```

### Move-only

Coroutine handles can't be copied without double-freeing the frame. Always
move:

```cpp
sap::async::Task<int> t = compute();
auto t2 = std::move(t);   // OK
auto t3 = t;              // compile error
```

### `value_type`

`Task<T>::value_type` is `T`. Useful for combinators that need to extract
the value type from a Task at compile time.

---

## `sync_wait`

The top-level driver. Takes a Task, blocks the calling thread until the
Task finishes, returns its value (or rethrows its exception). Use this in
`main()`, in tests, and anywhere else synchronous code needs to consume an
async result.

```cpp
sap::async::Task<int> compute_async() { co_return 7; }

int main() {
    int n = sap::async::sync_wait(compute_async());
    return n;
}
```

If the Task throws, the exception is rethrown out of `sync_wait`. If the
Task is void, `sync_wait` returns void.

`sync_wait` blocks the thread. It is **not** an executor — it has no
event loop. A Task that suspends waiting on I/O won't be resumed by
`sync_wait` alone. For now, use it only for Tasks that complete without
external resumption (synchronous logic, computation, composed Tasks that
all reduce to synchronous work).

---

## `when_all`

Runs multiple Tasks concurrently and returns a `Task` that completes when
all of them have completed. The result is a `tuple` of their return values
in argument order.

```cpp
sap::async::Task<int>    fetch_user();
sap::async::Task<bool>   fetch_flag();
sap::async::Task<double> fetch_score();

auto [user, flag, score] = sap::async::sync_wait(
    sap::async::when_all(fetch_user(), fetch_flag(), fetch_score())
);
```

All input Tasks are started together; the awaiter resumes once every Task
has finished. Total wall-clock time is `max(t1, t2, t3)`, not their sum.

### Exceptions

If any Task throws, the exception is propagated out of `when_all` and the
remaining results are discarded.

### Restrictions

- Void Tasks are not supported (can't put `void` in a `tuple`).
- Tasks are consumed by value — pass `make_task()` directly or
  `std::move(task)` for named locals.

---

## `when_any`

Runs multiple Tasks concurrently and returns a `Task` that completes as
soon as **any one** of them finishes. The result is a `variant` whose
active index identifies the winner.

```cpp
auto data_task    = fetch_from_primary();
auto fallback     = fetch_from_replica();

auto result = sap::async::sync_wait(
    sap::async::when_any(std::move(data_task), std::move(fallback))
);

if (result.index() == 0) {
    use_primary(std::get<0>(result));
} else {
    use_replica(std::get<1>(result));
}
```

Use cases: deadline races (kick off real work alongside a `sleep` Task and
take whichever wins), redundant fetches (whichever upstream replies first),
"any of these resources" patterns.

### Exceptions

If the winning Task threw, the exception is rethrown out of `when_any`.
Losing Tasks' exceptions, if any, are discarded.

### Restrictions

Same as `when_all`: no void Tasks, by-value parameters.

---

## Common pitfalls

### Lazy by default

Constructing a Task does not start its body running:

```cpp
auto t = compute_heavy();   // nothing happens — no work, no I/O, no syscalls
do_other_work();
auto n = co_await t;        // NOW the body runs; you wait the full duration
```

The above runs serially: `do_other_work_time + compute_heavy_time`. If you
want overlap, the in-between work has to either be expressed as a Task
(then composed via `when_all`) or you have to explicitly start the Task
running while keeping the awaitable to collect later.

The "start now, await later" escape hatch is a `spawn`-style API and isn't
in this directory yet. For now: use `when_all` if both sides are async, or
restructure so the sync part comes after the await.

### By-value parameters across `co_await`

Function parameters are stored in the coroutine frame, but **references
are stored as references** — they don't get a copy of the referent. If a
coroutine takes `const Foo&` and suspends, and the caller's `Foo` goes
away during the suspension, the resumed coroutine has a dangling reference.

```cpp
// DANGEROUS: req may not outlive the suspension.
sap::async::Task<int> bad(const std::string& req) {
    co_await sleep_a_bit();
    co_return req.size();   // req might be gone
}

// SAFE: req is moved into the coroutine frame.
sap::async::Task<int> good(std::string req) {
    co_await sleep_a_bit();
    co_return req.size();
}
```

Rule of thumb: take parameters by value (or move) in coroutines that
suspend, unless the lifetime is guaranteed (e.g. the coroutine is itself
driven by an owner that outlives every suspension point).

### No default executor

`Task<T>` doesn't know how to run itself. There is no implicit "main loop"
that picks up freshly-constructed Tasks and runs them. You drive a Task
by:

1. `sync_wait` — blocks the calling thread until done.
2. `co_await` from another Task — passes drive responsibility upstream.
3. `.detach()` — releases ownership; the body runs but you can't collect
   the result.

This is intentional. C++ doesn't ship an executor — every framework picks
its own. The current state of this directory is "language-level primitives
only"; an executor lives elsewhere and will be added separately.

### Frame allocation cost

Each Task creates a heap-allocated coroutine frame (typically 100–300
bytes). For hot paths producing millions of Tasks, this can be measurable.
Optimizer-driven Heap Allocation Elision (HALO) sometimes erases the
allocation, but only when the Task's lifetime is fully visible to the
compiler — usually not the case when the Task crosses a type-erased
boundary like `std::function` or `Task` returned through a virtual method.

If you need to bring this cost down, the standard mitigation is a custom
`operator new` on the promise that allocates from an arena.

### Exception propagation

| Where it's thrown | What happens |
|---|---|
| Inside a Task body | Stored in the promise. Rethrown when consumed via `co_await` or `sync_wait`. |
| Inside `co_await some_task()` | Rethrown at the `co_await` expression; can be caught with `try/catch`. |
| Inside a detached Task | Stored, never observed — discarded on frame destruction. |
| Across `when_all`/`when_any` | First exception wins; remaining results / siblings are discarded. |

### Coroutine identity across suspension

When a coroutine resumes, it may be running on a different thread than the
one that constructed it. This matters for:

- `thread_local` variables — not the same instance.
- `std::this_thread::get_id()` — different return value.
- RAII guards on the caller's stack — gone (the caller already returned).

This isn't an issue today because there's no executor yet (everything runs
on the calling thread). It becomes important when a multi-threaded
executor lands. Worth knowing now to avoid baking in single-thread
assumptions.

---

## Implementation status

Available now: `Task<T>`, `sync_wait`, `when_all`.

`when_any` is described above but its implementation header is not yet in
the tree — it depends on an executor that doesn't exist yet to actually
race tasks. A sequential placeholder would be misleading (the name implies
a race that wouldn't happen), so the function is intentionally absent
until it can do what its name claims.

`when_all` is shipped as a sequential implementation: tasks are awaited in
argument order rather than truly concurrently. The contract — the returned
tuple, exception propagation, ordering — is identical to the concurrent
version, so user code is forward-compatible. The wall-clock improvement
arrives when the executor does.
