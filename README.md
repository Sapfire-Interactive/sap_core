# sap_core

Foundational C++20 systems library. Provides allocators, containers, concurrency primitives, and error handling as drop-in replacements for the standard library — with explicit control over memory layout, allocation strategy, and error propagation.

Part of the [Sapfire](https://github.com/Sapfire-Interactive) library ecosystem.

## What's inside

### Memory
- **`linear_arena`** — bump allocator over a fixed memory block. O(1) allocation, no individual deallocation, reset in a single pointer assignment. Tracks peak usage and call count for profiling.
- **`fixed_block_pool`** — free-list pool backed by a `linear_arena`. O(1) alloc and dealloc for fixed-size blocks; allocates chunks from the arena on demand. Ideal for node-based containers.
- **`linear_allocator<T>`** — STL-compatible adapter wrapping `linear_arena`. Deallocations are no-ops.
- **`pool_allocator<T>`** — STL-compatible adapter: single-element allocations go through a `fixed_block_pool`, multi-element through the backing arena.
- **`stack_allocator`** — stack-frame-style allocator with LIFO deallocation support.

### Containers
Custom implementations backed by the allocator infrastructure above:
`vector`, `map`, `unordered_map`, `string`, `fixed_string`, `unique_ptr`, `shared_ptr`

### Concurrency
- **`spsc_queue<T, Capacity>`** — lock-free single-producer/single-consumer ring buffer. Head and tail are on separate cache lines to prevent false sharing. Capacity must be a power of two for bitmask wrapping.
- **`job_system`** — thread pool with one SPSC queue per worker. Jobs are dispatched round-robin across workers. `wait_idle()` blocks until all submitted work completes.

### Generational handles
- **`generational_index_allocator`** — allocates and recycles `{index, generation}` pairs. Stale references are detected by generation mismatch rather than dangling pointers.
- **`generational_vector<T>`** — sparse storage indexed by generational handles. Returns `nullptr` for stale or out-of-bounds accesses.

### Error handling
- **`result<T, E>`** — discriminated union holding either a value or an error. Manual `union` storage with explicit placement new/destroy — no heap allocation, no exceptions. Ships with `RESULT_CHECK(f)` for early-return propagation.

### Utilities
`clock` (POSIX + Windows), `timer`, `timestamp`, `guid`, `log`, `serialization`, platform detection macros.

## Design notes

All allocators use explicit arena pointers rather than global state — no hidden singletons, no thread-local magic. The `linear_arena` is intentionally not thread-safe; ownership and lifetime are caller-managed.

The SPSC queue pads head and tail to 64-byte boundaries. Without this, a write to `m_head` by the producer would false-share a cache line with `m_tail` read by the consumer, causing unnecessary cache coherency traffic on multi-core hardware.

`result<T, E>` stores value and error in a `union` to avoid the overhead of `std::variant`'s type-index and alignment padding, and to keep the control flow explicit.

## Build

Requires CMake 3.20+ and a C++20-capable compiler (GCC 12+, Clang 15+, MSVC 19.34+).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

To use as a CMake dependency:
```cmake
find_package(sap_core REQUIRED)
target_link_libraries(your_target PRIVATE sap_core::sap_core)
```

## Usage example

```cpp
#include <sap_core/stl/arena.h>
#include <sap_core/stl/allocator.h>
#include <sap_core/stl/vector.h>
#include <sap_core/stl/result.h>

// Stack-allocated arena — no heap involvement
alignas(16) std::byte buf[4096];
stl::linear_arena arena{buf, sizeof(buf)};

// STL-compatible container backed by the arena
stl::vector<int, stl::linear_allocator<int>> v{stl::linear_allocator<int>{&arena}};
v.push_back(1);
v.push_back(2);

// Error handling without exceptions
stl::result<int> ok{stl::success, 42};
stl::result<int> fail{stl::error, "something went wrong"};

if (!fail)
    std::println("{}", fail.error());
```
